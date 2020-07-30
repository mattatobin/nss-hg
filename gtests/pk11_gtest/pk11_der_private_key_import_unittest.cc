/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "secutil.h"

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

namespace nss_test {

const std::vector<uint8_t> kValidP256Key = {
    0x30, 0x81, 0x87, 0x02, 0x01, 0x00, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
    0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
    0x03, 0x01, 0x07, 0x04, 0x6d, 0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20,
    0xc9, 0xaf, 0xa9, 0xd8, 0x45, 0xba, 0x75, 0x16, 0x6b, 0x5c, 0x21, 0x57,
    0x67, 0xb1, 0xd6, 0x93, 0x4e, 0x50, 0xc3, 0xdb, 0x36, 0xe8, 0x9b, 0x12,
    0x7b, 0x8a, 0x62, 0x2b, 0x12, 0x0f, 0x67, 0x21, 0xa1, 0x44, 0x03, 0x42,
    0x00, 0x04, 0x60, 0xfe, 0xd4, 0xba, 0x25, 0x5a, 0x9d, 0x31, 0xc9, 0x61,
    0xeb, 0x74, 0xc6, 0x35, 0x6d, 0x68, 0xc0, 0x49, 0xb8, 0x92, 0x3b, 0x61,
    0xfa, 0x6c, 0xe6, 0x69, 0x62, 0x2e, 0x60, 0xf2, 0x9f, 0xb6, 0x79, 0x03,
    0xfe, 0x10, 0x08, 0xb8, 0xbc, 0x99, 0xa4, 0x1a, 0xe9, 0xe9, 0x56, 0x28,
    0xbc, 0x64, 0xf2, 0xf1, 0xb2, 0x0c, 0x2d, 0x7e, 0x9f, 0x51, 0x77, 0xa3,
    0xc2, 0x94, 0xd4, 0x46, 0x22, 0x99};

const std::vector<uint8_t> kValidRSAKey = {
    // 512-bit RSA private key (PKCS#8)
    0x30, 0x82, 0x01, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3e, 0x30, 0x82, 0x01, 0x3a, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xa2, 0x40, 0xce, 0xb5, 0x4e, 0x70, 0xdc, 0x14, 0x82, 0x5b, 0x58, 0x7d,
    0x2f, 0x5d, 0xfd, 0x46, 0x3c, 0x4b, 0x82, 0x50, 0xb6, 0x96, 0x00, 0x4a,
    0x1a, 0xca, 0xaf, 0xe4, 0x9b, 0xcf, 0x38, 0x4a, 0x46, 0xaa, 0x9f, 0xb4,
    0xd9, 0xc7, 0xee, 0x88, 0xe9, 0xef, 0x0a, 0x31, 0x5f, 0x53, 0x86, 0x8f,
    0x63, 0x68, 0x0b, 0x58, 0x34, 0x72, 0x49, 0xba, 0xed, 0xd9, 0x34, 0x15,
    0x16, 0xc4, 0xca, 0xb7, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x34,
    0xe6, 0xdc, 0x7e, 0xd0, 0xec, 0x8b, 0x55, 0x44, 0x8b, 0x73, 0xf6, 0x9d,
    0x13, 0x10, 0x19, 0x6e, 0x5f, 0x50, 0x45, 0xf0, 0xc2, 0x47, 0xa5, 0xe1,
    0xc6, 0x64, 0x43, 0x2d, 0x6a, 0x0a, 0xf7, 0xe7, 0xda, 0x40, 0xb8, 0x3a,
    0xf0, 0x47, 0xdd, 0x01, 0xf5, 0xe0, 0xa9, 0x0e, 0x47, 0xc2, 0x24, 0xd7,
    0xb5, 0x13, 0x3a, 0x35, 0x4d, 0x11, 0xaa, 0x50, 0x03, 0xb3, 0xe8, 0x54,
    0x6c, 0x99, 0x01, 0x02, 0x21, 0x00, 0xcd, 0xb2, 0xd7, 0xa7, 0x43, 0x5b,
    0xcb, 0x45, 0xe5, 0x0e, 0x86, 0xf6, 0xc1, 0x4e, 0x97, 0xed, 0x78, 0x1f,
    0x09, 0x56, 0xcd, 0x26, 0xe6, 0xf7, 0x5e, 0xd9, 0xfc, 0x88, 0x12, 0x5f,
    0x84, 0x07, 0x02, 0x21, 0x00, 0xc9, 0xee, 0x30, 0xaf, 0x6c, 0xb9, 0x5a,
    0xc9, 0xc1, 0x14, 0x9e, 0xd8, 0x4b, 0x33, 0x38, 0x48, 0x17, 0x41, 0x35,
    0x94, 0x09, 0xf3, 0x69, 0xc4, 0x97, 0xbe, 0x17, 0x7d, 0x95, 0x0f, 0xb7,
    0xd1, 0x02, 0x21, 0x00, 0x8b, 0x0e, 0xf9, 0x8d, 0x61, 0x13, 0x20, 0x63,
    0x9b, 0x0b, 0x6c, 0x20, 0x4a, 0xe4, 0xa7, 0xfe, 0xe8, 0xf3, 0x0a, 0x6c,
    0x3c, 0xfa, 0xac, 0xaf, 0xd4, 0xd6, 0xc7, 0x4a, 0xf2, 0x28, 0xd2, 0x67,
    0x02, 0x20, 0x6b, 0x0e, 0x1d, 0xbf, 0x93, 0x5b, 0xbd, 0x77, 0x43, 0x27,
    0x24, 0x83, 0xb5, 0x72, 0xa5, 0x3f, 0x0b, 0x1d, 0x26, 0x43, 0xa2, 0xf6,
    0xea, 0xb7, 0x30, 0x5f, 0xb6, 0x62, 0x7c, 0xf9, 0x85, 0x51, 0x02, 0x20,
    0x3d, 0x22, 0x63, 0x15, 0x6b, 0x32, 0x41, 0x46, 0x44, 0x78, 0xb7, 0x13,
    0xeb, 0x85, 0x4c, 0x4f, 0x6b, 0x3e, 0xf0, 0x52, 0xf0, 0x46, 0x3b, 0x65,
    0xd8, 0x21, 0x7d, 0xae, 0xc0, 0x09, 0x98, 0x34};

const std::vector<uint8_t> kInvalidLengthKey = {
    0x30, 0x1b,        // SEQUENCE(len=27)
    0x02, 0x01, 0x00,  // INT(len=1) = 0
    0x30, 0x13,        // SEQUENCE(len=19)
    0x06, 0x07,        // OID(len=7)
    // dhPublicKey (1.2.840.10046.2.1)
    0x2a, 0x86, 0x48, 0xce, 0x3e, 0x02, 0x01, 0x06, 0x08,  // OID(len=8)
    // prime256v1 (1.2.840.10045.3.1.7) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x04,
    0x00  // OCTET STRING(len=0)
};

const std::vector<uint8_t> kInvalidZeroLengthKey = {
    0x30, 0x1a,        // SEQUENCE(len=26)
    0x02, 0x01, 0x00,  // INT(len=1) = 0
    0x30, 0x13,        // SEQUENCE(len=19)
    0x06, 0x07,        // OID(len=7)
    // dhPublicKey (1.2.840.10046.2.1)
    0x2a, 0x86, 0x48, 0xce, 0x3e, 0x02, 0x01, 0x06, 0x08,  // OID(len=8)
    // prime256v1 (1.2.840.10045.3.1.7) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x04,
    0x00  // OCTET STRING(len=0)
};

class DERPrivateKeyImportTest : public ::testing::Test {
 public:
  bool ParsePrivateKey(const std::vector<uint8_t>& data, bool expect_success) {
    SECKEYPrivateKey* key = nullptr;
    SECStatus rv = SECFailure;
    std::string nick_str =
        ::testing::UnitTest::GetInstance()->current_test_info()->name() +
        std::to_string(rand());
    SECItem item = {siBuffer, const_cast<unsigned char*>(data.data()),
                    static_cast<unsigned int>(data.size())};
    SECItem nick = {siBuffer, reinterpret_cast<unsigned char*>(
                                  const_cast<char*>(nick_str.data())),
                    static_cast<unsigned int>(nick_str.length())};

    ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
    EXPECT_TRUE(slot);
    if (!slot) {
      return false;
    }

    if (PK11_NeedUserInit(slot.get())) {
      if (PK11_InitPin(slot.get(), nullptr, nullptr) != SECSuccess) {
        EXPECT_EQ(rv, SECSuccess) << "PK11_InitPin failed";
      }
    }
    rv = PK11_Authenticate(slot.get(), PR_TRUE, nullptr);
    EXPECT_EQ(rv, SECSuccess);

    rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
        slot.get(), &item, &nick, nullptr, true, false, KU_ALL, &key, nullptr);
    EXPECT_EQ(rv == SECSuccess, key != nullptr);

