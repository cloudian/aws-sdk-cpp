/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include "CuObjClient.h"

#include <memory>

namespace RdmaObj
{
    // rdmaobj/local_rdmaobj are null when libcuobjclient.so isn't available
    // at runtime. Checking !native_io is not sufficient on its own to prove
    // local_rdmaobj is non-null - it's also left null on any thread whose
    // own per-thread client construction failed, or that first touched it
    // while RDMA was disabled and native_io was later toggled back to
    // false without reconstructing it - so callers must still null-check
    // local_rdmaobj itself before dereferencing it.
    extern std::unique_ptr<CuObjClient> rdmaobj;
    extern const char* rdma_status;
    extern thread_local std::unique_ptr<CuObjClient> local_rdmaobj;
    extern bool native_io;

    cuObjMemoryType_t rdmaObjGetMemoryType(const void *ptr);
}
