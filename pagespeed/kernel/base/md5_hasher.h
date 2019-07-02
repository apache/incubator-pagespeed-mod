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


#ifndef PAGESPEED_KERNEL_BASE_MD5_HASHER_H_
#define PAGESPEED_KERNEL_BASE_MD5_HASHER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class MD5Hasher : public Hasher {
 public:
  static const int kDefaultHashSize = 10;

  MD5Hasher() : Hasher(kDefaultHashSize) {}
  explicit MD5Hasher(int hash_size) : Hasher(hash_size) { }
  virtual ~MD5Hasher();

  virtual GoogleString RawHash(const StringPiece& content) const;
  virtual int RawHashSizeInBytes() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MD5Hasher);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MD5_HASHER_H_