    if (expect_success) {
      // Try to find the key via its label
      ScopedSECKEYPrivateKeyList list(PK11_ListPrivKeysInSlot(
          slot.get(), const_cast<char*>(nick_str.c_str()), nullptr));
      EXPECT_FALSE(!list);
    }

    if (key) {
      rv = PK11_DeleteTokenPrivateKey(key, true);
      EXPECT_EQ(SECSuccess, rv);

      // PK11_DeleteTokenPrivateKey leaves an errorCode set when there's
      // no cert. This is expected, so clear it.
      if (PORT_GetError() == SSL_ERROR_NO_CERTIFICATE) {
        PORT_SetError(0);
      }
    }

    return rv == SECSuccess;
  }
};

TEST_F(DERPrivateKeyImportTest, ImportPrivateRSAKey) {
  EXPECT_TRUE(ParsePrivateKey(kValidRSAKey, true));
  EXPECT_FALSE(PORT_GetError()) << PORT_GetError();
}

TEST_F(DERPrivateKeyImportTest, ImportEcdsaKey) {
  EXPECT_TRUE(ParsePrivateKey(kValidP256Key, true));
  EXPECT_FALSE(PORT_GetError()) << PORT_GetError();
}

TEST_F(DERPrivateKeyImportTest, ImportInvalidPrivateKey) {
  EXPECT_FALSE(ParsePrivateKey(kInvalidLengthKey, false));
  EXPECT_EQ(PORT_GetError(), SEC_ERROR_BAD_DER) << PORT_GetError();
}

TEST_F(DERPrivateKeyImportTest, ImportZeroLengthPrivateKey) {
  EXPECT_FALSE(ParsePrivateKey(kInvalidZeroLengthKey, false));
  EXPECT_EQ(PORT_GetError(), SEC_ERROR_BAD_KEY) << PORT_GetError();
}

}  // namespace nss_test
