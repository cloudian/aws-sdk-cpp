/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include <aws/s3/S3ServiceClientModel.h>
#include <aws/s3/RdmaPtr.h>
#include <aws/s3/RdmaBufferPool.h>

namespace Aws
{
namespace S3
{
    namespace Model {
        using GetObjectRDMARequest = Aws::S3::Model::GetObjectRequest;
        using GetObjectRDMAOutcome = Aws::S3::Model::GetObjectOutcome;
        using GetObjectRDMAOutcomeAndPtrCallable = std::future<std::pair<Aws::S3::Model::GetObjectOutcome, RdmaPtr>>;
        using PutObjectRDMAOutcomeAndPtrCallable = std::future<std::pair<Aws::S3::Model::PutObjectRDMAOutcome, RdmaPtr>>;
        using UploadPartRDMAOutcomeAndPtrCallable = std::future<std::pair<Aws::S3::Model::UploadPartRDMAOutcome, RdmaPtr>>;
    }

    class S3Client;

    // Config from environment variables, with default values if env vars are not set or invalid
    class EnvConfigVariable {
        size_t value_;

    public:
        EnvConfigVariable(const char* name, size_t default_value);

        size_t get() const { return value_; }
        void set(size_t value) { value_ = value; }
    };  


    /**
     * Adds RDMA functionality to S3Client.
     * This class is intended to be used as a mixin, with S3Client inheriting from it.
     * It is not intended to be used directly.
     */
    class S3ClientRDMA
    {
        RdmaBufferPool pool_;
        EnvConfigVariable maxConcurrentReads_;
        EnvConfigVariable rdmaThreshold_;
        EnvConfigVariable mpuThreshold_;
        EnvConfigVariable maxConcurrentMPUUploads_;
        EnvConfigVariable concurrentMPUUploadPartSize_;


    public:
        S3ClientRDMA();

        //=====================================================================
        // Enable/disable RDMA
        //=====================================================================

        /*
        * Test if RDMA is enabled for this client.
        */
        static bool IsRDMAEnabled();

        /*
        * Enable or disable RDMA for this client.
        */
        static void EnableRDMA(bool enable = true);

        /* Returns true when built with S3_CLIENT_DISABLE_TRANSPARENT_RDMA. */
        static constexpr bool IsTransparentRDMADisabled() {
#ifdef S3_CLIENT_DISABLE_TRANSPARENT_RDMA
            return true;
#else
            return false;
#endif
        }


        //=====================================================================
        // Buffer pool
        //=====================================================================

        /**
         * Allocate a buffer from the pool, returning it to the pool on
         * destruction.
         *
         * If sz is 0, the default size of the buffer will be used.
         * If sz is greater than the pool size, a new buffer will be allocated
         * (which will not be returned to the pool on destruction)
         */
        RdmaBuffer get_rdma_buffer(size_t sz = 0) { return pool_.get_buffer(sz); }

        /**
         * The size of buffers in the pool
         */
        size_t get_rdma_buffer_size() const { return pool_.buffer_size(); }


        /**
         * For testing, allow buffer pool to be resized.
         */
        void resize_buffer_pool(size_t capacity, size_t bufferSize) {
            pool_.resize(capacity, bufferSize);
        }

        /**
         * For testing, force rdma threshold to be set.
         */
        void force_rdma_threshold(size_t threshold) {
                rdmaThreshold_.set(threshold);
        }

        void force_mpu_threshold(size_t threshold) {
                mpuThreshold_.set(threshold);
        }


        //=====================================================================
        // GetObject support
        //=====================================================================

        /*
         * GetObject, using a series of get requests to transfer content into an RDMA buffer, copying contents to the response stream
        */
        virtual Aws::S3::Model::GetObjectOutcome GetObject(const Aws::S3::Model::GetObjectRequest &request) const;

