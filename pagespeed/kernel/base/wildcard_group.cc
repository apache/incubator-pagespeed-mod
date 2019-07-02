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


// NOTE: THIS CODE IS DEAD.  IT IS ONLY LINKED BY THE SPEED_TEST PROVING IT'S
// SLOWER THAN FastWildcardGroup, PLUS ITS OWN UNIT TEST.

#include "pagespeed/kernel/base/wildcard_group.h"

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/wildcard.h"

namespace net_instaweb {

WildcardGroup::~WildcardGroup() {
  Clear();
}

void WildcardGroup::Clear() {
  STLDeleteElements(&wildcards_);
  allow_.clear();
}

void WildcardGroup::Allow(const StringPiece& expr) {
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(true);
}

void WildcardGroup::Disallow(const StringPiece& expr) {
  Wildcard* wildcard = new Wildcard(expr);
  wildcards_.push_back(wildcard);
  allow_.push_back(false);
}

bool WildcardGroup::Match(const StringPiece& str, bool allow) const {
  CHECK_EQ(wildcards_.size(), allow_.size());
  for (int i = wildcards_.size() - 1; i >= 0; --i) {
    // Match from last-inserted to first-inserted, returning status of
    // last-inserted match found.
    if (wildcards_[i]->Match(str)) {
      return allow_[i];
    }
  }
  return allow;
}

void WildcardGroup::CopyFrom(const WildcardGroup& src) {
  Clear();
  AppendFrom(src);
}

void WildcardGroup::AppendFrom(const WildcardGroup& src) {
  CHECK_EQ(src.wildcards_.size(), src.allow_.size());
  for (int i = 0, n = src.wildcards_.size(); i < n; ++i) {
    wildcards_.push_back(src.wildcards_[i]->Duplicate());
    allow_.push_back(src.allow_[i]);
  }
}

GoogleString WildcardGroup::Signature() const {
  GoogleString signature;
  for (int i = 0, n = wildcards_.size(); i < n; ++i) {
    StrAppend(&signature, wildcards_[i]->spec(), (allow_[i] ? "A" : "D"), ",");
  }
  return signature;
}

}  // namespace net_instaweb
