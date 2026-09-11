/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "CuObjClient.h"

// cuobjclient.h (pulled in via CuObjClient.h) is used here only for its
// types - sizeof(cuObjClient)/alignof(cuObjClient) and the mangled symbol
// signatures below - never to call any of its declared functions directly.
// That keeps this translation unit free of any undefined reference to
// libcuobjclient.so, so the main library never gets a build-time
// (DT_NEEDED) link dependency on it; the library is located and bound
// purely at runtime via dlopen()/dlsym() further down.

#include <cstdlib>
#include <dlfcn.h>

namespace
{
    // Itanium C++ ABI signatures for cuObjClient's mangled symbols.
    // Non-static member functions take the object as an implicit first
    // `thisPtr` argument; static member functions do not.
    using Ctor_t = void (*)(void *thisPtr, CUObjOps_t *ops, int proto);
    using Dtor_t = void (*)(void *thisPtr);
    using GetDescriptor_t = cuObjErr_t (*)(void *thisPtr, void *ptr, size_t size);
    using PutDescriptor_t = cuObjErr_t (*)(void *thisPtr, void *ptr);
    using IsConnected_t = bool (*)(void *thisPtr);
    using GetRDMAToken_t = cuObjErr_t (*)(void *thisPtr, void *ptr, size_t size, size_t buffer_offset, cuObjOpType_t operation, char **desc_str_out);
    using PutRDMAToken_t = cuObjErr_t (*)(void *thisPtr, char *desc_str);
    using GetMemoryType_t = cuObjMemoryType_t (*)(const void *ptr);

    // Mangled Itanium ABI symbol names for cuObjClient, derived from NVIDIA's
    // cuobjclient.h by compiling a reference translation unit that calls
    // each of these operations and reading its undefined symbols
    // (nm --undefined-only -C). Re-derive these if the vendor header's
    // cuObjClient signatures ever change:
    //   cuObjClient::cuObjClient(CUObjIOOps&, cuObjProto_enum)
    //   cuObjClient::~cuObjClient()
    //   cuObjClient::cuMemObjGetDescriptor(void*, unsigned long)
    //   cuObjClient::cuMemObjPutDescriptor(void*)
    //   cuObjClient::isConnected()
    //   cuObjClient::cuMemObjGetRDMAToken(void*, unsigned long, unsigned long, cuObjOpType_enum, char**)
    //   cuObjClient::cuMemObjPutRDMAToken(char*)
    //   cuObjClient::getMemoryType(void const*)
    constexpr const char *kCtorSym = "_ZN11cuObjClientC1ER10CUObjIOOps15cuObjProto_enum";
    constexpr const char *kDtorSym = "_ZN11cuObjClientD1Ev";
    constexpr const char *kGetDescriptorSym = "_ZN11cuObjClient21cuMemObjGetDescriptorEPvm";
    constexpr const char *kPutDescriptorSym = "_ZN11cuObjClient21cuMemObjPutDescriptorEPv";
    constexpr const char *kIsConnectedSym = "_ZN11cuObjClient11isConnectedEv";
    constexpr const char *kGetRDMATokenSym = "_ZN11cuObjClient20cuMemObjGetRDMATokenEPvmm16cuObjOpType_enumPPc";
    constexpr const char *kPutRDMATokenSym = "_ZN11cuObjClient20cuMemObjPutRDMATokenEPc";
    constexpr const char *kGetMemoryTypeSym = "_ZN11cuObjClient13getMemoryTypeEPKv";

    template <typename Fn>
    bool resolve(void *handle, const char *name, Fn &out, const char **error)
    {
        dlerror();
        out = reinterpret_cast<Fn>(dlsym(handle, name));
        if (const char *dlsymError = dlerror()) {
            (void) dlsymError;
            *error = "Cannot load libcuobjclient.so symbol";
            return false;
        }
        return true;
    }

    // cuObjClient's constructor requires an IO callback struct, but those
    // callbacks are only ever invoked from cuObjGet()/cuObjPut(), which we
    // never call - we get RDMA descriptors directly via getRDMAToken(()).
    // These stand in for that unused, structurally-required parameter.
    ssize_t unusedGetCallback(const void *, char *, size_t, loff_t, const cufileRDMAInfo_t *) { return -1; }
    ssize_t unusedPutCallback(const void *, const char *, size_t, loff_t, const cufileRDMAInfo_t *) { return -1; }
}

