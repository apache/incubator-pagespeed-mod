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

//
// Implementations of FileLoadRuleLiteral and FileLoadRuleRegexp, two
// subclasses of the abstract class FileLoadRule, in addition to implementation
// of FileLoadRule.
//
// Tests are in file_load_policy_test.

#include "net/instaweb/rewriter/public/file_load_rule.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

FileLoadRule::Classification FileLoadRule::Classify(
    const GoogleString& filename) const {
  if (Match(filename)) {
    return allowed_ ? kAllowed : kDisallowed;
  } else {
    return kUnmatched;
  }
}

FileLoadRule::~FileLoadRule() {}
FileLoadRuleRegexp::~FileLoadRuleRegexp() {}
FileLoadRuleLiteral::~FileLoadRuleLiteral() {}

bool FileLoadRuleRegexp::Match(const GoogleString& filename) const {
  return RE2::PartialMatch(filename, filename_regexp_str_);
}

bool FileLoadRuleLiteral::Match(const GoogleString& filename) const {
  return StringPiece(filename).starts_with(filename_prefix_);
}

}  // namespace net_instaweb
