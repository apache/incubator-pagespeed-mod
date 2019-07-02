/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "pagespeed/kernel/base/md5_hasher.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

namespace {

// See http://goto/gunitprimer for an introduction to gUnit.

class MD5HasherTest : public ::testing::Test {};

TEST_F(MD5HasherTest, CorrectHashSize) {
  // MD5 is 128-bit, which is 21.333 6-bit chars.
  const int kMaxHashSize = 21;
  for (int i = kMaxHashSize; i >= 0; --i) {
    MD5Hasher hasher(i);
    EXPECT_EQ(i, hasher.HashSizeInChars());
    EXPECT_EQ(i, hasher.Hash("foobar").size());
    // Large string.
    EXPECT_EQ(i, hasher.Hash(GoogleString(5000, 'z')).size());
  }
}

TEST_F(MD5HasherTest, HashesDiffer) {
  MD5Hasher hasher;

  // Basic sanity tests. More thorough tests belong in the base implementation.
  EXPECT_NE(hasher.Hash("foo"), hasher.Hash("bar"));
  EXPECT_NE(hasher.Hash(GoogleString(5000, 'z')),
            hasher.Hash(GoogleString(5001, 'z')));
}

}  // namespace

}  // namespace net_instaweb
