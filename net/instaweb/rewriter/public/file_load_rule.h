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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_RULE_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_RULE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/manually_ref_counted.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

// Class for storing information about what filesystem paths are appropriate for
// direct access and which need to be fetched through HTTP loopback.
class FileLoadRule : public ManuallyRefCounted {
 public:
  enum Classification {
    kAllowed,
    kDisallowed,
    kUnmatched,
  };

  virtual ~FileLoadRule();
  explicit FileLoadRule(bool allowed) : allowed_(allowed) {}

  // What does this rule say about this filename?
  Classification Classify(const GoogleString& filename) const;

 protected:
  // Is does this rule apply to this filename?
  virtual bool Match(const GoogleString& filename) const = 0;
  const bool allowed_;
};

class FileLoadRuleRegexp : public FileLoadRule {
 public:
  virtual ~FileLoadRuleRegexp();

  // If allowed is true, whitelist filenames matching filename_regexp.
  // Otherwise blacklist them.
  FileLoadRuleRegexp(const GoogleString& filename_regexp, bool allowed)
      : FileLoadRule(allowed),
        filename_regexp_(filename_regexp),
        filename_regexp_str_(filename_regexp)
  {}

  virtual bool Match(const GoogleString& filename) const;

 private:
  const RE2 filename_regexp_;
  // RE2s can't be copied, so we need to keep the string around.
  const GoogleString filename_regexp_str_;

  DISALLOW_COPY_AND_ASSIGN(FileLoadRuleRegexp);
};

class FileLoadRuleLiteral : public FileLoadRule {
 public:
  virtual ~FileLoadRuleLiteral();

  // If allowed is true, whitelist filenames starting with filename_prefix.
  // Otherwise blacklist them.
  FileLoadRuleLiteral(const GoogleString& filename_prefix, bool allowed)
      : FileLoadRule(allowed), filename_prefix_(filename_prefix)
  {}

  virtual bool Match(const GoogleString& filename) const;

 private:
  const GoogleString filename_prefix_;

  DISALLOW_COPY_AND_ASSIGN(FileLoadRuleLiteral);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_FILE_LOAD_RULE_H_
