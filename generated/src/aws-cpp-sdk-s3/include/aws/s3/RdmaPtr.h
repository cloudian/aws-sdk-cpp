/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include <cassert>
#include <cstddef>

namespace Aws
{
namespace S3
{
    /**
     * A wrapper over a memory region which registers it for RDMA transfers.
     * The memory is automatically unregistered on destruction.
     * (We skip registration for pre-registered buffers allocated from the pool)
     */
    class RdmaPtr
    {
    private:
        const void* data_;
        size_t size_;
        const bool registered_;
        // Whether this instance took out the registration and must release it.
        bool owns_registration_;
        size_t slice_offset_;
        size_t slice_size_;

    public:
        RdmaPtr(const void* data, size_t len, bool registered = false);
        RdmaPtr(const RdmaPtr &rhs) = delete;
        RdmaPtr &operator=(const RdmaPtr &rhs) = delete;

        RdmaPtr(RdmaPtr &&rhs) : data_{rhs.data_}, size_{rhs.size_}, registered_{rhs.registered_}, owns_registration_{rhs.owns_registration_}, slice_offset_{rhs.slice_offset_}, slice_size_{rhs.slice_size_} {
            rhs.data_ = nullptr;
            rhs.owns_registration_ = false;
        }

        ~RdmaPtr();

        /*
         * Test if rdma registration has been successful
         */ 
        operator bool () const { return data_ != nullptr; }

        /**
         * Test if this points to system memory (as opposed to GPU memory)
         */
        bool is_system_memory() const;

        /**
         * Operate on a slice of the rdma registered region
         * Returns a new RdmaPtr that shares the same underlying memory region but with a different offset and size.
         * The returned RdmaPtr will use the original Ptr's registration, so the original Ptr's registration must outlive the sliced ptr
         */
        RdmaPtr slice(size_t offset, size_t size) const {
            assert(offset < slice_size_);
            assert(offset + size <= slice_size_);

            auto ptr = RdmaPtr(this->data_, this->size_, true);
            ptr.slice_offset_ = slice_offset_ + offset;
            ptr.slice_size_ = size;
            return ptr;
        }

        // The underlying buffer that has been registered with cuObject
        char *registered_data() const { return reinterpret_cast<char *>(const_cast<void*>(data_)); }

        // The slice of the buffer used for this transfer (may be the entire buffer or a subset of it)
        char* data() const { return registered_data() + slice_offset_; }
        size_t offset() const { return slice_offset_; }
        size_t size() const { return slice_size_; }

        // Release the rdma registration
        void release();
    };
} // namespace S3
} // namespace Aws
