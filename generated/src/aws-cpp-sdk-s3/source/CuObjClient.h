/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

// cuobjclient.h is redistributable under the CUDA EULA
// (https://docs.nvidia.com/cuda/eula/index.html), and we already require
// the CUDA toolkit to build, so we use its types directly rather than
// mirroring them in our own header.
#include <cuobjclient.h>

#include <aws/core/utils/memory/stl/AWSString.h>

#include <memory>

// Forward reference
class RdmaToken;

/**
 * Binds directly to NVIDIA's libcuobjclient.so cuObjClient class at runtime,
 * via dlopen()/dlsym() on its mangled Itanium C++ ABI symbol names. There is
 * no build-time link dependency on libcuobjclient.so, and no separate
 * wrapper shared object: this class lives in the main library and is the
 * only thing that talks to the vendor library.
 *
 * libcuobjclient.so is entirely optional at runtime: if it cannot be
 * dlopen'd, or a required symbol cannot be resolved, create() returns
 * nullptr so callers can fall back to plain TCP.
 */
class CuObjClient {
public:
    struct Abi; // opaque set of resolved libcuobjclient.so symbols; defined in CuObjClient.cpp

    // Attempts to dlopen libcuobjclient.so and construct a cuObjClient. On
    // failure returns nullptr and, if error is non-null, sets *error to the
    // reason. The IO callbacks cuObjClient's constructor requires are never
    // invoked - callers get RDMA descriptors directly via getRDMAToken()
    // instead of going through cuObjGet()/cuObjPut() - so no ops need be
    // supplied here.
    static std::unique_ptr<CuObjClient> create(const char **error);

    ~CuObjClient();

    CuObjClient(const CuObjClient &) = delete;
    CuObjClient(const CuObjClient &&) = delete;
    CuObjClient &operator=(const CuObjClient &) = delete;
    CuObjClient &operator=(const CuObjClient &&) = delete;

    cuObjErr_t rdmaMemObjGetDescriptor(void *ptr, size_t size);
    cuObjErr_t rdmaMemObjPutDescriptor(void *ptr);
    bool isConnected();

    // Generates an RDMA descriptor for the [buffer_offset, buffer_offset+size)
    // window of a buffer previously registered (in full) via
    // rdmaMemObjGetDescriptor(ptr, ...).
    // If the operation cannot be performed, the returned RdmaToken will be empty.
    RdmaToken getRDMAToken(void *ptr, size_t size, size_t buffer_offset, cuObjOpType_t operation);

    // Classifies a pointer as system, CUDA managed, or CUDA device memory.
    static cuObjMemoryType_t getMemoryType(const void *ptr);

private:
    CuObjClient(const Abi &abi, void *impl) : abi_(&abi), impl_(impl) {}

    friend class RdmaToken;
    cuObjErr_t putRDMAToken(char *desc);

    const Abi *abi_;
    void *impl_; // raw cuObjClient*, constructed via our own placement new
};

/**
    An RDMA token allocated by the cuobjclient library.
    Returns the token to the library on destruction
*/
class RdmaToken {
    friend class CuObjClient;
    RdmaToken(CuObjClient* client, const char* token) : client_(client), token_(token) {}

public:
    ~RdmaToken() {
        release();
    }

    RdmaToken(const RdmaToken &) = delete;
    RdmaToken &operator=(const RdmaToken &) = delete;

    RdmaToken(RdmaToken &&other) noexcept : client_(other.client_), token_(other.token_) {
        other.token_ = nullptr;
    }

    RdmaToken &operator=(RdmaToken &&other) noexcept {
        if (this != &other) {
            release();
            client_ = other.client_;
            token_ = other.token_;
            other.token_ = nullptr;
        }
        return *this;
    }

    bool empty() const { return token_ == nullptr; }

    Aws::String get() const { return token_ ? Aws::String(token_) : ""; }

private:
    void release() {
        if (token_) {
            client_->putRDMAToken(const_cast<char*>(token_));
        }
    }

    CuObjClient* client_;
    const char* token_;
};
