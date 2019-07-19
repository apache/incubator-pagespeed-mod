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


#include "pagespeed/kernel/base/hasher.h"

#include <algorithm>

#include "base/logging.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

Hasher::Hasher(int max_chars): max_chars_(max_chars) {
  CHECK_LE(0, max_chars);
}

Hasher::~Hasher() {
}

GoogleString Hasher::Hash(const StringPiece& content) const {
  GoogleString raw_hash = RawHash(content);
  GoogleString out;
  Web64Encode(raw_hash, &out);

  // Truncate to how many characters are actually requested. We use
  // HashSizeInChars() here for consistency of rounding.
  out.resize(HashSizeInChars());
  return out;
}

int Hasher::HashSizeInChars() const {
  // For char hashes, we return the hash after Base64 encoding, which expands by
  // 4/3. We round down, this should not matter unless someone really wants that
  // extra few bits.
  return std::min(max_chars_, RawHashSizeInBytes() * 4 / 3);
}

uint64 Hasher::HashToUint64(const StringPiece& content) const {
  GoogleString raw_hash = RawHash(content);

  CHECK_LE(8UL, raw_hash.size());
  DCHECK_EQ(1UL, sizeof(raw_hash[0]));  // 1 byte == 8 bits.
  uint64 result = 0;
  for (int i = 0; i < 8; ++i) {
    result <<= 8;
    result |= static_cast<unsigned char>(raw_hash[i]);
  }
  return result;
}

}  // namespace net_instaweb
