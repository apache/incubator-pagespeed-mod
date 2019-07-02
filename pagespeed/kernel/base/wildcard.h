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


#ifndef PAGESPEED_KERNEL_BASE_WILDCARD_H_
#define PAGESPEED_KERNEL_BASE_WILDCARD_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class Wildcard {
 public:
  static const char kMatchAny;  // *
  static const char kMatchOne;  // ?

  // Create a wildcard object with the specification using * and ?
  // as wildcards.  There is currently no way to quote * or ?.
  explicit Wildcard(const StringPiece& wildcard_spec);

  // Determines whether a string matches the wildcard.
  bool Match(const StringPiece& str) const;

  // Determines whether this wildcard is just a simple name, lacking
  // any wildcard characters.
  bool IsSimple() const { return is_simple_; }

  // Returns the original wildcard specification.
  const StringPiece spec() const {
    return StringPiece(storage_.data(), storage_.size() - 1);
  }

  // Makes a duplicate copy of the wildcard object.
  Wildcard* Duplicate() const;

 private:
  Wildcard(const GoogleString& storage, int num_blocks,
           int last_block_offset, bool is_simple);

  void InitFromSpec(const StringPiece& wildcard_spec);

  GoogleString storage_;
  int num_blocks_;
  int last_block_offset_;
  bool is_simple_;
  DISALLOW_COPY_AND_ASSIGN(Wildcard);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_WILDCARD_H_
