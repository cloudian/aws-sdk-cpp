/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/s3/S3_EXPORTS.h>
#include <aws/s3/model/RequestCharged.h>
#include <aws/s3/model/ServerSideEncryption.h>

#include <utility>

namespace Aws {
template <typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils {
namespace Xml {
class XmlDocument;
}  // namespace Xml
}  // namespace Utils
namespace S3 {
namespace Model {
class UploadPartRDMAResult {
 public:
  AWS_S3_API UploadPartRDMAResult() = default;
  AWS_S3_API UploadPartRDMAResult(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);
  AWS_S3_API UploadPartRDMAResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);

  ///@{
  /**
   * <p>The server-side encryption algorithm used when you store this object in
   * Amazon S3 or Amazon FSx.</p>  <p>When accessing data stored in Amazon FSx
   * file systems using S3 access points, the only valid server side encryption
   * option is <code>aws:fsx</code>.</p>
   */
  inline ServerSideEncryption GetServerSideEncryption() const { return m_serverSideEncryption; }
  inline void SetServerSideEncryption(ServerSideEncryption value) {
    m_serverSideEncryptionHasBeenSet = true;
    m_serverSideEncryption = value;
  }
  inline UploadPartRDMAResult& WithServerSideEncryption(ServerSideEncryption value) {
    SetServerSideEncryption(value);
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>Entity tag for the uploaded object.</p>
   */
  inline const Aws::String& GetETag() const { return m_eTag; }
  template <typename ETagT = Aws::String>
  void SetETag(ETagT&& value) {
    m_eTagHasBeenSet = true;
    m_eTag = std::forward<ETagT>(value);
  }
  template <typename ETagT = Aws::String>
  UploadPartRDMAResult& WithETag(ETagT&& value) {
    SetETag(std::forward<ETagT>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 32-bit <code>CRC32</code> checksum of the part. This will
   * only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumCRC32() const { return m_checksumCRC32; }
  template <typename ChecksumCRC32T = Aws::String>
  void SetChecksumCRC32(ChecksumCRC32T&& value) {
    m_checksumCRC32HasBeenSet = true;
    m_checksumCRC32 = std::forward<ChecksumCRC32T>(value);
  }
  template <typename ChecksumCRC32T = Aws::String>
  UploadPartRDMAResult& WithChecksumCRC32(ChecksumCRC32T&& value) {
    SetChecksumCRC32(std::forward<ChecksumCRC32T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 32-bit <code>CRC32C</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumCRC32C() const { return m_checksumCRC32C; }
  template <typename ChecksumCRC32CT = Aws::String>
  void SetChecksumCRC32C(ChecksumCRC32CT&& value) {
    m_checksumCRC32CHasBeenSet = true;
    m_checksumCRC32C = std::forward<ChecksumCRC32CT>(value);
  }
  template <typename ChecksumCRC32CT = Aws::String>
  UploadPartRDMAResult& WithChecksumCRC32C(ChecksumCRC32CT&& value) {
    SetChecksumCRC32C(std::forward<ChecksumCRC32CT>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 64-bit <code>CRC64NVME</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumCRC64NVME() const { return m_checksumCRC64NVME; }
  template <typename ChecksumCRC64NVMET = Aws::String>
  void SetChecksumCRC64NVME(ChecksumCRC64NVMET&& value) {
    m_checksumCRC64NVMEHasBeenSet = true;
    m_checksumCRC64NVME = std::forward<ChecksumCRC64NVMET>(value);
  }
  template <typename ChecksumCRC64NVMET = Aws::String>
  UploadPartRDMAResult& WithChecksumCRC64NVME(ChecksumCRC64NVMET&& value) {
    SetChecksumCRC64NVME(std::forward<ChecksumCRC64NVMET>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 160-bit <code>SHA1</code> checksum of the part. This will
   * only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumSHA1() const { return m_checksumSHA1; }
  template <typename ChecksumSHA1T = Aws::String>
  void SetChecksumSHA1(ChecksumSHA1T&& value) {
    m_checksumSHA1HasBeenSet = true;
    m_checksumSHA1 = std::forward<ChecksumSHA1T>(value);
  }
  template <typename ChecksumSHA1T = Aws::String>
  UploadPartRDMAResult& WithChecksumSHA1(ChecksumSHA1T&& value) {
    SetChecksumSHA1(std::forward<ChecksumSHA1T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 256-bit <code>SHA256</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumSHA256() const { return m_checksumSHA256; }
  template <typename ChecksumSHA256T = Aws::String>
  void SetChecksumSHA256(ChecksumSHA256T&& value) {
    m_checksumSHA256HasBeenSet = true;
    m_checksumSHA256 = std::forward<ChecksumSHA256T>(value);
  }
  template <typename ChecksumSHA256T = Aws::String>
  UploadPartRDMAResult& WithChecksumSHA256(ChecksumSHA256T&& value) {
    SetChecksumSHA256(std::forward<ChecksumSHA256T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 512-bit <code>SHA512</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumSHA512() const { return m_checksumSHA512; }
  template <typename ChecksumSHA512T = Aws::String>
  void SetChecksumSHA512(ChecksumSHA512T&& value) {
    m_checksumSHA512HasBeenSet = true;
    m_checksumSHA512 = std::forward<ChecksumSHA512T>(value);
  }
  template <typename ChecksumSHA512T = Aws::String>
  UploadPartRDMAResult& WithChecksumSHA512(ChecksumSHA512T&& value) {
    SetChecksumSHA512(std::forward<ChecksumSHA512T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 128-bit <code>MD5</code> checksum of the part. This will
   * only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumMD5() const { return m_checksumMD5; }
  template <typename ChecksumMD5T = Aws::String>
  void SetChecksumMD5(ChecksumMD5T&& value) {
    m_checksumMD5HasBeenSet = true;
    m_checksumMD5 = std::forward<ChecksumMD5T>(value);
  }
  template <typename ChecksumMD5T = Aws::String>
  UploadPartRDMAResult& WithChecksumMD5(ChecksumMD5T&& value) {
    SetChecksumMD5(std::forward<ChecksumMD5T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 64-bit <code>XXHASH64</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumXXHASH64() const { return m_checksumXXHASH64; }
  template <typename ChecksumXXHASH64T = Aws::String>
  void SetChecksumXXHASH64(ChecksumXXHASH64T&& value) {
    m_checksumXXHASH64HasBeenSet = true;
    m_checksumXXHASH64 = std::forward<ChecksumXXHASH64T>(value);
  }
  template <typename ChecksumXXHASH64T = Aws::String>
  UploadPartRDMAResult& WithChecksumXXHASH64(ChecksumXXHASH64T&& value) {
    SetChecksumXXHASH64(std::forward<ChecksumXXHASH64T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 64-bit <code>XXHASH3</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumXXHASH3() const { return m_checksumXXHASH3; }
  template <typename ChecksumXXHASH3T = Aws::String>
  void SetChecksumXXHASH3(ChecksumXXHASH3T&& value) {
    m_checksumXXHASH3HasBeenSet = true;
    m_checksumXXHASH3 = std::forward<ChecksumXXHASH3T>(value);
  }
  template <typename ChecksumXXHASH3T = Aws::String>
  UploadPartRDMAResult& WithChecksumXXHASH3(ChecksumXXHASH3T&& value) {
    SetChecksumXXHASH3(std::forward<ChecksumXXHASH3T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>The Base64 encoded, 128-bit <code>XXHASH128</code> checksum of the part. This
   * will only be present if the checksum was provided in the request. For more
   * information, see <a
   * href="https://docs.aws.amazon.com/AmazonS3/latest/userguide/checking-object-integrity.html">Checking
   * object integrity</a> in the <i>Amazon S3 User Guide</i>.</p>
   */
  inline const Aws::String& GetChecksumXXHASH128() const { return m_checksumXXHASH128; }
  template <typename ChecksumXXHASH128T = Aws::String>
  void SetChecksumXXHASH128(ChecksumXXHASH128T&& value) {
    m_checksumXXHASH128HasBeenSet = true;
    m_checksumXXHASH128 = std::forward<ChecksumXXHASH128T>(value);
  }
  template <typename ChecksumXXHASH128T = Aws::String>
  UploadPartRDMAResult& WithChecksumXXHASH128(ChecksumXXHASH128T&& value) {
    SetChecksumXXHASH128(std::forward<ChecksumXXHASH128T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>If server-side encryption with a customer-provided encryption key was
   * requested, the response will include this header to confirm the encryption
   * algorithm that's used.</p>  <p>This functionality is not supported for
   * directory buckets.</p>
   */
  inline const Aws::String& GetSSECustomerAlgorithm() const { return m_sSECustomerAlgorithm; }
  template <typename SSECustomerAlgorithmT = Aws::String>
  void SetSSECustomerAlgorithm(SSECustomerAlgorithmT&& value) {
    m_sSECustomerAlgorithmHasBeenSet = true;
    m_sSECustomerAlgorithm = std::forward<SSECustomerAlgorithmT>(value);
  }
  template <typename SSECustomerAlgorithmT = Aws::String>
  UploadPartRDMAResult& WithSSECustomerAlgorithm(SSECustomerAlgorithmT&& value) {
    SetSSECustomerAlgorithm(std::forward<SSECustomerAlgorithmT>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>If server-side encryption with a customer-provided encryption key was
   * requested, the response will include this header to provide the round-trip
   * message integrity verification of the customer-provided encryption key.</p>
   *  <p>This functionality is not supported for directory buckets.</p>
   */
  inline const Aws::String& GetSSECustomerKeyMD5() const { return m_sSECustomerKeyMD5; }
  template <typename SSECustomerKeyMD5T = Aws::String>
  void SetSSECustomerKeyMD5(SSECustomerKeyMD5T&& value) {
    m_sSECustomerKeyMD5HasBeenSet = true;
    m_sSECustomerKeyMD5 = std::forward<SSECustomerKeyMD5T>(value);
  }
  template <typename SSECustomerKeyMD5T = Aws::String>
  UploadPartRDMAResult& WithSSECustomerKeyMD5(SSECustomerKeyMD5T&& value) {
    SetSSECustomerKeyMD5(std::forward<SSECustomerKeyMD5T>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>If present, indicates the ID of the KMS key that was used for object
   * encryption.</p>
   */
  inline const Aws::String& GetSSEKMSKeyId() const { return m_sSEKMSKeyId; }
  template <typename SSEKMSKeyIdT = Aws::String>
  void SetSSEKMSKeyId(SSEKMSKeyIdT&& value) {
    m_sSEKMSKeyIdHasBeenSet = true;
    m_sSEKMSKeyId = std::forward<SSEKMSKeyIdT>(value);
  }
  template <typename SSEKMSKeyIdT = Aws::String>
  UploadPartRDMAResult& WithSSEKMSKeyId(SSEKMSKeyIdT&& value) {
    SetSSEKMSKeyId(std::forward<SSEKMSKeyIdT>(value));
    return *this;
  }
  ///@}

  ///@{
  /**
   * <p>Indicates whether the multipart upload uses an S3 Bucket Key for server-side
   * encryption with Key Management Service (KMS) keys (SSE-KMS).</p>
   */
  inline bool GetBucketKeyEnabled() const { return m_bucketKeyEnabled; }
  inline void SetBucketKeyEnabled(bool value) {
    m_bucketKeyEnabledHasBeenSet = true;
    m_bucketKeyEnabled = value;
  }
  inline UploadPartRDMAResult& WithBucketKeyEnabled(bool value) {
    SetBucketKeyEnabled(value);
    return *this;
  }
  ///@}

  ///@{

  inline RequestCharged GetRequestCharged() const { return m_requestCharged; }
  inline void SetRequestCharged(RequestCharged value) {
    m_requestChargedHasBeenSet = true;
    m_requestCharged = value;
  }
  inline UploadPartRDMAResult& WithRequestCharged(RequestCharged value) {
    SetRequestCharged(value);
    return *this;
  }
  ///@}

  ///@{

  inline int GetRDMAReply() const { return m_rDMAReply; }
  inline void SetRDMAReply(int value) {
    m_rDMAReplyHasBeenSet = true;
    m_rDMAReply = value;
  }
  inline UploadPartRDMAResult& WithRDMAReply(int value) {
    SetRDMAReply(value);
    return *this;
  }
  ///@}

  ///@{

  inline const Aws::String& GetRequestId() const { return m_requestId; }
  template <typename RequestIdT = Aws::String>
  void SetRequestId(RequestIdT&& value) {
    m_requestIdHasBeenSet = true;
    m_requestId = std::forward<RequestIdT>(value);
  }
  template <typename RequestIdT = Aws::String>
  UploadPartRDMAResult& WithRequestId(RequestIdT&& value) {
    SetRequestId(std::forward<RequestIdT>(value));
    return *this;
  }
  ///@}
  inline Aws::Http::HttpResponseCode GetHttpResponseCode() const { return m_HttpResponseCode; }

 private:
  ServerSideEncryption m_serverSideEncryption{ServerSideEncryption::NOT_SET};

  Aws::String m_eTag;

  Aws::String m_checksumCRC32;

  Aws::String m_checksumCRC32C;

  Aws::String m_checksumCRC64NVME;

  Aws::String m_checksumSHA1;

  Aws::String m_checksumSHA256;

  Aws::String m_checksumSHA512;

  Aws::String m_checksumMD5;

  Aws::String m_checksumXXHASH64;

  Aws::String m_checksumXXHASH3;

  Aws::String m_checksumXXHASH128;

  Aws::String m_sSECustomerAlgorithm;

  Aws::String m_sSECustomerKeyMD5;

  Aws::String m_sSEKMSKeyId;

  bool m_bucketKeyEnabled{false};

  RequestCharged m_requestCharged{RequestCharged::NOT_SET};

  int m_rDMAReply{0};

  Aws::String m_requestId;
  Aws::Http::HttpResponseCode m_HttpResponseCode;
  bool m_serverSideEncryptionHasBeenSet = false;
  bool m_eTagHasBeenSet = false;
  bool m_checksumCRC32HasBeenSet = false;
  bool m_checksumCRC32CHasBeenSet = false;
  bool m_checksumCRC64NVMEHasBeenSet = false;
  bool m_checksumSHA1HasBeenSet = false;
  bool m_checksumSHA256HasBeenSet = false;
  bool m_checksumSHA512HasBeenSet = false;
  bool m_checksumMD5HasBeenSet = false;
  bool m_checksumXXHASH64HasBeenSet = false;
  bool m_checksumXXHASH3HasBeenSet = false;
  bool m_checksumXXHASH128HasBeenSet = false;
  bool m_sSECustomerAlgorithmHasBeenSet = false;
  bool m_sSECustomerKeyMD5HasBeenSet = false;
  bool m_sSEKMSKeyIdHasBeenSet = false;
  bool m_bucketKeyEnabledHasBeenSet = false;
  bool m_requestChargedHasBeenSet = false;
  bool m_rDMAReplyHasBeenSet = false;
  bool m_requestIdHasBeenSet = false;
};

}  // namespace Model
}  // namespace S3
}  // namespace Aws
