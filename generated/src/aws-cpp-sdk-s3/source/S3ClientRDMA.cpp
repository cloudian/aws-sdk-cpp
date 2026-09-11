/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "aws/s3/S3Client.h"

#include "RdmaObj.h"
#include "NativeIoUtils.h"
#include "ChecksumUtils.h"

#include <aws/core/utils/logging/LogMacros.h>
#include "aws/s3/model/AbortMultipartUploadRequest.h"
#include "aws/s3/model/CompletedPart.h"
#include "aws/s3/model/CompleteMultipartUploadRequest.h"
#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/PutObjectRDMARequest.h"
#include "aws/s3/model/UploadPartRequest.h"
#include "aws/s3/S3Client.h"
#include "aws/s3/S3Errors.h"

#include <charconv>
#include <iostream>


namespace Aws
{
namespace S3
{
    static const char CUOBJ_S3_CLIENT_LOG_TAG[] = "CuObjS3Client";

    EnvConfigVariable::EnvConfigVariable(const char* name, size_t default_value) {
        auto env = std::getenv(name);
        if (env != nullptr) {
            char* endptr;
            unsigned long long parsed_value = strtoull(env, &endptr, 10);
            if (*endptr == '\0') {
                value_ = static_cast<size_t>(parsed_value);
                return;
            }
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "Invalid " << name << " value: " << env);
        }
        // Default
        value_ = default_value;
    }

    S3ClientRDMA::S3ClientRDMA() : pool_(),
        maxConcurrentReads_("S3RDMA_CONCURRENT_READS", 4),
        rdmaThreshold_("S3RDMA_THRESHOLD_BYTES", 1024 * 1024),
        mpuThreshold_("S3RDMA_MPU_THRESHOLD_BYTES", 100 * 1024 * 1024), // 100MiB
        maxConcurrentMPUUploads_("S3RDMA_MAX_CONCURRENT_MPU_UPLOADS", 4),
        concurrentMPUUploadPartSize_("S3RDMA_CONCURRENT_MPU_UPLOAD_PART_SIZE_MIB", 10)
    {
        concurrentMPUUploadPartSize_.set(concurrentMPUUploadPartSize_.get() << 20); // Convert MiB to bytes
        CheckRdmaStatus();
    }


