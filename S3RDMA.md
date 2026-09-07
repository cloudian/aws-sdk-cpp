# RDMA support for S3 (cuobj)

This SDK is patched to add optional RDMA-accelerated `GetObject`, `PutObject`
and `UploadPart` operations for S3, using NVIDIA's `cuobj`/GPUDirect Storage
client library (`libcuobjclient.so`). This document covers the RDMA-specific
build options, API additions, and runtime behavior; everything else about
building and using this SDK is unchanged from upstream - see the top-level
[README](README.md).

## Build options

RDMA support adds no new required dependencies at build time beyond the
NVIDIA CUDA Toolkit (13.1.1 or later), which must already be installed and
provide the GPUDirect Storage (GDS) `cuobjclient.h` header - included in a
standard toolkit install. `libcuobjclient.so` itself is only `dlopen()`'d at
runtime; there is no build-time link dependency on it, so it does not need to
be present at build time, and the SDK falls back transparently to plain TCP
if it's absent (or no RDMA server is reachable) at runtime.

| CMake option | Description | Default |
|---|---|---|
| `NVIDIA_CUOBJ_INCLUDE_DIR` | Directory containing NVIDIA's `cuobjclient.h`. Auto-detected via the CUDA toolkit's own include directories; only needs to be set explicitly if GDS is installed somewhere else. | auto-detected |
| `S3_CLIENT_DISABLE_TRANSPARENT_RDMA` | Disable transparent RDMA in `GetObject`/`PutObject`/`UploadPart`; always use plain TCP for those operations (`GetObjectRDMA`/`PutObjectRDMA`/`UploadPartRDMA` remain available and unaffected). | `OFF` |

## What gets added

The generated `S3Client` is patched to derive from a new mixin class
`S3ClientRDMA`, with `GetObject`, `PutObject` and `UploadPart` renamed to
`GetObjectTCP`, `PutObjectTCP`, and `UploadPartTCP`. `S3ClientRDMA` then
implements its own `GetObject`, `PutObject` and `UploadPart` that use RDMA
acceleration when possible, falling back to the renamed `...TCP` methods
otherwise (see [Known issues](#known-issues) below on the memory-layout
implication of this).

Three additional operations are added, equivalent to their native
counterparts except that object contents are transferred over RDMA instead
of in the HTTP body: `GetObjectRDMA`, `PutObjectRDMA`, `UploadPartRDMA` (each
with the usual `...Async`/`...Callable` variants). Programmers may call these
directly, providing the buffer to transfer, to reduce the number of memory
copies required - e.g. for `GetObject`, reading directly into a caller-owned
buffer rather than copying from the TCP response stream.

For `GetObject` we:
* Get a buffer from the pool and issue a `GetObjectRDMA` to fill it.
* Copy the contents to the destination stream (instantiated from the
  request's stream factory).
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
* Issue a `PutObjectRDMA`/`UploadPartRDMA` to transfer the data.

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
`RdmaPtr` around your pointer and forwards to the latter. Which one to reach
for depends on how the memory you're transferring is managed:

### Passing a raw pointer

The simplest form, and a reasonable default for occasional or one-off
transfers:

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
For an occasional call that's a fine tradeoff for the convenience; for a
buffer you're transferring to/from repeatedly, that per-call registration
cost is worth avoiding - see the next two sections.

### Passing a pre-registered `RdmaPtr`

Construct an `RdmaPtr` yourself, and it stays registered for as long as the
`RdmaPtr` object lives, rather than just for the duration of a single call -
so you can issue many transfers against it without paying to register the
memory again each time:

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

`RdmaPtr::slice(offset, size)` is the tool for this: it returns a new
`RdmaPtr` describing a sub-window of an already-registered region, sharing
that region's registration rather than creating a new one - so a single
long-lived buffer can serve many concurrent or sequential transfers into
different parts of itself. The original `RdmaPtr` must outlive every slice
taken from it.

This is the right tool when you own a buffer with a lifetime and identity
that outlives any single S3 call - e.g. a scratch/staging area your
application manages itself, separate from the SDK's own buffer pool.

### Using the buffer pool

For the common case of a short-lived, fungible buffer - you don't care
*which* memory backs a given transfer, only that it's there for the duration
of one call - borrow one from the client's own pool of buffers, which are
already registered once and reused:

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

This is exactly the mechanism the SDK's own transparent `GetObject`,
`PutObject` and `UploadPart` implementations use internally (see
[What gets added](#what-gets-added) above) - borrow a buffer, issue the RDMA
transfer, let it return itself to the pool. Reaching for it directly gets you
that same behavior for your own transfers: the pool starts empty and
allocates/registers a buffer the first time it's needed (or once the pool is
exhausted), but every buffer it hands out afterward is reused already
registered, with no further registration cost.

The pool's capacity and per-buffer size are configured via
`S3RDMA_BUFFER_POOL_CAPACITY`/`S3RDMA_BUFFER_POOL_BUFSIZE_MIB` (see
[Environment variables](#environment-variables) below), or at runtime via the
client's `resize_buffer_pool()`. Requesting a buffer larger than the pool's
configured size (`get_rdma_buffer(sz)` with `sz` above that size) still
works, but that particular buffer is registered and deregistered just for
that one call and isn't returned to the pool - the same cost profile as the
raw-pointer form above, just with RAII cleanup.

### Summary

| Approach | Registration cost | Best for |
|---|---|---|
| Raw pointer (`void*`/`size_t`) | Once per call | Occasional, one-off transfers |
| Pre-registered `RdmaPtr` (optionally `.slice()`d) | Once for the `RdmaPtr`'s whole lifetime | A long-lived buffer you manage yourself, used for many transfers |
| Buffer pool (`get_rdma_buffer()`) | Once per buffer, the first time it's allocated (then reused with no further cost) | Short-lived, fungible buffers - the common case |

## Known issues

### Minimal changes, but a binary-compatibility caveat

No code changes are required to use the RDMA functionality, but you must
rebuild and link your application against this SDK. Because `S3Client`'s
memory layout changes (it gains a new base class), simply replacing the
application's S3 shared object without recompiling against the changed
`S3Client.h` will result in undefined behaviour.

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
