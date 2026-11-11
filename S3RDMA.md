# RDMA support for S3 (cuobj)

This SDK is patched to add optional RDMA-accelerated `GetObject`, `PutObject`
and `UploadPart` operations for S3, using NVIDIA's `cuobj`/GPUDirect Storage
client library (`libcuobjclient.so`), distributed in the [CUDA Toolkit](https://developer.nvidia.com/cuda/toolkit)

This document covers building and using the S3-RDMA enhancements; everything else is covered by the top-level
[README](README.md).

## Build options

Building the S3 SDK requires the Nvidia `cuobjclient.h` include file to be available. Installing the NVIDIA CUDA Toolkit (13.1.1 or later) will satisfy this requirement.

Additional CMake build options:


| CMake option | Description | Default |
|---|---|---|
| `NVIDIA_CUOBJ_INCLUDE_DIR` | Directory containing NVIDIA's `cuobjclient.h`. Auto-detected via the CUDA toolkit's own include directories; only needs to be set explicitly if GDS is installed somewhere else. | auto-detected |
| `S3_CLIENT_DISABLE_TRANSPARENT_RDMA` | Disable transparent RDMA in `GetObject`/`PutObject`/`UploadPart`; always use plain TCP for those operations. | `OFF` |

## S3Client RDMA features

The S3Client adds three new methods, `GetObjectRDMA`, `PutObjectRDMA` and `UploadPartRDMA` for directly uploading/downloading host/GPU memory buffers. `GetObjectTCP`, `PutObjectTCP` and `UploadPartTCP` are the original get/put/upload part functions, always using TCP to transfer object contents.

The `GetObject`, `PutObject` and `UploadPart` methods are enhanced 
to perform data transfers over RDMA when appropriate, using a pool of pre-registered host memory buffers for RDMA transfers.

For `GetObject` we:
* Get a buffer from the pool and issue a `GetObjectRDMA` to read data into it.
* Copy the contents to the destination stream.
* If the get request covers more data than fits in the buffer, issue further
  ranged `GetObjectRDMA` requests to read the remainder, copying to the
  destination stream as each arrives.

For `PutObject`/`UploadPart` we:
* Determine the body size.
  * If it's above `S3RDMA_MPU_THRESHOLD_BYTES`, switch to a multipart upload
    instead (`PutObject` only - see
    [Environment variables](#environment-variables) below), issuing
    `UploadPartRDMA` for each part.
  * If it's too large to fit in an RDMA buffer, or below
    `S3RDMA_THRESHOLD_BYTES`, fall back to `PutObjectTCP`/`UploadPartTCP` to
    send the data in the HTTP body.
* Otherwise, get a buffer from the pool and `read()` the body into it.
* Issue a `PutObjectRDMA`/`UploadPartRDMA` to write the data.

Example `GetObject` (no code changes required beyond linking against this
SDK - RDMA acceleration, when available, is transparent):

```C++
Aws::Client::ClientConfiguration config;
config.endpointOverride = "http://s3-region-1.my-cloudian-hyperstore.com";
config.region = "region1";
const auto sign_payloads = Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never;
const auto use_virtual_addressing = false;
auto client = Aws::S3::S3Client(config, sign_payloads, use_virtual_addressing);

Aws::S3::Model::GetObjectRequest getRequest;
getRequest.SetBucket("test");
getRequest.SetKey("test");
auto rsp = client.GetObject(getRequest);
assert(rsp.IsSuccess());
```

## Using the RDMA API directly

`GetObjectRDMA`, `PutObjectRDMA` and `UploadPartRDMA` each come in two forms:
a `(request, void* ptr, size_t len)` overload and a `(request, const RdmaPtr&)`
overload - the former is a convenience wrapper that just constructs an
`RdmaPtr` around your pointer and forwards to the latter. Which one to use
depends on how the memory you're transferring is managed:

### Passing a raw pointer

The simplest form is to pass a raw pointer and length to a host/GPU memory region
and let the S3Client handle the buffer registration and deregistration:

```C++
auto dst = std::unique_ptr<char[]>(new char[64 * 1024]);

Aws::S3::Model::GetObjectRDMARequest getRequest;
getRequest.SetBucket("test");
getRequest.SetKey("test");
auto rsp = client.GetObjectRDMA(getRequest, dst.get(), 64 * 1024);
assert(rsp.IsSuccess());
```

```C++
std::string body = "some payload";

Aws::S3::Model::PutObjectRDMARequest putRequest;
putRequest.SetBucket("test");
putRequest.SetKey("test");
auto rsp = client.PutObjectRDMA(putRequest, body.data(), body.size());
assert(rsp.IsSuccess());
```

Under the hood this calls cuobjclient functions to register `dst`/`body.data()` for RDMA (`RdmaPtr`'s
constructor) before the call and deregister (`RdmaPtr`'s destructor)
immediately after to pin and unpin that memory for the RDMA NIC.

### Passing a pre-registered `RdmaPtr`

RdmaPtr uses RAII to keep the memory registered for the lifetime of the ptr. 
This allows you to retain ownership of the registration, reusing the buffer for multiple RDMA 
transfers, thus amortizing the cost of the memory registration.

```C++
Aws::S3::RdmaPtr buf(myBuffer, myBufferSize);
if (!buf) {
    // Registration failed - e.g. myBufferSize exceeds what the RDMA
    // hardware/driver can register in one region, or RDMA isn't available.
}

for (auto &range : rangesToFetch) {
    // Describes a sub-region of the *same* registration - no additional
    // registration cost per slice.
    auto slice = buf.slice(range.offset, range.length);

    Aws::S3::Model::GetObjectRDMARequest getRequest;
    getRequest.SetBucket("test");
    getRequest.SetKey("test");
    getRequest.SetRange(range.toHttpRangeHeader());
    auto rsp = client.GetObjectRDMA(getRequest, slice);
    assert(rsp.IsSuccess());
}
// buf deregisters automatically here, once it goes out of scope.
```

`RdmaPtr::slice(offset, size)` returns a new `RdmaPtr` that describes a region 
within the parent region, without requiring additional memory registration, so a single
long-lived buffer can serve many concurrent or sequential transfers into
different parts of itself. The original `RdmaPtr` must outlive every slice
taken from it.

### Using the buffer pool

The client creates a pool of pre-registered buffers (the pool grows on demand to a defined limit).
This pool is used by the `GetObject`, `PutObject` and `UploadPart` handlers, but is also available
for use by applications directly.

```C++
auto buf = client.get_rdma_buffer(); // default pool buffer size

Aws::S3::Model::GetObjectRDMARequest getRequest;
getRequest.SetBucket("test");
getRequest.SetKey("test");
auto rsp = client.GetObjectRDMA(getRequest, buf.ptr());
if (rsp.IsSuccess()) {
    buf.trim(rsp.GetResult().GetRDMABytesTransferred());
    // ... use buf.data()/buf.size() ...
}
// buf returns itself to the pool here, once it goes out of scope, ready for
// the next caller to borrow with no further registration cost.
```

The pool's capacity and per-buffer size are configured via
`S3RDMA_BUFFER_POOL_CAPACITY`/`S3RDMA_BUFFER_POOL_BUFSIZE_MIB` (see
[Environment variables](#environment-variables) below), or at runtime via the
client's `resize_buffer_pool()`. Requesting a buffer larger than the pool's
configured size (`get_rdma_buffer(sz)` with `sz` above that size) still
works, but that particular buffer is registered and deregistered just for
that one call and isn't returned to the pool.

## Known issues

### Minimal changes, but a binary-compatibility caveat

No code changes are required to use the RDMA functionality, but you must
rebuild and link your application against this SDK: `S3Client`'s memory
layout changes (it gains a new base class), so it is not ABI-compatible with
upstream's despite exporting the same symbols - to prevent an application
from accidentally loading the wrong one, which would cause undefined
behaviour rather than a clean failure, this SDK's S3 client library is named
`libcloudian-aws-cpp-sdk-s3.so` instead of upstream's `libaws-cpp-sdk-s3.so`.

### Data event handlers

Registered data sent/received handler callback behavior differs depending on
whether data is transferred over TCP or RDMA:
* TCP - handlers are called back in the normal way as data is transferred.
* RDMA - for a single `GetObjectRDMA`/`PutObjectRDMA`/`UploadPartRDMA` call,
  the handler is called exactly once, when all data for that call has been
  sent/received. The `HttpRequest` and `HttpResponse` parameters are not
  valid (set to `nullptr`). When uploading objects there is no chunking or
  trailing headers overhead, so the reported transfer size will exactly
  match the amount sent/received in that call.

  The transparent `GetObject`/`PutObject`/`UploadPart` wrappers may issue
  several such calls internally - multiple ranged reads for an object larger
  than one RDMA buffer, or multiple parts for a multipart upload - in which
  case the handler fires once per underlying call, with that call's byte
  count, not once for the operation's total. Accumulate across calls
  (`xfer += bytesTransferred`) if you need the running total.

### Checksums

If a checksum algorithm is specified in the `PutObjectRDMA`/`UploadPartRDMA` request then the client
will calculate the checksum, if the memory buffer is in host memory. Checksums will not be calculated for GPU transfers -
applications must pre-calculate the checksum and include it in the request.

## Environment variables

Several environment variables control the behavior of the RDMA S3 client:

| Name | Description | Default |
|---|---|---|
| `S3RDMA_CLIENT_ALWAYS_USE_TCP` | Set to true to disable RDMA - the get/put/upload RDMA handlers will switch to using the HTTP body for transferring contents | `false` |
| `S3RDMA_CLIENT_NATIVE_IO` | Alias for `S3RDMA_CLIENT_ALWAYS_USE_TCP` - setting either one disables RDMA | `false` |
| `S3RDMA_BUFFER_POOL_CAPACITY` | The maximum number of RDMA bounce buffers the pool retains for reuse (buffers are allocated lazily, on demand - not pre-allocated) | `128` |
| `S3RDMA_BUFFER_POOL_BUFSIZE_MIB` | The size of RDMA bounce buffers in mebibytes | `10` |
| `S3RDMA_CONCURRENT_READS` | The maximum number of concurrent reads `GetObject` should issue when reading a large object into RDMA bounce buffers | `4` |
| `S3RDMA_THRESHOLD_BYTES` | The threshold for using RDMA to transfer data (`GetObject` will always use RDMA, since it doesn't know the object size upfront) | `1048576` (1MiB) |
| `S3RDMA_MPU_THRESHOLD_BYTES` | The threshold for `PutObject`/`PutObjectRDMA` switching to multipart uploads for larger buffers | `104857600` (100MiB) |
| `S3RDMA_MAX_CONCURRENT_MPU_UPLOADS` | The maximum number of concurrent upload part requests when using multipart uploads | `4` |
| `S3RDMA_CONCURRENT_MPU_UPLOAD_PART_SIZE_MIB` | The size of parts when using multipart uploads | `10` |
