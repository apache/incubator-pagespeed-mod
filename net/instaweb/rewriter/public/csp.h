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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSP_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSP_H_

#include <memory>
#include <string>
#include <vector>

#include "net/instaweb/rewriter/public/csp_directive.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class CspSourceExpression {
 public:
  enum Kind {
    kSelf, kSchemeSource, kHostSource,
    kUnsafeInline, kUnsafeEval, kStrictDynamic, kUnsafeHashedAttributes,
    kUnknown /* includes hash-or-nonce */
  };

  struct UrlData {
    UrlData() {}
    UrlData(StringPiece in_scheme, StringPiece in_host,
            StringPiece in_port, StringPiece in_path)
        : scheme_part(in_scheme.as_string()),
          host_part(in_host.as_string()),
          port_part(in_port.as_string()),
          path_part(in_path.as_string()) {}

    GoogleString scheme_part;  // doesn't include :
    GoogleString host_part;
    GoogleString port_part;
    GoogleString path_part;

    GoogleString DebugString() const {
      return StrCat("scheme:", scheme_part, " host:", host_part,
                    " port:", port_part, " path:", path_part);
    }

    bool operator==(const UrlData& other) const {
      return scheme_part == other.scheme_part &&
             host_part == other.host_part &&
             port_part == other.port_part &&
             path_part == other.path_part;
    }
  };

  CspSourceExpression() : kind_(kUnknown) {}
  explicit CspSourceExpression(Kind kind): kind_(kind) {}
  CspSourceExpression(Kind kind, const UrlData& url_data) : kind_(kind) {
    *mutable_url_data() = url_data;
  }

  static CspSourceExpression Parse(StringPiece input);

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

  // Tries to see if the input is either an entire scheme-source, or the
  // scheme-part portion of a host-source, filling in url_data->scheme_part
  // appropriately. Returns true only if this is a scheme-source, however.
  bool TryParseScheme(StringPiece* input);

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
  static std::unique_ptr<CspSourceList> Parse(StringPiece input);
  const std::vector<CspSourceExpression>& expressions() const {
    return expressions_;
  }

 private:
  std::vector<CspSourceExpression> expressions_;
};

// An individual policy. Note that a page is constrained by an intersection
// of some number of these.
class CspPolicy {
 public:
  CspPolicy();

  // Just an example for now...
  bool UnsafeEval() const { return false; /* */ }

  // May return null.
  static std::unique_ptr<CspPolicy> Parse(StringPiece input);

  // May return null.
  const CspSourceList* SourceListFor(CspDirective directive) {
    return policies_[static_cast<int>(directive)].get();
  }

 private:
  // The expectation is that some of these may be null.
  std::vector<std::unique_ptr<CspSourceList>> policies_;
};

// A set of all policies (maybe none!) on the page. Note that we do not track
// those with report disposition, only those that actually enforce --- reporting
// seems like it would keep the page author informed about our effects as it is.
class CspContext {
 public:
  bool UnsafeEval() const {
    return AllPermit(&CspPolicy::UnsafeEval);
  }

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