    void S3ClientRDMA::CheckRdmaStatus()
    {
        if (RdmaObj::rdma_status) {
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, RdmaObj::rdma_status);
        } else if (RdmaObj::native_io) {
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "RDMA has been disabled");
        } else {
            AWS_LOGSTREAM_INFO(CUOBJ_S3_CLIENT_LOG_TAG, "RDMA enabled");
        }
    }

    bool S3ClientRDMA::IsRDMAEnabled() {
        return !RdmaObj::native_io;
    }

    void S3ClientRDMA::EnableRDMA(bool enabled) {
        RdmaObj::native_io = !enabled;
    }

    size_t body_size(const std::shared_ptr<Aws::IOStream> &body) {
        if (!body) {
            return 0;
        }
        body->seekg(0, std::ios::end);
        size_t size = body->tellg();
        body->seekg(0, std::ios::beg);
        return size;
    }

    // Transfer body into rdma buffer if it fits
    bool S3ClientRDMA::bodyToRDMABuffer(const std::shared_ptr<Aws::IOStream> &body, RdmaBuffer& buf) const
    {
        // Don't use RDMA if put has no body
        if (!body) {
            AWS_LOGSTREAM_TRACE(CUOBJ_S3_CLIENT_LOG_TAG, "bodyToRDMABuffer: No body");
            return false;
        }

        // Get the size of the body
        size_t size = body_size(body);

        if (size > get_rdma_buffer_size()) {
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "bodyToRDMABuffer: body is too large for RDMA (body=" << size << "B, max rdma size=" << get_rdma_buffer_size() << "B");
            return false;
        }
        if (size < rdmaThreshold_.get()) {
            return false;
        }

        // Create a buffer to hold the data
        AWS_LOGSTREAM_TRACE(CUOBJ_S3_CLIENT_LOG_TAG, "bodyToRDMABuffer: transferring body to rdma buffer, size=" << size);
        buf = std::move(const_cast<S3ClientRDMA*>(this)->get_rdma_buffer(size));
        if (!buf.ptr()) {
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "bodyToRDMABuffer: failed to allocate RDMA buffer, falling back to TCP");
            return false;
        }
        body->read(buf.ptr().data(), size);
        return true;
    }


    //=====================================================================
    // GetObject support
    //=====================================================================

    Model::GetObjectOutcome S3ClientRDMA::GetObject(const Model::GetObjectRequest &request) const
    {
        // If we're not using cuobj, we can't do rdma
        // If we're getting a specific part we can't use rdma (not currently supported by HyperStore)
        if (IsTransparentRDMADisabled() || !IsRDMAEnabled() || request.PartNumberHasBeenSet())
        {
            auto s3client = dynamic_cast<const S3Client*>(this);
            return s3client->GetObjectTCP(request);
        }

        bool rangeSupplied = request.RangeHasBeenSet();

        // Get as much as will fit into an rdma buffer
        auto rdmaReq = Model::GetObjectRDMARequest(request);
        auto buf = const_cast<S3ClientRDMA*>(this)->get_rdma_buffer();

        auto outcome = this->GetObjectRDMA(rdmaReq, buf.ptr());
        if (!outcome.IsSuccess()) {
            return outcome;
        }

        // Determine the actual served range from the response's Content-Range header.
        // GetObjectRDMA() always sets a Range on the request (either the caller's own,
        // or a default one bounding it to the rdma buffer), so a range response is
        // always expected here. S3 clips a requested range that extends past the
        // object's end (e.g. a request for bytes=100-999 against a 200 byte object is
        // served as "bytes 100-199/200") - the transfer size must come from what was
        // actually served, not be assumed to match what was requested, or follow-up
        // reads would be scheduled past the object's end and rejected by S3.
        auto range_str = outcome.GetResult().GetContentRange();
        if (range_str.empty()) {
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::MISSING_PARAMETER, "MISSING_PARAMETER", "Response does not contain a ContentRange header", false);
            return Model::GetObjectOutcome(err);
        }
        size_t range_begin = 0, served_end = 0, complete_size = 0;
        int nparsed = std::sscanf(range_str.c_str(), "bytes %zu-%zu/%zu", &range_begin, &served_end, &complete_size);
        if (nparsed != 3) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "GetObject: failed to parse ContentRange=" << range_str);
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INVALID_PARAMETER_VALUE, "INVALID_PARAMETER_VALUE", "Response does not contain a valid ContentRange header", false);
            return Model::GetObjectOutcome(err);
        }

        // With no range requested we want the whole object, whose size is the
        // Content-Range denominator; with a range requested we only want what was
        // actually served (range_begin..served_end), which may be less than asked for.
        size_t total = rangeSupplied ? (served_end - range_begin + 1) : complete_size;
        AWS_LOGSTREAM_DEBUG(CUOBJ_S3_CLIENT_LOG_TAG, "GetObject: parsed ContentRange=" << range_str << ", total=" << total);

        auto transferred = static_cast<size_t>(outcome.GetResult().GetRDMABytesTransferred());
        AWS_LOGSTREAM_TRACE(CUOBJ_S3_CLIENT_LOG_TAG, "GetObject: RDMA bytes transferred=" << transferred);

        // x-amz-rdma-bytes-transferred is server-reported and not otherwise validated -
        // bound it to buf's actual allocated size, so a corrupted/malicious value can't
        // drive an out-of-bounds read of buf in the body->write() below.
        if (transferred > buf.size()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "GetObject: RDMA bytes transferred=" << transferred << " exceeds rdma buffer size=" << buf.size());
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INVALID_PARAMETER_VALUE, "INVALID_PARAMETER_VALUE", "x-amz-rdma-bytes-transferred exceeds the rdma buffer size", false);
            return Model::GetObjectOutcome(err);
        }

        // Use stream factory to create the response body stream
        auto body = std::unique_ptr<Aws::IOStream>(request.GetResponseStreamFactory()());
        body->write(buf.data(), transferred);

        if (transferred < total) {
            // We'll need to read the remaining data using additional reads - ensure we read from the same object
            if (!outcome.GetResult().GetVersionId().empty()) {
                rdmaReq.SetVersionId(outcome.GetResult().GetVersionId());
            } else {
                rdmaReq.SetIfMatch(outcome.GetResult().GetETag());
            }

            auto begin = range_begin + transferred;
            auto end = range_begin + total;

            auto futures = std::deque< Model::GetObjectRDMAOutcomeAndPtrCallable >();
            auto buffers = std::deque<RdmaBuffer>();
            size_t in_flight = 0;

            // Read the remaining data in chunks that fit into our rdma buffer
            while (begin != end) {
                auto to_read = std::min(end - begin, buf.size());
                std::ostringstream range;
                range << "bytes=" << begin << "-" << begin + to_read - 1;
                rdmaReq.SetRange(range.str());

                // Submit read
                AWS_LOGSTREAM_TRACE(CUOBJ_S3_CLIENT_LOG_TAG, "GetObject: submitting read for range " << range.str());
                auto chunk_buf = const_cast<S3ClientRDMA*>(this)->get_rdma_buffer();
                auto ptr = chunk_buf.ptr().slice(0, to_read);
                futures.push_back(this->GetObjectRDMAPtrCallable(rdmaReq, std::move(ptr)));
                buffers.push_back(std::move(chunk_buf));
                in_flight++;
                begin += to_read;

                if (in_flight == maxConcurrentReads_.get()) {
                    // Wait for the first read to complete
                    auto res = futures.front().get();
                    futures.pop_front();
                    auto& rsp = res.first;
                    auto& ptr = res.second;

                    if (!rsp.IsSuccess()) {
                        // Wait for in flight reads to complete before we return the failure
                        // (queue destructors will return buffers to pool and clean up futures)
                        for (auto& future : futures) {
                            future.get();
                        }
                        return std::move(rsp);
                    }

                    body->write(ptr.data(), ptr.size());

                    in_flight--;
                    buffers.pop_front();
                }
            }

            // Wait for remaining reads to complete
            while (!futures.empty()) {
                auto res = futures.front().get();
                futures.pop_front();
                auto& rsp = res.first;
                auto& ptr = res.second;

                if (!rsp.IsSuccess()) {
                    // Wait for in flight reads to complete before we return the failure
                    // (queue destructors will return buffers to pool and clean up futures)
                    for (auto& future : futures) {
                        future.get();
                    }
                    return std::move(rsp);
                }

                body->write(ptr.data(), ptr.size());
                buffers.pop_front();
            }
        }

        outcome.GetResult().SetContentLength(total);
        outcome.GetResult().ReplaceBody(body.release());

        return outcome;
    }

    Model::GetObjectOutcomeCallable S3ClientRDMA::GetObjectCallable(const Model::GetObjectRequest& request) const
    {
        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< Model::GetObjectOutcome() > >(s3client->GetAllocationTag(), [s3client, request](){ return s3client->GetObject(request); } );
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();
    }

    void S3ClientRDMA::GetObjectAsync(const Model::GetObjectRequest& request, const GetObjectResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const {
        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, request, handler, context]()
            {
                handler(s3client, request, s3client->GetObject(request), context);
            } );
    }

    Model::GetObjectRDMAOutcome S3ClientRDMA::GetObjectRDMA(Model::GetObjectRDMARequest &request, const RdmaPtr &dst) const
    {
        if (!dst) {
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "Destination pointer is null - did an RDMA buffer registration fail?", false);
            return Model::GetObjectOutcome(err);
        }

        if (dst.size() == 0) {
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INVALID_PARAMETER_VALUE, "INVALID_PARAMETER_VALUE", "Destination buffer has zero length", false);
            return Model::GetObjectOutcome(err);
        }

        if (!request.RangeHasBeenSet()) {
            // Request a range that covers the buffer.
            std::ostringstream range;
            range << "bytes=0-" << (dst.size() - 1);
            request.SetRange(range.str());
        }

        auto s3client = dynamic_cast<const S3Client*>(this);

        if (!IsRDMAEnabled() && !dst.is_system_memory()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "GetObjectRDMA: RDMA is disabled and the destination is not system memory");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is disabled and the destination is not system memory", false);
            return Model::GetObjectOutcome(err);
        }

        // NOTE: is_system_memory() queries the real memory type directly via the
        // cuobjclient library, so it stays accurate even when RDMA is disabled (e.g.
        // after a decline flips native_io) - see RdmaPtr::is_system_memory(). The one
        // case it can't detect is the library never having loaded at all, in which case
        // it assumes system memory. If that's a possibility in your environment, verify
        // IsRDMAEnabled() before calling GetObjectRDMA with a GPU-resident `dst`.
        //
        // NOTE: Even if RDMA is enabled, the server may choose to return data over TCP.
        // Set the stream factory so if data is returned over TCP it is streamed 
        // directly into the destination buffer.
        if (!IsRDMAEnabled() || dst.is_system_memory()) {            
            auto stream_factory = [&dst]() {
                auto stream = new Aws::StringStream();
                stream->rdbuf()->pubsetbuf(dst.data(), dst.size());
                return stream;
            };
            request.SetResponseStreamFactory(stream_factory);
        }

        // The native IO fallback below reads into dst via a host-memory
        // stream buffer, so it can only be used for system memory - non-
        // system (e.g. GPU) memory must always go through RDMA regardless
        // of the threshold.
        if (!IsRDMAEnabled() || (dst.size() < rdmaThreshold_.get() && dst.is_system_memory()))
        {
            // Below the threshold - don't even propose RDMA, just read directly into the
            // supplied buffer using plain TCP.
            auto rsp = s3client->GetObjectTCP(request);
            if (rsp.IsSuccess())
            {
                rsp.GetResult().SetRDMABytesTransferred(size_t(rsp.GetResult().GetContentLength()));
            }
            return rsp;
        }

        if (!RdmaObj::local_rdmaobj) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "RDMA is enabled but this thread has no RDMA client");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is enabled but this thread has no RDMA client", false);
            return Model::GetObjectOutcome(err);
        }

        auto token = RdmaObj::local_rdmaobj->getRDMAToken(dst.registered_data(), dst.size(), dst.offset(), CUOBJ_GET);
        if (token.empty())
        {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "cuMemObjGetRDMAToken failed");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "cuMemObjGetRDMAToken failed", false);
            return Model::GetObjectOutcome(err);
        }
        request.SetRDMAToken(token);

        auto rsp = s3client->GetObjectTCP(request);
        if (!rsp.IsSuccess()) {
            return rsp;
        }

        // Result shapes don't expose a public RDMAReplyHasBeenSet(), but an absent header
        // leaves GetRDMAReply() at its default of 0, which isn't one of the success codes -
        // so it's handled the same as an explicit decline.
        int rdma_reply = rsp.GetResult().GetRDMAReply();
        bool rdma_accepted = (rdma_reply == 200 || rdma_reply == 204 || rdma_reply == 206);

        if (rdma_accepted) {
            auto data_received_handler = request.GetDataReceivedEventHandler();
            if (data_received_handler)
            {
                // Invoke data received callback with the amount of data transferred
                auto transferred = static_cast<long long>(rsp.GetResult().GetRDMABytesTransferred());
                data_received_handler(nullptr, nullptr, transferred);
            }
        } else {
            // Server declined RDMA, and will have sent the contents over TCP.
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "GetObjectRDMA: server declined RDMA (x-amz-rdma-reply=" << rsp.GetResult().GetRDMAReply() << "), data was sent over TCP");
            EnableRDMA(false);

            if (!dst.is_system_memory()) {
                AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "GetObjectRDMA: server declined RDMA for a non-system-memory destination - unable to receive the HTTP fallback response");
                auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA was declined by the server for a non-system-memory destination", false);
                return Model::GetObjectOutcome(err);
            }

            rsp.GetResult().SetRDMABytesTransferred(size_t(rsp.GetResult().GetContentLength()));
        }

        return rsp;
    }

    Model::GetObjectRDMAOutcomeAndPtrCallable S3ClientRDMA::GetObjectRDMAPtrCallable(Model::GetObjectRDMARequest& request, RdmaPtr &&dst) const
    {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto dst_heap = new RdmaPtr(std::move(dst));

        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< std::pair<Model::GetObjectRDMAOutcome, RdmaPtr>() >>(s3client->GetAllocationTag(), [s3client, dst_heap, request]() mutable {
            auto dst = std::move(*dst_heap);
            delete dst_heap;
            auto outcome = s3client->GetObjectRDMA(request, dst);
            return std::make_pair(std::move(outcome), std::move(dst));
        });
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();
    }


    void S3ClientRDMA::GetObjectRDMAAsync(Model::GetObjectRDMARequest &request, RdmaPtr &&dst, const GetObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context) const
    {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto dst_heap = new RdmaPtr(std::move(dst));

        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, dst_heap, request, handler, context]() mutable
        {
            auto dst = std::move(*dst_heap);
            delete dst_heap;
            auto outcome = s3client->GetObjectRDMA(request, dst);
            handler(s3client, request, std::move(dst), std::move(outcome), context);
        } );
    }


    //=====================================================================
    // PutObject support
    //=====================================================================

    Model::PutObjectOutcome S3ClientRDMA::PutObject(const Model::PutObjectRequest &request) const
    {
        auto s3client = dynamic_cast<const S3Client*>(this);
        if (IsTransparentRDMADisabled() || !IsRDMAEnabled())
        {
            return s3client->PutObjectTCP(request);
        }

        // Check if body exists and get its size
        if (!request.GetBody()) {
            return s3client->PutObjectTCP(request);
        }

        auto mpu_threshold = mpuThreshold_.get();
        if (mpu_threshold != -1ULL) {
            size_t bodySize = body_size(request.GetBody());
            if (bodySize > mpu_threshold) {
                // Use multipart upload for large bodies
                return this->uploadUsingMPUFromStream(request);
            }
        }

        RdmaBuffer buf;
        if (!bodyToRDMABuffer(request.GetBody(), buf))
        {
            // Couldn't transfer body to rdma buffer, fallback to tcp
            return s3client->PutObjectTCP(request);
        }

        auto req = nativeio::toRDMARequest(request);
        auto ptr = buf.ptr();
        auto rsp = this->PutObjectRDMA(req, ptr);
        return nativeio::toNativeResponse(rsp);
    }

    Model::PutObjectOutcomeCallable S3ClientRDMA::PutObjectCallable(const Model::PutObjectRequest& request) const
    {
        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< Model::PutObjectOutcome() > >(s3client->GetAllocationTag(), [s3client, request](){ return s3client->PutObject(request); } );
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();
    }

    void S3ClientRDMA::PutObjectAsync(const Model::PutObjectRequest& request, const PutObjectResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const {
        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, request, handler, context]()
            {
                handler(s3client, request, s3client->PutObject(request), context);
            }) ;
    }

    Model::PutObjectRDMAOutcome S3ClientRDMA::PutObjectRDMA(Model::PutObjectRDMARequest &request, const RdmaPtr &src) const
    {
        if (!src) {
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "Source pointer is null - did an RDMA buffer registration fail?", false);
            return Model::PutObjectRDMAOutcome(err);
        }

        auto s3client = dynamic_cast<const S3Client*>(this);

        if (!IsRDMAEnabled() && !src.is_system_memory()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "PutObjectRDMA: RDMA is disabled and the source is not system memory");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is disabled and the source is not system memory", false);
            return Model::PutObjectRDMAOutcome(err);
        }

        if (src.size() > mpuThreshold_.get()) {
            // Use multipart upload for large bodies
            return this->uploadUsingMPU(request, src);
        }

        // The native IO fallback below sends src via a host-memory stream
        // buffer, so it can only be used for system memory - non-system
        // (e.g. GPU) memory must always go through RDMA regardless of the
        // threshold.
        if (!IsRDMAEnabled() || (src.size() < rdmaThreshold_.get() && src.is_system_memory()))
        {
            auto req = nativeio::toNativeRequest(request);
            const std::shared_ptr<Aws::IOStream> body = Aws::MakeShared<Aws::StringStream>("");
            body->rdbuf()->pubsetbuf(src.data(), src.size());
            req.SetBody(body);
            auto rsp = s3client->PutObjectTCP(req);
            return nativeio::toRDMAResponse(rsp);
        }

        checksum::calculate(request, src);

        if (!RdmaObj::local_rdmaobj) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "RDMA is enabled but this thread has no RDMA client");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is enabled but this thread has no RDMA client", false);
            return Model::PutObjectRDMAOutcome(err);
        }

        auto token = RdmaObj::local_rdmaobj->getRDMAToken(src.registered_data(), src.size(), src.offset(), CUOBJ_PUT);
        if (token.empty())
        {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "cuMemObjGetRDMAToken failed");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "cuMemObjGetRDMAToken failed", false);
            return Model::PutObjectRDMAOutcome(err);
        }

        request.SetRDMAToken(token);
        auto rsp = s3client->PutObjectRDMA(request);

        if (!rsp.IsSuccess()) {
            return rsp;
        }

        if (rsp.GetResult().GetRDMAReply() != 200)
        {
            // The HTTP request succeeded but the server declined RDMA for this PUT
            // (x-amz-rdma-reply != 200). Per the RDMA protocol, a PUT decline means the
            // server never received any data (the request body was empty), so the object
            // was not stored - the whole upload must be retried over plain TCP with the
            // real body.
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "PutObjectRDMA: server declined RDMA (x-amz-rdma-reply=" << rsp.GetResult().GetRDMAReply() << "), retrying over TCP");
            EnableRDMA(false);

            if (!src.is_system_memory()) {
                AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "PutObjectRDMA: server declined RDMA for a non-system-memory source - unable to retry over TCP");
                auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA was declined by the server for a non-system-memory source", false);
                return Model::PutObjectRDMAOutcome(err);
            }

            auto req = nativeio::toNativeRequest(request);
            const std::shared_ptr<Aws::IOStream> body = Aws::MakeShared<Aws::StringStream>("");
            body->rdbuf()->pubsetbuf(src.data(), src.size());
            req.SetBody(body);
            auto tcpRsp = s3client->PutObjectTCP(req);
            return nativeio::toRDMAResponse(tcpRsp);
        }

        auto data_sent_handler = request.GetDataSentEventHandler();
        if (data_sent_handler)
        {
            // Invoke data sent callback with the amount of data transferred
            data_sent_handler(nullptr, static_cast<long long>(src.size()));
        }

        return rsp;
    }

    Model::PutObjectRDMAOutcomeAndPtrCallable S3ClientRDMA::PutObjectRDMAPtrCallable(Model::PutObjectRDMARequest &request, RdmaPtr &&dst) const {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto dst_heap = new RdmaPtr(std::move(dst));

        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< std::pair<Model::PutObjectRDMAOutcome, RdmaPtr>() >>(s3client->GetAllocationTag(), [s3client, dst_heap, request]() mutable{
            auto dst = std::move(*dst_heap);
            delete dst_heap;
            auto outcome = s3client->PutObjectRDMA(request, dst);
            return std::make_pair(std::move(outcome), std::move(dst));
        });
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();

    }

    void S3ClientRDMA::PutObjectRDMAAsync(Model::PutObjectRDMARequest &request, RdmaPtr &&src, const PutObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context) const
    {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto src_heap = new RdmaPtr(std::move(src));

        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, src_heap, request, handler, context]() mutable
        {
            auto src = std::move(*src_heap);
            delete src_heap;
            auto outcome = s3client->PutObjectRDMA(request, src);
            handler(s3client, request, std::move(src), std::move(outcome), context);
        } );
    }

    Model::PutObjectRDMAOutcome S3ClientRDMA::uploadUsingMPU(Model::PutObjectRDMARequest &request, const RdmaPtr &src) const {
        (void) request;
        (void) src;

        // Start by creating an MPU upload
        auto s3client = dynamic_cast<const S3Client*>(this);
        auto createMPUReq = nativeio::toCreateMPURequest(request);
        auto createMPURsp = s3client->CreateMultipartUpload(createMPUReq);

        if (!createMPURsp.IsSuccess()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPU: CreateMultipartUpload failed: " << createMPURsp.GetError().GetMessage());
            return Model::PutObjectRDMAOutcome(createMPURsp.GetError());
        }

        // Upload parts in parallel
        auto uploadId = createMPURsp.GetResult().GetUploadId();
        auto partSize = concurrentMPUUploadPartSize_.get();
        auto totalSize = src.size();
        auto numParts = (totalSize + partSize - 1) / partSize;
        auto maxConcurrentUploads = maxConcurrentMPUUploads_.get();

        std::vector<Model::UploadPartRDMAOutcomeAndPtrCallable> partFutures;
        for (size_t partNumber = 1; partNumber <= numParts; partNumber++) {
            auto offset = (partNumber - 1) * partSize;
            auto size = std::min(partSize, totalSize - offset);

            Model::UploadPartRDMARequest uploadPartReq;
            uploadPartReq.SetBucket(request.GetBucket());
            uploadPartReq.SetKey(request.GetKey());
            uploadPartReq.SetUploadId(uploadId);
            uploadPartReq.SetPartNumber(partNumber);
            uploadPartReq.SetDataSentEventHandler(request.GetDataSentEventHandler());

            auto ptr = src.slice(offset, size);
            auto fut = this->UploadPartRDMAPtrCallable(uploadPartReq, std::move(ptr));
            partFutures.emplace_back(std::move(fut));

            if (partFutures.size() > maxConcurrentUploads) {
                partFutures[partFutures.size() - maxConcurrentUploads - 1].wait();
            }
        }

        // Collect results
        std::vector<Model::CompletedPart> completedParts;
        for (size_t i = 0; i < partFutures.size(); i++) {
            auto res = partFutures[i].get();
            auto& uploadPartRsp = res.first;
            auto partNumber = i + 1;

            if (!uploadPartRsp.IsSuccess()) {
                AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPU: UploadPart failed for part " << partNumber << ": " << uploadPartRsp.GetError().GetMessage());
                // Abort the MPU
                Model::AbortMultipartUploadRequest abortReq;
                abortReq.SetBucket(request.GetBucket());
                abortReq.SetKey(request.GetKey());
                abortReq.SetUploadId(uploadId);
                s3client->AbortMultipartUpload(abortReq);
                return Model::PutObjectRDMAOutcome(uploadPartRsp.GetError());
            }

            Model::CompletedPart completedPart;
            completedPart.SetETag(uploadPartRsp.GetResult().GetETag());
            completedPart.SetPartNumber(partNumber);
            completedParts.push_back(std::move(completedPart));
        }

        // Complete the MPU
        Model::CompleteMultipartUploadRequest completeReq;
        completeReq.SetBucket(request.GetBucket());
        completeReq.SetKey(request.GetKey());
        completeReq.SetUploadId(uploadId);
        Model::CompletedMultipartUpload completedUpload;
        completedUpload.SetParts(std::move(completedParts));
        completeReq.SetMultipartUpload(std::move(completedUpload));
        auto completeRsp = s3client->CompleteMultipartUpload(completeReq);
        if (!completeRsp.IsSuccess()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPU: CompleteMultipartUpload failed: " << completeRsp.GetError().GetMessage());
            return Model::PutObjectRDMAOutcome(completeRsp.GetError());
        }
        return nativeio::toRDMAResponse(completeRsp);
    }

    Model::PutObjectOutcome S3ClientRDMA::uploadUsingMPUFromStream(const Model::PutObjectRequest &request) const {
        auto s3client = dynamic_cast<const S3Client*>(this);

        // Create multipart upload request from standard PutObject request
        auto createMPUReq = nativeio::toCreateMPURequest(request);
        auto createMPURsp = s3client->CreateMultipartUpload(createMPUReq);
        if (!createMPURsp.IsSuccess()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPUFromStream: CreateMultipartUpload failed: " << createMPURsp.GetError().GetMessage());
            return Model::PutObjectOutcome(createMPURsp.GetError());
        }

        auto uploadId = createMPURsp.GetResult().GetUploadId();
        auto partSize = concurrentMPUUploadPartSize_.get();
        auto totalSize = body_size(request.GetBody());
        auto numParts = (totalSize + partSize - 1) / partSize;

        auto buffers = std::deque<RdmaBuffer>();
        auto partFutures = std::deque<Model::UploadPartRDMAOutcomeAndPtrCallable>();
        auto maxConcurrentUploads = maxConcurrentMPUUploads_.get();

        // Upload parts sequentially to avoid complex stream management
        for (size_t partNumber = 1; partNumber <= numParts; partNumber++) {
            auto offset = (partNumber - 1) * partSize;
            auto size = std::min(partSize, totalSize - offset);

            // Create a buffer for this part
            RdmaBuffer buf = const_cast<S3ClientRDMA*>(this)->get_rdma_buffer(size);

            // Read part data into buffer
            request.GetBody()->seekg(offset, std::ios::beg);
            request.GetBody()->read(buf.ptr().data(), size);

            // Create upload part request
            Model::UploadPartRDMARequest uploadPartReq;
            uploadPartReq.SetBucket(request.GetBucket());
            uploadPartReq.SetKey(request.GetKey());
            uploadPartReq.SetUploadId(uploadId);
            uploadPartReq.SetPartNumber(partNumber);
            // Note: DataSentEventHandler is typically not available on PutObjectRequest

            // Upload the part
            partFutures.push_back(this->UploadPartRDMAPtrCallable(uploadPartReq, buf.ptr()));
            buffers.push_back(std::move(buf));
            if (buffers.size() > maxConcurrentUploads) {
                // Wait for the first part to complete before uploading the next one
                partFutures[partFutures.size() - maxConcurrentUploads - 1].wait();
                // We can now safely return the buffer to the pool since the upload is complete
                buffers.pop_front();
            }
        }

        auto completedParts = std::vector<Model::CompletedPart>();
        for (size_t partNumber = 1; partNumber <= numParts; partNumber++) {
            auto uploadPartRsp = partFutures[partNumber - 1].get().first;
            if (!uploadPartRsp.IsSuccess()) {
                AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPUFromStream: UploadPart failed for part " << partNumber << ": " << uploadPartRsp.GetError().GetMessage());
                // Abort the MPU
                Model::AbortMultipartUploadRequest abortReq;
                abortReq.SetBucket(request.GetBucket());
                abortReq.SetKey(request.GetKey());
                abortReq.SetUploadId(uploadId);
                s3client->AbortMultipartUpload(abortReq);
                return Model::PutObjectOutcome(uploadPartRsp.GetError());
            }

            Model::CompletedPart completedPart;
            completedPart.SetETag(uploadPartRsp.GetResult().GetETag());
            completedPart.SetPartNumber(partNumber);
            completedParts.push_back(std::move(completedPart));
        }

        // Complete the MPU
        Model::CompleteMultipartUploadRequest completeReq;
        completeReq.SetBucket(request.GetBucket());
        completeReq.SetKey(request.GetKey());
        completeReq.SetUploadId(uploadId);
        Model::CompletedMultipartUpload completedUpload;
        completedUpload.SetParts(std::move(completedParts));
        completeReq.SetMultipartUpload(std::move(completedUpload));

        auto completeRsp = s3client->CompleteMultipartUpload(completeReq);
        if (!completeRsp.IsSuccess()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "uploadUsingMPUFromStream: CompleteMultipartUpload failed: " << completeRsp.GetError().GetMessage());
            return Model::PutObjectOutcome(completeRsp.GetError());
        }

        // Convert CompleteMultipartUpload result to PutObject result
        Model::PutObjectResult result;
        result.SetETag(completeRsp.GetResult().GetETag());
        // Note: VersionId access is restricted in CompleteMultipartUploadResult

        return Model::PutObjectOutcome(std::move(result));
    }

    //=====================================================================
    // UploadPart support
    //=====================================================================

    Model::UploadPartOutcome S3ClientRDMA::UploadPart(const Model::UploadPartRequest &request) const
    {
        auto s3client = dynamic_cast<const S3Client*>(this);
        if (IsTransparentRDMADisabled() || !IsRDMAEnabled())
        {
            return s3client->UploadPartTCP(request);
        }

        RdmaBuffer buf;
        if (!bodyToRDMABuffer(request.GetBody(), buf))
        {
            // Couldn't transfer body to rdma buffer, fallback to tcp
            return s3client->UploadPartTCP(request);
        }

        auto req = nativeio::toRDMARequest(request);
        auto ptr = buf.ptr();
        auto rsp = this->UploadPartRDMA(req, ptr);
        return nativeio::toNativeResponse(rsp);
    }

    Model::UploadPartOutcomeCallable S3ClientRDMA::UploadPartCallable(const Model::UploadPartRequest& request) const
    {
        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< Model::UploadPartOutcome() > >(s3client->GetAllocationTag(), [s3client, request](){ return s3client->UploadPart(request); } );
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();
    }

    void S3ClientRDMA::UploadPartAsync(const Model::UploadPartRequest& request, const UploadPartResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) const {
        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, request, handler, context]()
            {
                handler(s3client, request, s3client->UploadPart(request), context);
            }) ;
    }

    Model::UploadPartRDMAOutcome S3ClientRDMA::UploadPartRDMA(Model::UploadPartRDMARequest &request, const RdmaPtr &src) const
    {
        if (!src) {
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "Source pointer is null - did an RDMA buffer registration fail?", false);
            return Model::UploadPartRDMAOutcome(err);
        }

        auto s3client = dynamic_cast<const S3Client*>(this);

        if (!IsRDMAEnabled() && !src.is_system_memory()) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "UploadPartRDMA: RDMA is disabled and the source is not system memory");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is disabled and the source is not system memory", false);
            return Model::UploadPartRDMAOutcome(err);
        }

        // The native IO fallback below sends src via a host-memory stream
        // buffer, so it can only be used for system memory - non-system
        // (e.g. GPU) memory must always go through RDMA regardless of the
        // threshold.
        if (!IsRDMAEnabled() || (src.size() < rdmaThreshold_.get() && src.is_system_memory()))
        {
            auto req = nativeio::toNativeRequest(request);
            const std::shared_ptr<Aws::IOStream> body = Aws::MakeShared<Aws::StringStream>("");
            body->rdbuf()->pubsetbuf(src.data(), src.size());
            req.SetBody(body);
            auto rsp = s3client->UploadPartTCP(req);
            return nativeio::toRDMAResponse(rsp);
        }

        checksum::calculate(request, src);

        if (!RdmaObj::local_rdmaobj) {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "RDMA is enabled but this thread has no RDMA client");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA is enabled but this thread has no RDMA client", false);
            return Model::UploadPartRDMAOutcome(err);
        }

        auto token = RdmaObj::local_rdmaobj->getRDMAToken(src.registered_data(), src.size(), src.offset(), CUOBJ_PUT);
        if (token.empty())
        {
            AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "cuMemObjGetRDMAToken failed");
            auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "cuMemObjGetRDMAToken failed", false);
            return Model::UploadPartRDMAOutcome(err);
        }

        request.SetRDMAToken(token);
        auto rsp = s3client->UploadPartRDMA(request);

        if (!rsp.IsSuccess()) {
            return rsp;
        }

        if (rsp.GetResult().GetRDMAReply() != 200)
        {
            // The HTTP request succeeded but the server declined RDMA for this part
            // (x-amz-rdma-reply != 200). As with PutObject, a decline means the server
            // never received any data, so the part was not stored - retry it over plain
            // TCP with the real body.
            AWS_LOGSTREAM_WARN(CUOBJ_S3_CLIENT_LOG_TAG, "UploadPartRDMA: server declined RDMA (x-amz-rdma-reply=" << rsp.GetResult().GetRDMAReply() << "), retrying over TCP");
            EnableRDMA(false);

            if (!src.is_system_memory()) {
                AWS_LOGSTREAM_ERROR(CUOBJ_S3_CLIENT_LOG_TAG, "UploadPartRDMA: server declined RDMA for a non-system-memory source - unable to retry over TCP");
                auto err = Aws::Client::AWSError<S3Errors>(S3Errors::INTERNAL_FAILURE, "INTERNAL_FAILURE", "RDMA was declined by the server for a non-system-memory source", false);
                return Model::UploadPartRDMAOutcome(err);
            }

            auto req = nativeio::toNativeRequest(request);
            const std::shared_ptr<Aws::IOStream> body = Aws::MakeShared<Aws::StringStream>("");
            body->rdbuf()->pubsetbuf(src.data(), src.size());
            req.SetBody(body);
            auto tcpRsp = s3client->UploadPartTCP(req);
            return nativeio::toRDMAResponse(tcpRsp);
        }

        auto data_sent_handler = request.GetDataSentEventHandler();
        if (data_sent_handler)
        {
            // Invoke data sent callback with the amount of data transferred
            data_sent_handler(nullptr, static_cast<long long>(src.size()));
        }

        return rsp;
    }

    Model::UploadPartRDMAOutcomeAndPtrCallable S3ClientRDMA::UploadPartRDMAPtrCallable(Model::UploadPartRDMARequest &request, RdmaPtr &&dst) const {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto dst_heap = new RdmaPtr(std::move(dst));

        auto s3client = dynamic_cast<const S3Client*>(this);
        auto task = Aws::MakeShared< std::packaged_task< std::pair<Model::UploadPartRDMAOutcome, RdmaPtr>() >>(s3client->GetAllocationTag(), [s3client, dst_heap, request]() mutable {
            auto dst = std::move(*dst_heap);
            delete dst_heap;
            auto outcome = s3client->UploadPartRDMA(request, dst);
            return std::make_pair(std::move(outcome), std::move(dst));
        });
        auto packagedFunction = [task]() { (*task)(); };
        s3client->m_clientConfiguration.executor->Submit(packagedFunction);
        return task->get_future();

    }

    void S3ClientRDMA::UploadPartRDMAAsync(Model::UploadPartRDMARequest &request, RdmaPtr &&src, const UploadPartRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context) const
    {
        // Move RdmaPtr to the heap so it can be passed through callbacks (knowing we'll only call back once)
        // In C++11, lambdas cannot capture move-only types, so we need to use a heap-allocated pointer
        auto src_heap = new RdmaPtr(std::move(src));

        auto s3client = dynamic_cast<const S3Client*>(this);
        s3client->m_clientConfiguration.executor->Submit( [s3client, src_heap, request, handler, context]() mutable
        {
            auto src = std::move(*src_heap);
            delete src_heap;
            auto outcome = s3client->UploadPartRDMA(request, src);
            handler(s3client, request, std::move(src), std::move(outcome), context);
        } );
    }

} // namespace S3
} // namespace Aws
