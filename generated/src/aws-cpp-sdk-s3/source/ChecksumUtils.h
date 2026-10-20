/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include "aws/s3/RdmaPtr.h"
#include "aws/core/utils/HashingUtils.h"
#include <aws/core/utils/crypto/MD5.h>
#include <aws/core/utils/crypto/CRC32.h>
#include <aws/core/utils/crypto/CRC64.h>
#include <aws/core/utils/crypto/Sha1.h>
#include <aws/core/utils/crypto/Sha256.h>
#include <aws/s3/model/ChecksumAlgorithm.h>



namespace checksum
{
    template <typename request_t>
    void calculate(request_t &req, const Aws::S3::RdmaPtr &src)
    {
        if (!src.is_system_memory()) {
            AWS_LOGSTREAM_WARN("ChecksumUtils", "Cannot calculate checksum for non-system memory RDMA buffer");
            return;
        }

        // Calculate checksum if requested
        switch (req.GetChecksumAlgorithm())
        {
        case Aws::S3::Model::ChecksumAlgorithm::NOT_SET:
            /*
            // HyperStore no longer computes an MD5 for RDMA transfers
            if (!req.ChecksumCRC32HasBeenSet() && !req.ChecksumCRC32CHasBeenSet() && !req.ChecksumSHA1HasBeenSet() && !req.ChecksumSHA256HasBeenSet() && !req.ContentMD5HasBeenSet())
            {
                // Add md5 hash if no checksum has been set
                Aws::Utils::Crypto::MD5 hash;
                hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
                req.SetContentMD5(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            }
            */
            break;
        case Aws::S3::Model::ChecksumAlgorithm::CRC32:
        {
            Aws::Utils::Crypto::CRC32 hash;
            hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
            req.SetChecksumCRC32(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            break;
        }
        case Aws::S3::Model::ChecksumAlgorithm::CRC32C:
        {
            Aws::Utils::Crypto::CRC32C hash;
            hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
            req.SetChecksumCRC32C(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            break;
        }
        case Aws::S3::Model::ChecksumAlgorithm::SHA1:
        {
            Aws::Utils::Crypto::Sha1 hash;
            hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
            req.SetChecksumSHA1(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            break;
        }
        case Aws::S3::Model::ChecksumAlgorithm::SHA256:
        {
            Aws::Utils::Crypto::Sha256 hash;
            hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
            req.SetChecksumSHA256(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            break;
        }
        case Aws::S3::Model::ChecksumAlgorithm::CRC64NVME:
        {
            Aws::Utils::Crypto::CRC64 hash;
            hash.Update(reinterpret_cast<unsigned char *>(src.data()), src.size());
            req.SetChecksumCRC64NVME(Aws::Utils::HashingUtils::Base64Encode(hash.GetHash().GetResult()));
            break;
        }
        default:
            AWS_LOGSTREAM_WARN("ChecksumUtils", "Unsupported checksum algorithm for RDMA request; leaving checksum headers unchanged");
            break;
        }
    }

}
