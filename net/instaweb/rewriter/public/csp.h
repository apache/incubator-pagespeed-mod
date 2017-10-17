/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)
//
// This provides basic parsing and evaluation of a (subset of)
// Content-Security-Policy that's relevant for PageSpeed Automatic.
// CspContext is the main class.
//
// Limitations versus the full spec:
// 1) We don't fully parse some kinds of source expressions, like nonce and
//    hash ones.
// 2) Only some of the directives are parsed.
// 3) URL matching doesn't support WebSocket (ws: and wss:) schemes, since
//    mod_pagespeed doesn't, and they make for some really ugly conditionals.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSP_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSP_H_

#include <memory>
#include <string>
#include <vector>

#include "net/instaweb/rewriter/public/csp_directive.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class CspSourceExpression {
 public:
  enum Kind {
    kSelf, kSchemeSource, kHostSource,
    kUnsafeInline, kUnsafeEval, kStrictDynamic, kUnsafeHashedAttributes,
    kHashOrNonce, kUnknown
  };

  struct UrlData {
    UrlData() : path_exact_match(false) {}
    // Constructor for tests, assumes already normalized.
    UrlData(StringPiece in_scheme, StringPiece in_host,
            StringPiece in_port, StringPiece in_path,
            bool exact_match = false)
        : scheme_part(in_scheme.as_string()),
          host_part(in_host.as_string()),
          port_part(in_port.as_string()),
          path_exact_match(exact_match) {
      StringPieceVector portions;
      SplitStringPieceToVector(in_path, "/", &portions, true);
      for (StringPiece p : portions) {
        path_part.push_back(p.as_string());
      }
    }

    // All the components here are stored in a manner that matches the way
    // GoogleUrl stores their corresponding portions, to make it easy to
    // compare against incoming URLs:
    // 1) The case-insensitive scheme and host portions are lowercased.
    // 2) The case-sensitive path doesn't have its case changed, but the
    //    % escaping is normalized. We also pre-split it since we have
    //    to check per-component.
    GoogleString scheme_part;  // doesn't include :
    GoogleString host_part;
    GoogleString port_part;
    // separated by /
    std::vector<GoogleString> path_part;
    bool path_exact_match;

    GoogleString DebugString() const {
      return StrCat("scheme:", scheme_part, " host:", host_part,
                    " port:", port_part,
                    " path:",  JoinCollection(path_part, "/"),
                    " path_exact_match:", BoolToString(path_exact_match));
    }

    // For convenience of unit testing.
    bool operator==(const UrlData& other) const {
      return scheme_part == other.scheme_part &&
             host_part == other.host_part &&
             port_part == other.port_part &&
             path_part == other.path_part &&
             path_exact_match == other.path_exact_match;
    }
  };

  CspSourceExpression() : kind_(kUnknown) {}
  explicit CspSourceExpression(Kind kind): kind_(kind) {}
  CspSourceExpression(Kind kind, const UrlData& url_data) : kind_(kind) {
    *mutable_url_data() = url_data;
  }

  static CspSourceExpression Parse(StringPiece input);

  bool Matches(const GoogleUrl& origin_url, const GoogleUrl& url) const;

  GoogleString DebugString() const {
    return StrCat("kind:", IntegerToString(kind_),
                  " url_data:{", url_data().DebugString(), "}");
  }

  bool operator==(const CspSourceExpression& other) const {
    return (kind_ == other.kind_) && (url_data() == other.url_data());
  }

  Kind kind() const { return kind_; }

  const UrlData& url_data() const {
    if (url_data_.get() == nullptr) {
      url_data_.reset(new UrlData());
    }
    return *url_data_.get();
  }

 private:
  // input here is without the quotes, and non-empty.
  static CspSourceExpression ParseQuoted(StringPiece input);

  // Returns true if input matches the base64-value production in CSP spec.
  static bool ParseBase64(StringPiece input);

  // Tries to see if the input is either an entire scheme-source, or the
  // scheme-part portion of a host-source, filling in url_data->scheme_part
  // appropriately. Returns true only if this is a scheme-source, however.
  bool TryParseScheme(StringPiece* input);

  static bool HasDefaultPortForScheme(const GoogleUrl& url);

  UrlData* mutable_url_data() {
    if (url_data_.get() == nullptr) {
      url_data_.reset(new UrlData());
    }
    return url_data_.get();
  }

  Kind kind_;
  mutable std::unique_ptr<UrlData> url_data_;
};

class CspSourceList {
 public:
  CspSourceList()
      : saw_unsafe_inline_(false), saw_unsafe_eval_(false),
        saw_strict_dynamic_(false), saw_unsafe_hashed_attributes_(false),
        saw_hash_or_nonce_(false) {}

