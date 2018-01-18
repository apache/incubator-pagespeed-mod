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


#include <list>

#include "net/instaweb/rewriter/public/file_load_mapping.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/file_load_rule.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

FileLoadPolicy::~FileLoadPolicy() {
  for (FileLoadMappings::const_iterator it = file_load_mappings_.begin();
       it != file_load_mappings_.end(); ++it) {
    (*it)->DecrementRefs();
  }
  for (FileLoadRules::const_iterator it = file_load_rules_.begin();
       it != file_load_rules_.end(); ++it) {
    (*it)->DecrementRefs();
  }
}

// Figure out whether our rules say to load this url from file, ignoring content
// type restrictions for the moment.
bool FileLoadPolicy::ShouldLoadFromFileHelper(const GoogleUrl& url,
                                              GoogleString* filename) const {
  if (!url.IsWebValid()) {
    return false;
  }

  const StringPiece url_string = url.AllExceptQuery();
  if (!url_string.empty()) {
    // TODO(sligocki): Consider layering a cache over this lookup.
    // Note: Later associations take precedence over earlier ones.
    for (FileLoadMappings::const_reverse_iterator mappings_iter =
             file_load_mappings_.rbegin();
         mappings_iter != file_load_mappings_.rend(); ++mappings_iter) {
      if ((*mappings_iter)->Substitute(url_string, filename)) {
        // GoogleUrl will decode most %XX escapes, but it does not convert
        // "%20" -> " " which has come up often.
        GlobalReplaceSubstring("%20", " ", filename);

        // We know know what file this url should map to, and we want know
        // whether this one is safe to load directly or whether we need to back
        // off and load through HTTP.  By default a mapping set up with
        // Associate() permits direct loading of anything it applies to, but
        // AddRule() lets people add exceptions.  See if any exceptions apply.
        for (FileLoadRules::const_reverse_iterator rules_iter =
                 file_load_rules_.rbegin();
             rules_iter != file_load_rules_.rend(); ++rules_iter) {
          const FileLoadRule::Classification classification =
              (*rules_iter)->Classify(*filename);
          if (classification == FileLoadRule::kAllowed) {
            return true;  // Whitelist entry: load directly.
          } else if (classification == FileLoadRule::kDisallowed) {
            return false;  // Blacklist entry: fall back to HTTP.
          }
        }
        return true;  // No exception applied; default allow.
      }
    }
  }
  return false;  // No mapping found, no file to load from.
}

bool FileLoadPolicy::ShouldLoadFromFile(const GoogleUrl& url,
                                        GoogleString* filename) const {
  const bool should_load = ShouldLoadFromFileHelper(url, filename);
  if (!should_load) {
    return false;
  }

  // We could now load it from file, but if the extension is unrecognized we
  // won't have a content type.  We want to always serve with content type, so
  // filter those out.  This also lets us limit to static resources, which are
  // the only content types we want to handle.
  const ContentType* content_type = NameExtensionToContentType(*filename);
  return content_type != NULL && content_type->IsLikelyStaticResource();
}

bool FileLoadPolicy::AddRule(const GoogleString& rule_str, bool is_regexp,
                             bool allow, GoogleString* error) {
  FileLoadRule* rule = NULL;
  if (is_regexp) {
    const RE2 re(rule_str);
    if (!re.ok()) {
      error->assign(re.error());
      return false;
    }
    rule = new FileLoadRuleRegexp(rule_str, allow);
  } else {
    rule = new FileLoadRuleLiteral(rule_str, allow);
  }
  file_load_rules_.push_back(rule);
  return true;
}

bool FileLoadPolicy::AssociateRegexp(StringPiece url_regexp,
                                     StringPiece filename_prefix,
                                     GoogleString* error) {
  GoogleString url_regexp_str, filename_prefix_str;

  url_regexp.CopyToString(&url_regexp_str);
  filename_prefix.CopyToString(&filename_prefix_str);

  if (!url_regexp.starts_with("^")) {
    error->assign("File mapping regular expression must match beginning "
                  "of string. (Must start with '^'.)");
    return false;
  }

  const RE2 re(url_regexp_str);
  if (!re.ok()) {
    error->assign(re.error());
    return false;
  } else if (!re.CheckRewriteString(filename_prefix_str, error)) {
    return false;
  }

  file_load_mappings_.push_back(
      new FileLoadMappingRegexp(url_regexp_str, filename_prefix_str));

  return true;
}

void FileLoadPolicy::Associate(StringPiece url_prefix_in,
                               StringPiece filename_prefix_in) {
  GoogleString url_prefix, filename_prefix;

  url_prefix_in.CopyToString(&url_prefix);
  filename_prefix_in.CopyToString(&filename_prefix);

  // Make sure these are directories.  Add a terminal slashes if absent.
  EnsureEndsInSlash(&url_prefix);
  EnsureEndsInSlash(&filename_prefix);

  // TODO(sligocki): Should fail if filename_prefix doesn't start with '/'?

  file_load_mappings_.push_back(
      new FileLoadMappingLiteral(url_prefix, filename_prefix));
}

void FileLoadPolicy::Merge(const FileLoadPolicy& other) {
  for (FileLoadMappings::const_iterator it = other.file_load_mappings_.begin();
       it != other.file_load_mappings_.end(); ++it) {
    // Copy associations over.
    (*it)->IncrementRefs();
    file_load_mappings_.push_back(*it);
  }

  for (FileLoadRules::const_iterator it = other.file_load_rules_.begin();
       it != other.file_load_rules_.end(); ++it) {
    // Copy rules over.
    (*it)->IncrementRefs();
    file_load_rules_.push_back(*it);
  }
}

}  // namespace net_instaweb
