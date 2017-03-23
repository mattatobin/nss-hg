/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#include "gtest/gtest.h"
#include "scoped_ptrs.h"

#include "nss.h"
#include "prerror.h"
#include "secasn1.h"
#include "secerr.h"
#include "secitem.h"

const SEC_ASN1Template mySEC_NullTemplate[] = {
    {SEC_ASN1_NULL, 0, NULL, sizeof(SECItem)}};

namespace nss_test {

class QuickDERTest : public ::testing::Test,
                     public ::testing::WithParamInterface<SECItem> {};

static const uint8_t kNullTag = 0x05;
static const uint8_t kLongLength = 0x80;

// Length of zero wrongly encoded as 0x80 instead of 0x00.
static uint8_t kOverlongLength_0_0[] = {kNullTag, kLongLength | 0};

// Length of zero wrongly encoded as { 0x81, 0x00 } instead of 0x00.
static uint8_t kOverlongLength_1_0[] = {kNullTag, kLongLength | 1, 0x00};

// Length of zero wrongly encoded as:
//
//     { 0x90, <arbitrary junk of 12 bytes>,
//       0x00, 0x00, 0x00, 0x00 }
//
// instead of 0x00. Note in particular that if there is an integer overflow
// then the arbitrary junk is likely get left-shifted away, as long as there
// are at least sizeof(length) bytes following it. This would be a good way to
// smuggle arbitrary input into DER-encoded data in a way that an non-careful
// parser would ignore.
static uint8_t kOverlongLength_16_0[] = {kNullTag, kLongLength | 0x10,
                                         0x11,     0x22,
                                         0x33,     0x44,
                                         0x55,     0x66,
                                         0x77,     0x88,
                                         0x99,     0xAA,
                                         0xBB,     0xCC,
                                         0x00,     0x00,
                                         0x00,     0x00};

static const SECItem kInvalidDER[] = {
    {siBuffer, kOverlongLength_0_0, sizeof(kOverlongLength_0_0)},
    {siBuffer, kOverlongLength_1_0, sizeof(kOverlongLength_1_0)},
    {siBuffer, kOverlongLength_16_0, sizeof(kOverlongLength_16_0)},
};

TEST_P(QuickDERTest, InvalidLengths) {
  const SECItem& original_input(GetParam());

  ScopedSECItem copy_of_input(SECITEM_AllocItem(nullptr, nullptr, 0U));
  ASSERT_TRUE(copy_of_input);
  ASSERT_EQ(SECSuccess,
            SECITEM_CopyItem(nullptr, copy_of_input.get(), &original_input));

  PORTCheapArenaPool pool;
  PORT_InitCheapArena(&pool, DER_DEFAULT_CHUNKSIZE);
  ScopedSECItem parsed_value(SECITEM_AllocItem(nullptr, nullptr, 0U));
  ASSERT_TRUE(parsed_value);
  ASSERT_EQ(SECFailure,
            SEC_QuickDERDecodeItem(&pool.arena, parsed_value.get(),
                                   mySEC_NullTemplate, copy_of_input.get()));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
  PORT_DestroyCheapArena(&pool);
}

INSTANTIATE_TEST_CASE_P(QuickderTestsInvalidLengths, QuickDERTest,
                        testing::ValuesIn(kInvalidDER));

}  // namespace nss_test
