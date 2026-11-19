/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "aws/s3/RdmaBufferPool.h"
#include "aws/core/utils/logging/LogMacros.h"

#include "RdmaObj.h"

#include <cstdlib>
#include <cstring>

namespace Aws {
namespace S3 {

static const char BUFFER_POOL_TAG[] = "RdmaBufferPool";

size_t RdmaBufferPool::defaultMaxPoolSize() {
    auto env = std::getenv("S3RDMA_BUFFER_POOL_CAPACITY");
    if (env != nullptr) {
        size_t capacity = strtoul(env, nullptr, 0);
        if (capacity) {
            return capacity;
        } else {
            AWS_LOGSTREAM_ERROR(BUFFER_POOL_TAG, "Invalid S3RDMA_BUFFER_POOL_CAPACITY value: " << env);
        }
    }
    // Default to 128 buffers
    return 128;
}

size_t RdmaBufferPool::defaultBufferSize() {
    auto env = std::getenv("S3RDMA_BUFFER_POOL_BUFSIZE_MIB");
    if (env != nullptr) {
        size_t size = strtoul(env, nullptr, 0);
        if (size != 0) {
            return size << 20;
        } else {
            AWS_LOGSTREAM_ERROR(BUFFER_POOL_TAG, "Invalid S3RDMA_BUFFER_POOL_BUFSIZE_MIB value: " << env);
        }
    }
    // Default to 10MB
    return 10 << 20;
}


char* RdmaBufferPool::allocate_buffer(size_t sz) {
    auto buf = malloc(sz);
    // Register whenever the library is loaded at all, regardless of whether RDMA
    // transfers are currently enabled (RdmaObj::native_io can flip at runtime, e.g.
    // after a decline) - RdmaObj::rdmaobj itself never changes once set, so
    // allocate_buffer() and free_buffer() always agree on whether a given buffer
    // was registered, with nothing extra to track. Registering buffers we might
    // not end up using RDMA for costs a little unnecessary setup, which is cheap.
    if (RdmaObj::rdmaobj) {
        auto rc = RdmaObj::rdmaobj->rdmaMemObjGetDescriptor(buf, sz);
        if (rc != CU_OBJ_SUCCESS) {
            AWS_LOGSTREAM_ERROR(BUFFER_POOL_TAG, "failed to register rdma buffer: ptr=" << std::hex << buf << std::dec << " length=" << sz << " rc=" << rc);
            free(buf);
            return nullptr;
        }
    }
    return (char*) buf;
}

void RdmaBufferPool::free_buffer(char* buf) {
    if (RdmaObj::rdmaobj) {
        auto rc = RdmaObj::rdmaobj->rdmaMemObjPutDescriptor(buf);
        if (rc != CU_OBJ_SUCCESS) {
            AWS_LOGSTREAM_ERROR(BUFFER_POOL_TAG, "failed to deregister rdma buffer: ptr=" << std::hex << (void*) buf << std::dec << " rc=" << rc);
        }
    }
    free(buf);
}

} // namespace S3
} // namespace Aws
