/**
 * Copyright Cloudian, Inc. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once

#include "aws/s3/RdmaPtr.h"

#include <cassert>
#include <mutex>
#include <vector>

namespace Aws
{
namespace S3
{

/* 
 * A RDMA registered buffer allocated from the pool.
 * The destructor returns the buffer to the pool
*/
class RdmaBuffer {
    friend class RdmaBufferPool;

    char* buf_;
    size_t size_;
    size_t capacity_;
    class RdmaBufferPool* pool_;

    RdmaBuffer(char* buf, size_t size, RdmaBufferPool* pool) : buf_{buf}, size_(size), capacity_(size), pool_{pool} {}
    void returnBuf();

public:
    RdmaBuffer(): buf_{nullptr}, size_(0), capacity_(0), pool_(nullptr) {}
    
    // Buffer can be moved but not copied
    RdmaBuffer(RdmaBuffer&& rhs) : buf_(rhs.buf_), size_(rhs.size_), capacity_(rhs.capacity_), pool_(rhs.pool_) {
        rhs.buf_ = nullptr;
    };
    RdmaBuffer(RdmaBuffer& rhs) = delete;

    ~RdmaBuffer();
    
    RdmaBuffer& operator=(RdmaBuffer&& rhs);

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    const char* data() const { return buf_; }
    char* data() { return buf_; }

    // Trim this buffer to match the number of bytes read
    void trim(size_t sz) {
        assert(sz <= capacity_);
        size_ = sz;
    }

    // Get an RDMAPtr for this buffer.
    // The buffer should not be destructed while a ptr is in scope
    RdmaPtr ptr() const {
        return RdmaPtr(buf_, size_, true);
    }
};

/*
 * A pool of RDMA registered buffers.
 * The pool starts off empty and grows to maxPoolSize
*/
class RdmaBufferPool {
    std::mutex m_;
    size_t maxPoolSize_;
    size_t bufferSize_;
    std::vector<char*> pool_;
    size_t nBuffers_;

    private:
        friend class RdmaBuffer;

        // Create a buffer, registering it for RDMA
        char* allocate_buffer(size_t sz);
        // Free a buffer, first deregistering it for RDMA
        void free_buffer(char* buf);

        // Return a buffer to the pool (or free it if pool is full)
        // Called by Buffer destructor
        void put_buffer(char* buf) {
            {
                std::lock_guard<std::mutex> lock(m_);
                if (nBuffers_ < maxPoolSize_) {
                    pool_[nBuffers_] = buf;
                    nBuffers_ += 1;
                    return;
                }
            }
            // Pool is full, destroy buffer
            free_buffer(buf);
        }

    public:
        RdmaBufferPool() : maxPoolSize_{defaultMaxPoolSize()}, bufferSize_{defaultBufferSize()}, pool_{maxPoolSize_}, nBuffers_{0} {
        }

        ~RdmaBufferPool() {
            for (size_t i = 0; i < nBuffers_; i++) {
                free_buffer(pool_[i]);
            }
        }

        size_t buffer_size() const {
            return bufferSize_;
        }

        size_t capacity() const {
            return maxPoolSize_;
        }

        size_t available() const {
            return nBuffers_;
        }

        void resize(size_t capacity, size_t bufferSize) {
            std::lock_guard<std::mutex> lock(m_);
            for (size_t i = 0; i < nBuffers_; i++) {
                free_buffer(pool_[i]);
                pool_[i] = nullptr;
            }
            maxPoolSize_ = capacity;
            bufferSize_ = bufferSize;
            nBuffers_ = 0;
            pool_.resize(capacity);
        }

        // Get a buffer from the pool, allocating a new buffer if the pool is empty
        RdmaBuffer get_buffer(size_t size) {
            if (size == 0) {
                size = bufferSize_;
            }
            if (size <= bufferSize_) {
                std::lock_guard<std::mutex> lock(m_);
                if (nBuffers_) {
                    nBuffers_ -= 1;
                    return RdmaBuffer(pool_[nBuffers_], size, this);
                }
            }
            // No buffers in the pool or size is greater than pool size - we'll allocate one instead
            return RdmaBuffer(allocate_buffer(std::max(size, bufferSize_)), size, this);
        }


        static size_t defaultMaxPoolSize();
        static size_t defaultBufferSize();
};

inline void RdmaBuffer::returnBuf() {
    if (buf_) {
        if (capacity_ > pool_->bufferSize_) {
            pool_->free_buffer(buf_);
        } else {
            pool_->put_buffer(buf_);
        }
    }
}

inline RdmaBuffer::~RdmaBuffer() {
    returnBuf();
}

inline RdmaBuffer& RdmaBuffer::operator=(RdmaBuffer&& rhs) {
    returnBuf();
    this->buf_ = rhs.buf_;
    this->size_ = rhs.size_;
    this->pool_ = rhs.pool_;
    rhs.buf_ = nullptr;
    return *this;
}

} // namespace S3
} // namespace Aws