        /**
         * A Callable wrapper for GetObject that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::GetObjectOutcomeCallable GetObjectCallable(const Model::GetObjectRequest& request) const;

        /* 
         * GetObjectAsync, async version of GetObject
        */
        virtual void GetObjectAsync(const Model::GetObjectRequest& request, const GetObjectResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /* GetObject, transferring contents using rdma into supplied memory buffer */
        virtual Aws::S3::Model::GetObjectRDMAOutcome GetObjectRDMA(Aws::S3::Model::GetObjectRDMARequest &request, void* dst, size_t len) const
        {
            return this->GetObjectRDMA(request, RdmaPtr(dst, len));
        }

        /* GetObject, transferring contents using rdma into pre-registered memory buffer
         *
         * WARNING: memory type (host vs GPU) is determined by querying the cuobjclient
         * library directly, which works regardless of whether RDMA transfers are
         * currently enabled (IsRDMAEnabled()). The one case this can't detect is the
         * library never having loaded at all (e.g. libcuobjclient.so is missing) - in
         * that case `dst` is unconditionally treated as host memory, since there's no
         * way to query its real type and no GPU-aware library could be in use anyway.
         * If that's a possibility in your environment, callers should still verify
         * IsRDMAEnabled() before calling this with a GPU-resident `dst`.
         */
        virtual Aws::S3::Model::GetObjectRDMAOutcome GetObjectRDMA(Aws::S3::Model::GetObjectRDMARequest &request, const RdmaPtr &dst) const;

        /**
         * A Callable wrapper for GetObjectRDMA that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::GetObjectRDMAOutcomeAndPtrCallable GetObjectRDMAPtrCallable(Model::GetObjectRDMARequest &request, RdmaPtr &&dst) const;

        virtual Model::GetObjectRDMAOutcomeAndPtrCallable GetObjectRDMAPtrCallable(Model::GetObjectRDMARequest &request, void* dst, size_t len) const
        {
            return GetObjectRDMAPtrCallable(request, RdmaPtr(dst, len));
        }

        /* Callback for GetObjectAsync. Ownership of the RdmaPtr is passed to the callback, ensuring the buffer remains registered for the life of the async operation */
        using GetObjectRDMAResponseReceivedHandler = std::function<void(const S3Client *, const Aws::S3::Model::GetObjectRDMARequest &, RdmaPtr &&, Aws::S3::Model::GetObjectRDMAOutcome &&, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &)>;

        /* GetObjectAsync, transferring contents using rdma into supplied memory buffer */
        virtual void GetObjectRDMAAsync(Aws::S3::Model::GetObjectRDMARequest &request, void* dst, size_t len, const GetObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const
        {
            GetObjectRDMAAsync(request, RdmaPtr(dst, len), handler, context);
        }

        /* GetObjectAsync, transferring contents using rdma into pre-registered memory buffer */
        virtual void GetObjectRDMAAsync(Aws::S3::Model::GetObjectRDMARequest &request, RdmaPtr &&dst, const GetObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const;


        //=====================================================================
        // PutObject support
        //=====================================================================

        /*
         * PutObject, using RDMA to transfer data if body is small enough to fit into a single RDMA buffer
         */
        virtual Aws::S3::Model::PutObjectOutcome PutObject(const Aws::S3::Model::PutObjectRequest &request) const;

        /**
         * A Callable wrapper for PutObject that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::PutObjectOutcomeCallable PutObjectCallable(const Model::PutObjectRequest& request) const;

        /* 
         * PutObjectAsync, async version of PutObject
        */
        virtual void PutObjectAsync(const Model::PutObjectRequest& request, const PutObjectResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /* PutObject, transferring contents using rdma from supplied memory buffer */
        virtual Aws::S3::Model::PutObjectRDMAOutcome PutObjectRDMA(Aws::S3::Model::PutObjectRDMARequest &request, const void* src, size_t len) const
        {
            return this->PutObjectRDMA(request, RdmaPtr(src, len));
        }

        /* PutObject, transferring contents using rdma from pre-registered memory buffer
         *
         * WARNING: same caveat as GetObjectRDMA - memory type is queried directly and
         * doesn't depend on IsRDMAEnabled(), except when the cuobjclient library never
         * loaded at all, in which case `src` is unconditionally treated as host memory.
         */
        virtual Aws::S3::Model::PutObjectRDMAOutcome PutObjectRDMA(Aws::S3::Model::PutObjectRDMARequest &request, const RdmaPtr &src) const;

        /**
         * A Callable wrapper for PutObjectRDMA that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::PutObjectRDMAOutcomeAndPtrCallable PutObjectRDMAPtrCallable(Model::PutObjectRDMARequest &request, RdmaPtr &&dst) const;

        virtual Model::PutObjectRDMAOutcomeAndPtrCallable PutObjectRDMAPtrCallable(Model::PutObjectRDMARequest &request, void* dst, size_t len) const
        {
            return PutObjectRDMAPtrCallable(request, RdmaPtr(dst, len));
        }

        using PutObjectRDMAResponseReceivedHandler = std::function<void(const S3Client *, const Aws::S3::Model::PutObjectRDMARequest &, RdmaPtr &&, Aws::S3::Model::PutObjectRDMAOutcome &&, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &)>;

        /* PutObjectAsync, transferring contents using rdma from supplied memory buffer */
        virtual void PutObjectRDMAAsync(Aws::S3::Model::PutObjectRDMARequest &request, const void* src, size_t len, const PutObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const
        {
            PutObjectRDMAAsync(request, RdmaPtr(src, len), handler, context);
        }

        /* PutObjectAsync, transferring contents using rdma from pre-registered memory buffer */
        virtual void PutObjectRDMAAsync(Aws::S3::Model::PutObjectRDMARequest &request, RdmaPtr &&src, const PutObjectRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const;


        //=====================================================================
        // UploadPart support
        //=====================================================================

        /*
         * UploadPart, using RDMA to transfer data if body is small enough to fit into a single RDMA buffer
        */
        virtual Aws::S3::Model::UploadPartOutcome UploadPart(const Aws::S3::Model::UploadPartRequest &request) const;

        /**
         * A Callable wrapper for UploadPart that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::UploadPartOutcomeCallable UploadPartCallable(const Model::UploadPartRequest& request) const;

        /* 
         * UploadPartAsync, async version of UploadPart
        */
        virtual void UploadPartAsync(const Model::UploadPartRequest& request, const UploadPartResponseReceivedHandler& handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context = nullptr) const;

        /* UploadPart, transferring contents using rdma from supplied memory buffer */
        virtual Aws::S3::Model::UploadPartRDMAOutcome UploadPartRDMA(Aws::S3::Model::UploadPartRDMARequest &request, const void *src, size_t len) const
        {
            return this->UploadPartRDMA(request, RdmaPtr(src, len));
        }

        /* UploadPart, transferring contents using rdma from pre-registered memory buffer
         *
         * WARNING: same caveat as GetObjectRDMA - memory type is queried directly and
         * doesn't depend on IsRDMAEnabled(), except when the cuobjclient library never
         * loaded at all, in which case `src` is unconditionally treated as host memory.
         */
        virtual Aws::S3::Model::UploadPartRDMAOutcome UploadPartRDMA(Aws::S3::Model::UploadPartRDMARequest &request, const RdmaPtr &src) const;

        /**
         * A Callable wrapper for UploadPartRDMA that returns a future to the operation so that it can be executed in parallel to other requests.
         */
        virtual Model::UploadPartRDMAOutcomeAndPtrCallable UploadPartRDMAPtrCallable(Model::UploadPartRDMARequest &request, RdmaPtr &&dst) const;

        virtual Model::UploadPartRDMAOutcomeAndPtrCallable UploadPartRDMAPtrCallable(Model::UploadPartRDMARequest &request, void* dst, size_t len) const
        {
            return UploadPartRDMAPtrCallable(request, RdmaPtr(dst, len));
        }

        using UploadPartRDMAResponseReceivedHandler = std::function<void(const S3Client *, const Aws::S3::Model::UploadPartRDMARequest &, RdmaPtr &&, Aws::S3::Model::UploadPartRDMAOutcome &&, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &)>;

        /* PutObjectAsync, transferring contents using rdma from supplied memory buffer */
        virtual void UploadPartRDMAAsync(Aws::S3::Model::UploadPartRDMARequest &request, const void *src, const size_t len, const UploadPartRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const
        {
            UploadPartRDMAAsync(request, RdmaPtr(src, len), handler, context);
        }

        /* UploadPartAsync, transferring contents using rdma from pre-registered memory buffer */
        virtual void UploadPartRDMAAsync(Aws::S3::Model::UploadPartRDMARequest &request, RdmaPtr &&src, const UploadPartRDMAResponseReceivedHandler &handler, const std::shared_ptr<const Aws::Client::AsyncCallerContext> &context = nullptr) const;

    private:
        void CheckRdmaStatus();
        bool bodyToRDMABuffer(const std::shared_ptr<Aws::IOStream> &body, RdmaBuffer& buffer) const;
        Model::PutObjectRDMAOutcome uploadUsingMPU(Model::PutObjectRDMARequest &request, const RdmaPtr &src) const;
        Model::PutObjectOutcome uploadUsingMPUFromStream(const Model::PutObjectRequest &request) const;
    };
} // namespace S3
} // namespace Aws
