/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include "aws/s3/model/GetObjectRequest.h"
#include "aws/s3/model/PutObjectRequest.h"
#include "aws/s3/model/PutObjectRDMARequest.h"
#include "aws/s3/model/UploadPartRequest.h"
#include "aws/s3/model/UploadPartRDMARequest.h"
#include "aws/s3/model/CreateMultipartUploadRequest.h"
#include "aws/s3/model/CompleteMultipartUploadRequest.h"
#include "aws/s3/S3ServiceClientModel.h"

namespace nativeio
{
#define IS_SET(obj, name) obj.name##HasBeenSet()
#define GET_FIELD(obj, name) obj.Get##name()
#define SET_FIELD(obj, name, value) obj.Set##name(value)
#define COPY_REQUEST_FIELD(lhs, rhs, name)          \
    if (IS_SET(lhs, name))                          \
    {                                               \
        SET_FIELD(rhs, name, GET_FIELD(lhs, name)); \
    }
#define COPY_RESULT_FIELD(lhs, rhs, name) SET_FIELD(rhs, name, GET_FIELD(lhs, name))


    // Convert a rdma put object request into a native request
    inline Aws::S3::Model::PutObjectRequest toNativeRequest(const Aws::S3::Model::PutObjectRDMARequest &request)
    {
        Aws::S3::Model::PutObjectRequest req;

        req.SetDataSentEventHandler(request.GetDataSentEventHandler());
        COPY_REQUEST_FIELD(request, req, ACL);
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, CacheControl);
        COPY_REQUEST_FIELD(request, req, ContentDisposition);
        COPY_REQUEST_FIELD(request, req, ContentEncoding);
        COPY_REQUEST_FIELD(request, req, ContentLanguage);
        COPY_REQUEST_FIELD(request, req, ContentLength);
        COPY_REQUEST_FIELD(request, req, ContentMD5);
        COPY_REQUEST_FIELD(request, req, ContentType);
        COPY_REQUEST_FIELD(request, req, ChecksumAlgorithm);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32C);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC64NVME);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA1);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA256);
        COPY_REQUEST_FIELD(request, req, Expires);
        COPY_REQUEST_FIELD(request, req, IfMatch);
        COPY_REQUEST_FIELD(request, req, IfNoneMatch);
        COPY_REQUEST_FIELD(request, req, GrantFullControl);
        COPY_REQUEST_FIELD(request, req, GrantRead);
        COPY_REQUEST_FIELD(request, req, GrantReadACP);
        COPY_REQUEST_FIELD(request, req, GrantWriteACP);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, WriteOffsetBytes);
        COPY_REQUEST_FIELD(request, req, Metadata);
        COPY_REQUEST_FIELD(request, req, ServerSideEncryption);
        COPY_REQUEST_FIELD(request, req, StorageClass);
        COPY_REQUEST_FIELD(request, req, WebsiteRedirectLocation);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, SSEKMSKeyId);
        COPY_REQUEST_FIELD(request, req, SSEKMSEncryptionContext);
        COPY_REQUEST_FIELD(request, req, BucketKeyEnabled);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, Tagging);
        COPY_REQUEST_FIELD(request, req, ObjectLockMode);
        COPY_REQUEST_FIELD(request, req, ObjectLockRetainUntilDate);
        COPY_REQUEST_FIELD(request, req, ObjectLockLegalHoldStatus);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }

    // Convert a native put object request into a rdma request
    inline Aws::S3::Model::PutObjectRDMARequest toRDMARequest(const Aws::S3::Model::PutObjectRequest &request)
    {
        Aws::S3::Model::PutObjectRDMARequest req;

        req.SetDataSentEventHandler(request.GetDataSentEventHandler());
        COPY_REQUEST_FIELD(request, req, ACL);
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, CacheControl);
        COPY_REQUEST_FIELD(request, req, ContentDisposition);
        COPY_REQUEST_FIELD(request, req, ContentEncoding);
        COPY_REQUEST_FIELD(request, req, ContentLanguage);
        COPY_REQUEST_FIELD(request, req, ContentLength);
        COPY_REQUEST_FIELD(request, req, ContentMD5);
        req.SetContentType(request.GetContentType());
        COPY_REQUEST_FIELD(request, req, ChecksumAlgorithm);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32C);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC64NVME);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA1);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA256);
        COPY_REQUEST_FIELD(request, req, Expires);
        COPY_REQUEST_FIELD(request, req, IfMatch);
        COPY_REQUEST_FIELD(request, req, IfNoneMatch);
        COPY_REQUEST_FIELD(request, req, GrantFullControl);
        COPY_REQUEST_FIELD(request, req, GrantRead);
        COPY_REQUEST_FIELD(request, req, GrantReadACP);
        COPY_REQUEST_FIELD(request, req, GrantWriteACP);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, WriteOffsetBytes);
        COPY_REQUEST_FIELD(request, req, Metadata);
        COPY_REQUEST_FIELD(request, req, ServerSideEncryption);
        COPY_REQUEST_FIELD(request, req, StorageClass);
        COPY_REQUEST_FIELD(request, req, WebsiteRedirectLocation);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, SSEKMSKeyId);
        COPY_REQUEST_FIELD(request, req, SSEKMSEncryptionContext);
        COPY_REQUEST_FIELD(request, req, BucketKeyEnabled);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, Tagging);
        COPY_REQUEST_FIELD(request, req, ObjectLockMode);
        COPY_REQUEST_FIELD(request, req, ObjectLockRetainUntilDate);
        COPY_REQUEST_FIELD(request, req, ObjectLockLegalHoldStatus);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }

    // Convert a native put object response into an rdma response
    inline Aws::S3::Model::PutObjectRDMAOutcome toRDMAResponse(Aws::S3::Model::PutObjectOutcome &response)
    {
        if (!response.IsSuccess())
        {
            return Aws::S3::Model::PutObjectRDMAOutcome(response.GetError());
        }

        auto &res = response.GetResult();
        Aws::S3::Model::PutObjectRDMAResult result;

        COPY_RESULT_FIELD(res, result, Expiration);
        COPY_RESULT_FIELD(res, result, ETag);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32C);
        COPY_RESULT_FIELD(res, result, ChecksumCRC64NVME);
        COPY_RESULT_FIELD(res, result, ChecksumSHA1);
        COPY_RESULT_FIELD(res, result, ChecksumSHA256);
        COPY_RESULT_FIELD(res, result, ServerSideEncryption);
        COPY_RESULT_FIELD(res, result, VersionId);
        COPY_RESULT_FIELD(res, result, SSECustomerAlgorithm);
        COPY_RESULT_FIELD(res, result, SSECustomerKeyMD5);
        COPY_RESULT_FIELD(res, result, SSEKMSKeyId);
        COPY_RESULT_FIELD(res, result, SSEKMSEncryptionContext);
        COPY_RESULT_FIELD(res, result, BucketKeyEnabled);
        COPY_RESULT_FIELD(res, result, Size);
        COPY_RESULT_FIELD(res, result, RequestCharged);
        COPY_RESULT_FIELD(res, result, RequestId);

        return Aws::S3::Model::PutObjectRDMAOutcome(std::move(result));
    }

    // Convert a rdma put object response into a native response
    inline Aws::S3::Model::PutObjectOutcome toNativeResponse(Aws::S3::Model::PutObjectRDMAOutcome &response)
    {
        if (!response.IsSuccess())
        {
            return Aws::S3::Model::PutObjectOutcome(response.GetError());
        }

        auto &res = response.GetResult();
        Aws::S3::Model::PutObjectResult result;

        COPY_RESULT_FIELD(res, result, Expiration);
        COPY_RESULT_FIELD(res, result, ETag);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32C);
        COPY_RESULT_FIELD(res, result, ChecksumCRC64NVME);
        COPY_RESULT_FIELD(res, result, ChecksumSHA1);
        COPY_RESULT_FIELD(res, result, ChecksumSHA256);
        COPY_RESULT_FIELD(res, result, ServerSideEncryption);
        COPY_RESULT_FIELD(res, result, VersionId);
        COPY_RESULT_FIELD(res, result, SSECustomerAlgorithm);
        COPY_RESULT_FIELD(res, result, SSECustomerKeyMD5);
        COPY_RESULT_FIELD(res, result, SSEKMSKeyId);
        COPY_RESULT_FIELD(res, result, SSEKMSEncryptionContext);
        COPY_RESULT_FIELD(res, result, BucketKeyEnabled);
        COPY_RESULT_FIELD(res, result, Size);
        COPY_RESULT_FIELD(res, result, RequestCharged);
        COPY_RESULT_FIELD(res, result, RequestId);

        return Aws::S3::Model::PutObjectOutcome(std::move(result));
    }


    // Convert a rdma upload part request into a native request
    inline Aws::S3::Model::UploadPartRequest toNativeRequest(const Aws::S3::Model::UploadPartRDMARequest &request)
    {
        Aws::S3::Model::UploadPartRequest req;

        req.SetDataSentEventHandler(request.GetDataSentEventHandler());
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, ContentLength);
        COPY_REQUEST_FIELD(request, req, ContentMD5);
        COPY_REQUEST_FIELD(request, req, ChecksumAlgorithm);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32C);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC64NVME);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA1);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA256);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, PartNumber);
        COPY_REQUEST_FIELD(request, req, UploadId);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }

    // Convert a native upload part request into a rdma request
    inline Aws::S3::Model::UploadPartRDMARequest toRDMARequest(const Aws::S3::Model::UploadPartRequest &request)
    {
        Aws::S3::Model::UploadPartRDMARequest req;

        req.SetDataSentEventHandler(request.GetDataSentEventHandler());
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, ContentLength);
        COPY_REQUEST_FIELD(request, req, ContentMD5);
        COPY_REQUEST_FIELD(request, req, ChecksumAlgorithm);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC32C);
        COPY_REQUEST_FIELD(request, req, ChecksumCRC64NVME);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA1);
        COPY_REQUEST_FIELD(request, req, ChecksumSHA256);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, PartNumber);
        COPY_REQUEST_FIELD(request, req, UploadId);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }

    // Convert a native upload part response into a rdma response
    inline Aws::S3::Model::UploadPartRDMAOutcome toRDMAResponse(Aws::S3::Model::UploadPartOutcome &response)
    {
        if (!response.IsSuccess())
        {
            return Aws::S3::Model::PutObjectRDMAOutcome(response.GetError());
        }

        auto &res = response.GetResult();
        Aws::S3::Model::UploadPartRDMAResult result;

        COPY_RESULT_FIELD(res, result, ETag);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32C);
        COPY_RESULT_FIELD(res, result, ChecksumCRC64NVME);
        COPY_RESULT_FIELD(res, result, ChecksumSHA1);
        COPY_RESULT_FIELD(res, result, ChecksumSHA256);
        COPY_RESULT_FIELD(res, result, SSECustomerAlgorithm);
        COPY_RESULT_FIELD(res, result, SSECustomerKeyMD5);
        COPY_RESULT_FIELD(res, result, SSEKMSKeyId);
        COPY_RESULT_FIELD(res, result, BucketKeyEnabled);
        COPY_RESULT_FIELD(res, result, RequestCharged);
        COPY_RESULT_FIELD(res, result, RequestId);

        return Aws::S3::Model::UploadPartRDMAOutcome(std::move(result));
    }

    // Convert a rdma upload part response into a native response
    inline Aws::S3::Model::UploadPartOutcome toNativeResponse(Aws::S3::Model::UploadPartRDMAOutcome &response)
    {
        if (!response.IsSuccess())
        {
            return Aws::S3::Model::UploadPartOutcome(response.GetError());
        }

        auto &res = response.GetResult();
        Aws::S3::Model::UploadPartResult result;

        COPY_RESULT_FIELD(res, result, ETag);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32C);
        COPY_RESULT_FIELD(res, result, ChecksumCRC64NVME);
        COPY_RESULT_FIELD(res, result, ChecksumSHA1);
        COPY_RESULT_FIELD(res, result, ChecksumSHA256);
        COPY_RESULT_FIELD(res, result, SSECustomerAlgorithm);
        COPY_RESULT_FIELD(res, result, SSECustomerKeyMD5);
        COPY_RESULT_FIELD(res, result, SSEKMSKeyId);
        COPY_RESULT_FIELD(res, result, BucketKeyEnabled);
        COPY_RESULT_FIELD(res, result, RequestCharged);
        COPY_RESULT_FIELD(res, result, RequestId);

        return Aws::S3::Model::UploadPartOutcome(std::move(result));
    }

    // Convert an rdma put object request into a create MPU request
    inline Aws::S3::Model::CreateMultipartUploadRequest toCreateMPURequest(const Aws::S3::Model::PutObjectRDMARequest &request)
    {
        (void) request;
        Aws::S3::Model::CreateMultipartUploadRequest req;

        COPY_REQUEST_FIELD(request, req, ACL);
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, CacheControl);
        COPY_REQUEST_FIELD(request, req, ContentDisposition);
        COPY_REQUEST_FIELD(request, req, ContentEncoding);
        COPY_REQUEST_FIELD(request, req, ContentLanguage);
        COPY_REQUEST_FIELD(request, req, ContentType);
        COPY_REQUEST_FIELD(request, req, Expires);
        COPY_REQUEST_FIELD(request, req, GrantFullControl);
        COPY_REQUEST_FIELD(request, req, GrantRead);
        COPY_REQUEST_FIELD(request, req, GrantReadACP);
        COPY_REQUEST_FIELD(request, req, GrantWriteACP);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, Metadata);
        COPY_REQUEST_FIELD(request, req, ServerSideEncryption);
        COPY_REQUEST_FIELD(request, req, StorageClass);
        COPY_REQUEST_FIELD(request, req, WebsiteRedirectLocation);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, SSEKMSKeyId);
        COPY_REQUEST_FIELD(request, req, SSEKMSEncryptionContext);
        COPY_REQUEST_FIELD(request, req, BucketKeyEnabled);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, Tagging);
        COPY_REQUEST_FIELD(request, req, ObjectLockMode);
        COPY_REQUEST_FIELD(request, req, ObjectLockRetainUntilDate);
        COPY_REQUEST_FIELD(request, req, ObjectLockLegalHoldStatus);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }

    // Convert a create MPU response into an rdma put object response
    inline Aws::S3::Model::PutObjectRDMAOutcome toRDMAResponse(const Aws::S3::Model::CompleteMultipartUploadOutcome &response)
    {
        if (!response.IsSuccess())
        {
            return Aws::S3::Model::PutObjectRDMAOutcome(response.GetError());
        }

        auto &res = response.GetResult();
        Aws::S3::Model::PutObjectRDMAResult result;

        COPY_RESULT_FIELD(res, result, ETag);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32);
        COPY_RESULT_FIELD(res, result, ChecksumCRC32C);
        COPY_RESULT_FIELD(res, result, ChecksumCRC64NVME);
        COPY_RESULT_FIELD(res, result, ChecksumSHA1);
        COPY_RESULT_FIELD(res, result, ChecksumSHA256);
        COPY_RESULT_FIELD(res, result, SSEKMSKeyId);
        COPY_RESULT_FIELD(res, result, BucketKeyEnabled);
        COPY_RESULT_FIELD(res, result, RequestCharged);
        COPY_RESULT_FIELD(res, result, RequestId);

        return Aws::S3::Model::PutObjectRDMAOutcome(std::move(result));
    }

    inline Aws::S3::Model::CreateMultipartUploadRequest toCreateMPURequest(const Aws::S3::Model::PutObjectRequest &request)
    {
        (void) request;
        Aws::S3::Model::CreateMultipartUploadRequest req;

        COPY_REQUEST_FIELD(request, req, ACL);
        COPY_REQUEST_FIELD(request, req, Bucket);
        COPY_REQUEST_FIELD(request, req, CacheControl);
        COPY_REQUEST_FIELD(request, req, ContentDisposition);
        COPY_REQUEST_FIELD(request, req, ContentEncoding);
        COPY_REQUEST_FIELD(request, req, ContentLanguage);
        req.SetContentType(request.GetContentType());
        COPY_REQUEST_FIELD(request, req, Expires);
        COPY_REQUEST_FIELD(request, req, GrantFullControl);
        COPY_REQUEST_FIELD(request, req, GrantRead);
        COPY_REQUEST_FIELD(request, req, GrantReadACP);
        COPY_REQUEST_FIELD(request, req, GrantWriteACP);
        COPY_REQUEST_FIELD(request, req, Key);
        COPY_REQUEST_FIELD(request, req, Metadata);
        COPY_REQUEST_FIELD(request, req, ServerSideEncryption);
        COPY_REQUEST_FIELD(request, req, StorageClass);
        COPY_REQUEST_FIELD(request, req, WebsiteRedirectLocation);
        COPY_REQUEST_FIELD(request, req, SSECustomerAlgorithm);
        COPY_REQUEST_FIELD(request, req, SSECustomerKey);
        COPY_REQUEST_FIELD(request, req, SSECustomerKeyMD5);
        COPY_REQUEST_FIELD(request, req, SSEKMSKeyId);
        COPY_REQUEST_FIELD(request, req, SSEKMSEncryptionContext);
        COPY_REQUEST_FIELD(request, req, BucketKeyEnabled);
        COPY_REQUEST_FIELD(request, req, RequestPayer);
        COPY_REQUEST_FIELD(request, req, Tagging);
        COPY_REQUEST_FIELD(request, req, ObjectLockMode);
        COPY_REQUEST_FIELD(request, req, ObjectLockRetainUntilDate);
        COPY_REQUEST_FIELD(request, req, ObjectLockLegalHoldStatus);
        COPY_REQUEST_FIELD(request, req, ExpectedBucketOwner);
        COPY_REQUEST_FIELD(request, req, CustomizedAccessLogTag);

        return req;
    }
}
