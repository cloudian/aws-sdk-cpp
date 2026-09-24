/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include "RdmaObj.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace RdmaObj
{
    /**
     * Check if an environment variable flag is enabled.
     * Considers the flag enabled if the environment variable is set to:
     * - "1", "true", "yes", "on" (case insensitive)
     * - or if it exists and has no value (empty string)
     * Returns false if the variable doesn't exist or is set to "0", "false", "no", "off"
     */
    static bool isEnvFlagEnabled(const char* envVarName) {
        const char* value = std::getenv(envVarName);
        if (!value) {
            return false; // Environment variable not set
        }

        // If empty string, consider it enabled (just setting the variable enables it)
        if (strlen(value) == 0) {
            return true;
        }

        // Convert to lowercase for case-insensitive comparison
        std::string lowerValue = value;
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);

        // Check for enabled values
        return (lowerValue == "1" || lowerValue == "true" || lowerValue == "yes" || lowerValue == "on");
    }

    std::unique_ptr<CuObjClient> rdmaobj;

    static const char* rdmaStatusCheck() {
        const char *error = nullptr;
        auto client = CuObjClient::create(&error);
        if (!client) {
            return error ? error : "Cannot load libcuobjclient.so";
        }

        if (!client->isConnected()) {
            return "RDMA disabled - rdmaObjClient not connected";
        }

        if (isEnvFlagEnabled("S3RDMA_CLIENT_NATIVE_IO") || isEnvFlagEnabled("S3RDMA_CLIENT_ALWAYS_USE_TCP")) {
            return "RDMA disabled - S3RDMA_CLIENT_NATIVE_IO or S3RDMA_CLIENT_ALWAYS_USE_TCP environment variable enabled";
        }

        // Do a dummy getRDMAToken to check that RDMA is available
        char test_buf[64];

        // register the buffer
        if (client->rdmaMemObjGetDescriptor(test_buf, sizeof(test_buf)) != CU_OBJ_SUCCESS) {
            return "RDMA disabled - rdmaMemObjGetDescriptor failed";
        }

        bool tokenOk = !client->getRDMAToken(test_buf, sizeof(test_buf), 0, CUOBJ_PUT).empty();

        // deregister the buffer
        if (client->rdmaMemObjPutDescriptor(test_buf) != CU_OBJ_SUCCESS) {
            return "RDMA disabled - rdmaMemObjPutDescriptor failed";
        }

        if (!tokenOk) {
            return "RDMA disabled - test cuMemObjGetRDMAToken failed";
        }

        rdmaobj = std::move(client);

        return nullptr;
    }

    const char* rdma_status = rdmaStatusCheck();

    // Declared before local_rdmaobj so it's already initialized with its
    // real value by the time createLocalRdmaObj() (local_rdmaobj's own
    // initializer) reads it - same-TU dynamic initializers run in
    // declaration order.
    bool native_io = rdma_status != nullptr;

    static std::unique_ptr<CuObjClient> createLocalRdmaObj() {
        if (native_io) {
            // RDMA isn't usable process-wide, so skip constructing (and,
            // since ~CuObjClient() deliberately skips the vendor
            // destructor, leaking) a per-thread client that would never
            // be used anyway.
            return nullptr;
        }
        const char *error = nullptr;
        return CuObjClient::create(&error);
    }

    thread_local std::unique_ptr<CuObjClient> local_rdmaobj = createLocalRdmaObj();

    cuObjMemoryType_t rdmaObjGetMemoryType(const void *ptr)
    {
        return CuObjClient::getMemoryType(ptr);
    }
}