  static std::unique_ptr<CspSourceList> Parse(StringPiece input);
  const std::vector<CspSourceExpression>& expressions() const {
    return expressions_;
  }

  bool saw_unsafe_inline() const { return saw_unsafe_inline_; }
  bool saw_unsafe_eval() const { return saw_unsafe_eval_; }
  bool saw_strict_dynamic() const { return saw_strict_dynamic_; }
  bool saw_unsafe_hashed_attributes() const {
    return saw_unsafe_hashed_attributes_;
  }

  bool saw_hash_or_nonce() const { return saw_hash_or_nonce_; }

  bool Matches(const GoogleUrl& origin_url, const GoogleUrl& url) const;

 private:
  std::vector<CspSourceExpression> expressions_;
  bool saw_unsafe_inline_;
  bool saw_unsafe_eval_;
  bool saw_strict_dynamic_;
  bool saw_unsafe_hashed_attributes_;
  bool saw_hash_or_nonce_;
};

// An individual policy. Note that a page is constrained by an intersection
// of some number of these.
class CspPolicy {
 public:
  CspPolicy();

  // May return null.
  static std::unique_ptr<CspPolicy> Parse(StringPiece input);

  // May return null.
  const CspSourceList* SourceListFor(CspDirective directive) const {
    return policies_[static_cast<int>(directive)].get();
  }

  bool PermitsEval() const;
  bool PermitsInlineScript() const;
  bool PermitsInlineScriptAttribute() const;
  bool PermitsInlineStyle() const;
  bool PermitsInlineStyleAttribute() const;

  // Tests whether 'url' can be loaded within 'origin_url' as 'role', where
  // 'role' should be kStyleSrc, kScriptSrc or kImgSrc.
  bool CanLoadUrl(CspDirective role, const GoogleUrl& origin_url,
                  const GoogleUrl& url) const;

  bool IsBasePermitted(const GoogleUrl& previous_origin,
                       const GoogleUrl& base_candidate) const;

 private:
  // The expectation is that some of these may be null.
  std::vector<std::unique_ptr<CspSourceList>> policies_;
};

// A set of all policies (maybe none!) on the page. Note that we do not track
// those with report disposition, only those that actually enforce --- reporting
// seems like it would keep the page author informed about our effects as it is.
class CspContext {
 public:
  bool PermitsEval() const {
    return AllPermit(&CspPolicy::PermitsEval);
  }

  bool PermitsInlineScript() const {
    return AllPermit(&CspPolicy::PermitsInlineScript);
  }

  bool PermitsInlineScriptAttribute() const {
    return AllPermit(&CspPolicy::PermitsInlineScriptAttribute);
  }

  bool PermitsInlineStyle() const {
    return AllPermit(&CspPolicy::PermitsInlineStyle);
  }

  bool PermitsInlineStyleAttribute() const {
    return AllPermit(&CspPolicy::PermitsInlineStyleAttribute);
  }

  bool CanLoadUrl(CspDirective role, const GoogleUrl& origin_url,
                  const GoogleUrl& url) {
    // All policies must OK, with base case being 'true'.
    for (const auto& policy : policies_) {
      if (!policy->CanLoadUrl(role, origin_url, url)) {
        return false;
      }
    }
    return true;
  }

  bool IsBasePermitted(const GoogleUrl& previous_origin,
                       const GoogleUrl& base_candidate) const {
    for (const auto& policy : policies_) {
      if (!policy->IsBasePermitted(previous_origin, base_candidate)) {
        return false;
      }
    }
    return true;
  }

  bool HasDirective(CspDirective directive) const {
    for (const auto& policy : policies_) {
      if (policy->SourceListFor(directive) != nullptr) {
        return true;
      }
    }
    return false;
  }

  bool HasDirectiveOrDefaultSrc(CspDirective directive) const {
    for (const auto& policy : policies_) {
      if (policy->SourceListFor(directive) != nullptr ||
          policy->SourceListFor(CspDirective::kDefaultSrc) != nullptr) {
        return true;
      }
    }
    return false;
  }

  void AddPolicy(std::unique_ptr<CspPolicy> policy);
  void Clear() { policies_.clear(); }
  size_t policies_size() const { return policies_.size(); }
  bool empty() const { return policies_.empty(); }

 private:
  typedef bool (CspPolicy::*SimplePredicateFn)() const;

  bool AllPermit(SimplePredicateFn predicate) const {
    // Note that empty policies_ means "true" --- there is no policy whatsoever,
    // so everything is permitted. If there is more than that, all policies
    // must agree, too.
    for (const auto& policy : policies_) {
      if (!(policy.get()->*predicate)()) {
        return false;
      }
    }
    return true;
  }

  std::vector<std::unique_ptr<CspPolicy>> policies_;
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSP_H_
