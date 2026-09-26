/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "aws/s3/RdmaPtr.h"
#include "aws/core/utils/logging/LogMacros.h"

#include "RdmaObj.h"

#include <stdexcept>
#include <cassert>


namespace Aws
{
namespace S3
{

    static const char PTR_TAG[] = "RdmaBufferPool";

    RdmaPtr::RdmaPtr(const void* pdata, size_t len, bool registered) : data_{pdata}, size_{len}, registered_{registered}, owns_registration_{false}, slice_offset_{0}, slice_size_{len}
    {
        if (!RdmaObj::native_io && !registered_)
        {
            AWS_LOGSTREAM_TRACE(PTR_TAG, "Registering buffer: ptr=" << pdata << " length=" << len);
            auto rc = RdmaObj::rdmaobj->rdmaMemObjGetDescriptor(data(), size());
            if (rc != CU_OBJ_SUCCESS)
            {
                AWS_LOGSTREAM_ERROR(PTR_TAG, "Failed to register: ptr=" << (void*) pdata << " length=" << len << ", rc=" << rc);
                data_ = nullptr;
            }
            else
            {
                owns_registration_ = true;
            }
        }
    }

    RdmaPtr::~RdmaPtr()
    {
        release();
    }

    void RdmaPtr::release()
    {
        if (owns_registration_ && data_ != nullptr)
        {
            AWS_LOGSTREAM_TRACE(PTR_TAG, "Freeing buffer: ptr=" << (void*) data_ << " length=" << size_);
            auto rc = RdmaObj::rdmaobj->rdmaMemObjPutDescriptor(data());
            if (rc != CU_OBJ_SUCCESS)
            {
                AWS_LOGSTREAM_WARN(PTR_TAG, "Failed to deregister: ptr=" << data_ << " length=" << size_ << ", rc=" << rc);
            }
            else
            {
                owns_registration_ = false;
                data_ = nullptr;
            }
        }
    }


    bool RdmaPtr::is_system_memory() const {
        // rdmaObjGetMemoryType() is backed by a static cuObjClient function that
        // resolves its own symbols independently of RdmaObj::rdmaobj/native_io, so it's
        // always safe to call regardless of whether RDMA is currently enabled or
        // connected. It only fails (CUOBJ_MEMORY_INVALID) if libcuobjclient.so itself
        // never loaded, in which case we can't determine memory type at all - assume
        // system memory (no cuobjclient = no cuda = no gpu).
        auto type = RdmaObj::rdmaObjGetMemoryType(data_);
        return type == CUOBJ_MEMORY_SYSTEM || type == CUOBJ_MEMORY_INVALID;
    }
} // namespace S3
} // namespace Aws
