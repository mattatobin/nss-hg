/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "pk11pub.h"
#include "tls13hkdf.h"
#include <memory>

#include "databuffer.h"
#include "gtest_utils.h"
#include "scoped_ptrs.h"

namespace nss_test {

const uint8_t kKey1Data[] = {
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
};
const DataBuffer kKey1(kKey1Data,
                       sizeof(kKey1Data));

// The same as key1 but with the first byte
// 0x01.
const uint8_t kKey2Data[] = {
  0x01,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
  0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
  0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
  0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
};
const DataBuffer kKey2(kKey2Data,
                       sizeof(kKey2Data));

const char kLabelMasterSecret[] = "master secret";

const uint8_t kSessionHash[] = {
  0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
  0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff,
  0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
  0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
  0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,
  0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf
};

const size_t kHashLength[] = {
  0,  /* ssl_hash_none   */
  16, /* ssl_hash_md5    */
  20, /* ssl_hash_sha1   */
  28, /* ssl_hash_sha224 */
  32, /* ssl_hash_sha256 */
  48, /* ssl_hash_sha384 */
  64, /* ssl_hash_sha512 */
};

static void ImportKey(ScopedPK11SymKey* to, const DataBuffer& key,
                      PK11SlotInfo* slot) {
  SECItem key_item = {
    siBuffer,
    const_cast<uint8_t*>(key.data()),
    static_cast<unsigned int>(key.len())
  };

  PK11SymKey* inner =
      PK11_ImportSymKey(slot, CKM_SSL3_MASTER_KEY_DERIVE, PK11_OriginUnwrap,
                        CKA_DERIVE, &key_item, NULL);
  ASSERT_NE(nullptr, inner);
  to->reset(inner);
}


static void DumpData(const std::string& label, const uint8_t* buf, size_t len) {
  DataBuffer d(buf, len);

  std::cerr << label << ": " << d << std::endl;
}

void DumpKey(const std::string& label, ScopedPK11SymKey& key) {
  SECStatus rv = PK11_ExtractKeyValue(key.get());
  ASSERT_EQ(SECSuccess, rv);

  SECItem *key_data = PK11_GetKeyData(key.get());
  ASSERT_NE(nullptr, key_data);

  DumpData(label, key_data->data, key_data->len);
}

class TlsHkdfTest
  : public ::testing::Test,
    public ::testing::WithParamInterface<SSLHashType> {
 public:
  TlsHkdfTest()
    : k1_(),
      k2_(),
      hash_type_(GetParam()),
      slot_(PK11_GetInternalSlot()) {
    EXPECT_NE(nullptr, slot_);
  }

  void SetUp() {
    ImportKey(&k1_, kKey1, slot_.get());
    ImportKey(&k2_, kKey2, slot_.get());
  }

  void VerifyKey(const ScopedPK11SymKey& key, const DataBuffer& expected) {
    SECStatus rv = PK11_ExtractKeyValue(key.get());
    ASSERT_EQ(SECSuccess, rv);

    SECItem *key_data = PK11_GetKeyData(key.get());
    ASSERT_NE(nullptr, key_data);

    EXPECT_EQ(expected.len(), key_data->len);
    EXPECT_EQ(0, memcmp(expected.data(),
                        key_data->data, expected.len()));
  }

  void HkdfExtract(const ScopedPK11SymKey& ikmk1, const ScopedPK11SymKey& ikmk2,
                   SSLHashType base_hash,
                   const DataBuffer& expected) {
    PK11SymKey* prk = nullptr;
    SECStatus rv = tls13_HkdfExtract(
        ikmk1.get(), ikmk2.get(), base_hash, &prk);
    ASSERT_EQ(SECSuccess, rv);
    ScopedPK11SymKey prkk(prk);

    DumpKey("Output", prkk);
    VerifyKey(prkk, expected);
  }

  void HkdfExpandLabel(ScopedPK11SymKey* prk, SSLHashType base_hash,
                       const uint8_t *session_hash, size_t session_hash_len,
                       const char *label, size_t label_len,
                       const DataBuffer& expected) {
    std::vector<uint8_t> output(expected.len());

    SECStatus rv = tls13_HkdfExpandLabelRaw(prk->get(), base_hash,
                                            session_hash, session_hash_len,
                                            label, label_len,
                                            &output[0], output.size());
    ASSERT_EQ(SECSuccess, rv);
    DumpData("Output", &output[0], output.size());
    EXPECT_EQ(0, memcmp(expected.data(), &output[0],
                        expected.len()));
  }

 protected:
  ScopedPK11SymKey k1_;
  ScopedPK11SymKey k2_;
  SSLHashType hash_type_;

 private:
  ScopedPK11SlotInfo slot_;
};


TEST_P(TlsHkdfTest, HkdfKey2Only) {
  const uint8_t tv[][48] = {
    { /* ssl_hash_none   */ },
    { /* ssl_hash_md5    */ },
    { /* ssl_hash_sha1   */ },
    { /* ssl_hash_sha224 */ },
    { 0x2f, 0x5f, 0x78, 0xd0, 0xa4, 0xc4, 0x36, 0xee,
      0x6c, 0x8a, 0x4e, 0xf9, 0xd0, 0x43, 0x81, 0x02,
      0x13, 0xfd, 0x47, 0x83, 0x63, 0x3a, 0xd2, 0xe1,
      0x40, 0x6d, 0x2d, 0x98, 0x00, 0xfd, 0xc1, 0x87, },
    { 0x7b, 0x40, 0xf9, 0xef, 0x91, 0xff, 0xc9, 0xd1,
      0x29, 0x24, 0x5c, 0xbf, 0xf8, 0x82, 0x76, 0x68,
      0xae, 0x4b, 0x63, 0xe8, 0x03, 0xdd, 0x39, 0xa8,
      0xd4, 0x6a, 0xf6, 0xe5, 0xec, 0xea, 0xf8, 0x7d,
      0x91, 0x71, 0x81, 0xf1, 0xdb, 0x3b, 0xaf, 0xbf,
      0xde, 0x71, 0x61, 0x15, 0xeb, 0xb5, 0x5f, 0x68  }
  };

  const DataBuffer expected_data(tv[hash_type_], kHashLength[hash_type_]);
  HkdfExtract(nullptr, k2_, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfKey1Key2) {
  const uint8_t tv[][48] = {
    { /* ssl_hash_none   */ },
    { /* ssl_hash_md5    */ },
    { /* ssl_hash_sha1   */ },
    { /* ssl_hash_sha224 */ },
    { 0x79, 0x53, 0xb8, 0xdd, 0x6b, 0x98, 0xce, 0x00,
      0xb7, 0xdc, 0xe8, 0x03, 0x70, 0x8c, 0xe3, 0xac,
      0x06, 0x8b, 0x22, 0xfd, 0x0e, 0x34, 0x48, 0xe6,
      0xe5, 0xe0, 0x8a, 0xd6, 0x16, 0x18, 0xe5, 0x48, },
    { 0x01, 0x93, 0xc0, 0x07, 0x3f, 0x6a, 0x83, 0x0e,
      0x2e, 0x4f, 0xb2, 0x58, 0xe4, 0x00, 0x08, 0x5c,
      0x68, 0x9c, 0x37, 0x32, 0x00, 0x37, 0xff, 0xc3,
      0x1c, 0x5b, 0x98, 0x0b, 0x02, 0x92, 0x3f, 0xfd,
      0x73, 0x5a, 0x6f, 0x2a, 0x95, 0xa3, 0xee, 0xf6,
      0xd6, 0x8e, 0x6f, 0x86, 0xea, 0x63, 0xf8, 0x33  }
  };

  const DataBuffer expected_data(tv[hash_type_], kHashLength[hash_type_]);
  HkdfExtract(k1_, k2_, hash_type_, expected_data);
}

TEST_P(TlsHkdfTest, HkdfExpandLabel) {
  const uint8_t tv[][48] = {
    { /* ssl_hash_none   */ },
    { /* ssl_hash_md5    */ },
    { /* ssl_hash_sha1   */ },
    { /* ssl_hash_sha224 */ },
    { 0x34, 0x7c, 0x67, 0x80, 0xff, 0x0b, 0xba, 0xd7,
      0x1c, 0x28, 0x3b, 0x16, 0xeb, 0x2f, 0x9c, 0xf6,
      0x2d, 0x24, 0xe6, 0xcd, 0xb6, 0x13, 0xd5, 0x17,
      0x76, 0x54, 0x8c, 0xb0, 0x7d, 0xcd, 0xe7, 0x4c, },
    { 0x4b, 0x1e, 0x5e, 0xc1, 0x49, 0x30, 0x78, 0xea,
      0x35, 0xbd, 0x3f, 0x01, 0x04, 0xe6, 0x1a, 0xea,
      0x14, 0xcc, 0x18, 0x2a, 0xd1, 0xc4, 0x76, 0x21,
      0xc4, 0x64, 0xc0, 0x4e, 0x4b, 0x36, 0x16, 0x05,
      0x6f, 0x04, 0xab, 0xe9, 0x43, 0xb1, 0x2d, 0xa8,
      0xa7, 0x17, 0x9a, 0x5f, 0x09, 0x91, 0x7d, 0x1f  }
  };

  const DataBuffer expected_data(tv[hash_type_], kHashLength[hash_type_]);
  HkdfExpandLabel(&k1_, hash_type_, kSessionHash, kHashLength[hash_type_],
                  kLabelMasterSecret, strlen(kLabelMasterSecret),
                  expected_data);
}

static const SSLHashType kHashTypes[] = {ssl_hash_sha256, ssl_hash_sha384};
INSTANTIATE_TEST_CASE_P(AllHashFuncs, TlsHkdfTest,
                        ::testing::ValuesIn(kHashTypes));

} // namespace nss_test