struct CuObjClient::Abi {
    Ctor_t ctor;
    Dtor_t dtor;
    GetDescriptor_t getDescriptor;
    PutDescriptor_t putDescriptor;
    IsConnected_t isConnected;
    GetRDMAToken_t getRDMAToken;
    PutRDMAToken_t putRDMAToken;
    GetMemoryType_t getMemoryType;
};

namespace
{
    struct AbiResolution {
        const CuObjClient::Abi *abi;
        const char *error;
    };

    // Resolved (and cached) on first use; safe to call from any thread and
    // any number of times, always returning the same cached result.
    const AbiResolution &resolveAbi()
    {
        static const AbiResolution resolution = []() -> AbiResolution {
            void *handle = dlopen("libcuobjclient.so", RTLD_LAZY | RTLD_LOCAL);
            if (!handle) {
                return {nullptr, "Cannot load libcuobjclient.so"};
            }

            static CuObjClient::Abi resolved;
            const char *error = nullptr;
            if (resolve(handle, kCtorSym, resolved.ctor, &error) &&
                resolve(handle, kDtorSym, resolved.dtor, &error) &&
                resolve(handle, kGetDescriptorSym, resolved.getDescriptor, &error) &&
                resolve(handle, kPutDescriptorSym, resolved.putDescriptor, &error) &&
                resolve(handle, kIsConnectedSym, resolved.isConnected, &error) &&
                resolve(handle, kGetRDMATokenSym, resolved.getRDMAToken, &error) &&
                resolve(handle, kPutRDMATokenSym, resolved.putRDMAToken, &error) &&
                resolve(handle, kGetMemoryTypeSym, resolved.getMemoryType, &error)) {
                return {&resolved, nullptr};
            }
            return {nullptr, error};
        }();
        return resolution;
    }
}

std::unique_ptr<CuObjClient> CuObjClient::create(const char **error)
{
    const AbiResolution &resolution = resolveAbi();
    if (!resolution.abi) {
        if (error) {
            *error = resolution.error;
        }
        return nullptr;
    }

    // cuObjClient declares no operator new() of its own, so a plain,
    // correctly sized and aligned allocation is a valid target for
    // placement-constructing it.
    void *raw = nullptr;
    if (posix_memalign(&raw, alignof(cuObjClient), sizeof(cuObjClient)) != 0) {
        if (error) {
            *error = "Cannot allocate memory for cuObjClient";
        }
        return nullptr;
    }

    CUObjOps_t ops = { unusedGetCallback, unusedPutCallback };
    resolution.abi->ctor(raw, &ops, static_cast<int>(CUOBJ_PROTO_RDMA_DC_V1));

    return std::unique_ptr<CuObjClient>(new CuObjClient(*resolution.abi, raw));
}

CuObjClient::~CuObjClient()
{
    // Deliberately not calling the cuObjClient destructor here, because
    // we see concurrent destruction can cause heap corruption (double linked list corruption) in libcuobjclient.so.
    // abi_->dtor(impl_);
    free(impl_);
}

cuObjErr_t CuObjClient::rdmaMemObjGetDescriptor(void *ptr, size_t size)
{
    return abi_->getDescriptor(impl_, ptr, size);
}

cuObjErr_t CuObjClient::rdmaMemObjPutDescriptor(void *ptr)
{
    return abi_->putDescriptor(impl_, ptr);
}

bool CuObjClient::isConnected()
{
    return abi_->isConnected(impl_);
}

Aws::String CuObjClient::getRDMAToken(void *ptr, size_t size, size_t buffer_offset, cuObjOpType_t operation)
{
    char *desc = nullptr;
    if (abi_->getRDMAToken(impl_, ptr, size, buffer_offset, operation, &desc) != CU_OBJ_SUCCESS) {
        return Aws::String();
    }

    Aws::String token(desc);
    putRDMAToken(desc);
    return token;
}

cuObjErr_t CuObjClient::putRDMAToken(char *desc)
{
    return abi_->putRDMAToken(impl_, desc);
}

cuObjMemoryType_t CuObjClient::getMemoryType(const void *ptr)
{
    const AbiResolution &resolution = resolveAbi();
    return resolution.abi ? resolution.abi->getMemoryType(ptr) : CUOBJ_MEMORY_INVALID;
}
