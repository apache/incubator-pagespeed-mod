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
// This provides basic parsing and evaluation of a (subset of)
// Content-Security-Policy that's relevant for PageSpeed Automatic.

#include "net/instaweb/rewriter/public/csp.h"

#include "net/instaweb/rewriter/public/csp_directive.h"

namespace net_instaweb {

namespace {

void TrimCspWhitespace(StringPiece* input) {
  // AKA RWS in HTTP spec, which of course isn't the HTML notion of whitespace
  // that TrimWhitespace uses.
  while (!input->empty() && ((*input)[0] == ' ' || (*input)[0] == '\t')) {
    input->remove_prefix(1);
  }

  while (input->ends_with(" ") || input->ends_with("\t")) {
    input->remove_suffix(1);
  }
}

char Last(StringPiece input) {
  DCHECK(!input.empty());
  return input[input.size() - 1];
}

inline bool IsAsciiAlpha(char ch) {
  return (((ch >= 'a') && (ch <= 'z')) ||
          ((ch >= 'A') && (ch <= 'Z')));
}

inline bool IsSchemeContinuation(char ch) {
  return IsAsciiAlphaNumeric(ch) || (ch == '+') || (ch == '-') || (ch == '.');
}

inline bool IsBase64Char(char ch) {
  // ALPHA / DIGIT / "+" / "/" / "-" / "_"
  return IsAsciiAlphaNumeric(ch) ||
         (ch == '+') || (ch == '/') || (ch == '-') || (ch == '_');
}

}  // namespace

bool CspSourceExpression::TryParseScheme(StringPiece* input) {
  if (input->size() < 2) {
    // Need at least a: or such.
    return false;
  }

  if (!IsAsciiAlpha((*input)[0])) {
    return false;
  }

  size_t pos = 1;
  while (pos < input->size() && IsSchemeContinuation((*input)[pos])) {
    ++pos;
  }

  if (pos == input->size() || (*input)[pos] != ':') {
    // All schema characters, but no : or :// afterwards -> something else.
    return false;
  }

  if (pos == (input->size() - 1)) {
    // Last character was also : -> clearly a scheme-source
    kind_ = kSchemeSource;
    input->substr(0, pos).CopyToString(&mutable_url_data()->scheme_part);
    input->remove_prefix(pos + 1);
    LowerString(&mutable_url_data()->scheme_part);
    return true;
  }

  // We now want to see if it's actually foo://
  if ((pos + 2 < input->size())
       && ((*input)[pos + 1] == '/') && ((*input)[pos + 2] == '/')) {
    input->substr(0, pos).CopyToString(&mutable_url_data()->scheme_part);
    input->remove_prefix(pos + 3);
    LowerString(&mutable_url_data()->scheme_part);
  }

  // Either way, it's not a valid scheme-source at this point, even if it's
  // a valid host-source
  return false;
}

CspSourceExpression CspSourceExpression::Parse(StringPiece input) {
  TrimCspWhitespace(&input);
  if (input.empty()) {
    return CspSourceExpression();
  }

  if (input.size() > 2 && input[0] == '\'' && Last(input) == '\'') {
    return ParseQuoted(input.substr(1, input.size() - 2));
  }

  CspSourceExpression result;
  if (!result.TryParseScheme(&input)) {
    // This looks like a host-source, and we have already skipped
    // over a scheme, and ://, if any.
    // From the spec:
    // host-source = [ scheme-part "://" ] host-part [ port-part ] [ path-part ]

    // host-part   = "*" / [ "*." ] 1*host-char *( "." 1*host-char )
    // host-char   = ALPHA / DIGIT / "-"
    // port-part   = ":" ( 1*DIGIT / "*" )
    //
    // Key bit from path-part: it's either empty or starts with /
    result.kind_ = kHostSource;
    if (input.empty()) {
      return CspSourceExpression();
    }

    if (input.starts_with("*.")) {
      result.mutable_url_data()->host_part = "*.";
      input.remove_prefix(2);
    } else if (input.starts_with("*")) {
      result.mutable_url_data()->host_part = "*";
      input.remove_prefix(1);
    }

    while (!input.empty()
           && (IsAsciiAlphaNumeric(input[0])
               || (input[0] == '-') || (input[0] == '.'))) {
      result.mutable_url_data()->host_part.push_back(input[0]);
      input.remove_prefix(1);
    }
    LowerString(&result.mutable_url_data()->host_part);

    // Verify accumulated host-part is valid.
    StringPiece host_part(result.url_data().host_part);
    if (host_part.empty()) {
      return CspSourceExpression();
    }

    if (host_part[0] == '*' && host_part.size() > 1 && host_part[1] != '.') {
      return CspSourceExpression();
    }

    // Start on port-part, if any
    if (input.starts_with(":")) {
      input.remove_prefix(1);
      if (input.empty()) {
        return CspSourceExpression();
      }

      if (IsDecimalDigit(input[0])) {
        while (!input.empty() && IsDecimalDigit(input[0])) {
          result.mutable_url_data()->port_part.push_back(input[0]);
          input.remove_prefix(1);
        }
      } else if (input[0] == '*') {
        result.mutable_url_data()->port_part = "*";
        input.remove_prefix(1);
      } else {
        return CspSourceExpression();
      }
    }

    // path-part, if any.
    if (!input.empty() && input[0] != '/') {
      return CspSourceExpression();
    }

    // Normalize and tokenize the path.
    StringPieceVector components;
    SplitStringPieceToVector(input, "/", &components, true);

    for (StringPiece c : components) {
      GoogleString canon = GoogleUrl::CanonicalizePath(c);
      if (canon.empty()) {
        LOG(DFATAL) << "Path canonicalization returned empty string?" << c;
        return CspSourceExpression();
      }
      result.mutable_url_data()->path_part.push_back(canon.substr(1));
    }
    result.mutable_url_data()->path_exact_match =
        !input.empty() && !input.ends_with("/");
  }

  return result;
}

bool CspSourceExpression::Matches(
    const GoogleUrl& origin_url, const GoogleUrl& url) const {
  // Implementation of the "Does url match expression in origin with
  // redirect count?" algorithm (where redirect count is 0 for our
  // purposes, since we check the request).
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-list

  if (kind_ != kSelf && kind_ != kSchemeSource && kind_ != kHostSource) {
    return false;
  }

  if (!origin_url.IsAnyValid() || !url.IsAnyValid()) {
    return false;
  }

  // Check for 'self' first, since that doesn't need/have url_data()
  if (kind_ == kSelf) {
    if (origin_url.Origin() == url.Origin()) {
      return true;
    }

    if (origin_url.Host() != url.Host()) {
      return false;
    }

    if (origin_url.SchemeIs("http") && url.SchemeIs("https")) {
      // Using the same port is OK.
      if (origin_url.EffectiveIntPort() == url.EffectiveIntPort()) {
        return true;
      }

      // Using default ports for both is OK, too.
      if (HasDefaultPortForScheme(origin_url) && HasDefaultPortForScheme(url)) {
        return true;
      }
    }

    return false;
  }

  // Give our state some short names closer to those in the spec
  StringPiece expr_scheme = url_data().scheme_part;
  StringPiece expr_host = url_data().host_part;
  StringPiece expr_port = url_data().port_part;
  const std::vector<GoogleString>& expr_path = url_data().path_part;

  // Some special handling of *, which for some reason handles some schemes
  // a bit differently than other things with * host portion and no scheme
  // specified.
  if (kind_ == kHostSource &&
      expr_scheme.empty() &&
      expr_host == "*" &&
      expr_port.empty() &&
      expr_path.empty()) {
    if (url.SchemeIs("http") ||
        url.SchemeIs("https") ||
        url.SchemeIs("ftp")) {
      return true;
    }
    return (url.Scheme() == origin_url.Scheme());
  }

  if (!expr_scheme.empty()
      && url.Scheme() != expr_scheme
      && !(expr_scheme == "http" && url.SchemeIs("https"))) {
    return false;
  }

  if (kind_ == kSchemeSource) {
    return true;
  }

  if (url.Host().empty() || expr_host.empty()) {
    return false;
  }

  if (expr_scheme.empty()
      && url.Scheme() != origin_url.Scheme()
      && !(origin_url.SchemeIs("http") && url.SchemeIs("https"))) {
    return false;
  }

  if (expr_host[0] == '*') {
    StringPiece remaining = expr_host.substr(1);
    if (!url.Host().ends_with(remaining)) {
      return false;
    }
  } else {
    if (url.Host() != expr_host) {
      return false;
    }
  }

  // TODO(morlovich): Implement IP-address handling here, once appropriate
  // spec has been read.

  if (expr_port.empty()) {
    if (!HasDefaultPortForScheme(url)) {
      return false;
    }
  } else {
    // TODO(morlovich): Check whether the :80/:443 case is about effective
    // or explicit port.
    if (expr_port != "*"
        && expr_port != IntegerToString(url.EffectiveIntPort())
        && !(expr_port == "80" && url.EffectiveIntPort() == 443)) {
      return false;
    }
  }

  // TODO(morlovich):Redirect following may require changes here ---
  // this would also be skipped for redirects.
  if (!expr_path.empty()) {
    // TODO(morlovich): Verify that behavior for query here is what we want.
    StringPieceVector url_path_list;
    SplitStringPieceToVector(url.PathAndLeaf(), "/", &url_path_list, true);
    if (expr_path.size() > url_path_list.size()) {
      return false;
    }

    if (url_data().path_exact_match
        && (url_path_list.size() != expr_path.size())) {
      return false;
    }

    for (int i = 0, n = expr_path.size(); i < n; ++i) {
      if (expr_path[i] != url_path_list[i]) {
        return false;
      }
    }
  }

  return true;
}

CspSourceExpression CspSourceExpression::ParseQuoted(StringPiece input) {
  CHECK(!input.empty());

  if (input[0] == 'u' || input[0] == 'U') {
    if (StringCaseEqual(input, "unsafe-inline")) {
      return CspSourceExpression(kUnsafeInline);
    }
    if (StringCaseEqual(input, "unsafe-eval")) {
      return CspSourceExpression(kUnsafeEval);
    }
    if (StringCaseEqual(input, "unsafe-hashed-attributes")) {
      return CspSourceExpression(kUnsafeHashedAttributes);
    }
  }

  if (input[0] == 's' || input[0] == 'S') {
    if (StringCaseEqual(input, "self")) {
      return CspSourceExpression(kSelf);
    }
    if (StringCaseEqual(input, "strict-dynamic")) {
      return CspSourceExpression(kStrictDynamic);
    }
    // TODO(morlovich): Test case sensitivity here and below against spec,
    // potentially file feedback. What's a bit goofy is that the grammar, as
    // interpreted by rules of RFC5234, calls for case-insensitive algorithm
    // names, while the matching algorithm treats them case-sensitively.
    if (StringCaseStartsWith(input, "sha256-") ||
        StringCaseStartsWith(input, "sha384-") ||
        StringCaseStartsWith(input, "sha512-")) {
      input.remove_prefix(7);
      return ParseBase64(input) ? CspSourceExpression(kHashOrNonce)
                                : CspSourceExpression(kUnknown);
    }
  }

  if (StringCaseStartsWith(input, "nonce-")) {
    input.remove_prefix(6);
    return ParseBase64(input) ? CspSourceExpression(kHashOrNonce)
                              : CspSourceExpression(kUnknown);
  }
  return CspSourceExpression(kUnknown);
}

bool CspSourceExpression::ParseBase64(StringPiece input) {
  // base64-value  = 1*( ALPHA / DIGIT / "+" / "/" / "-" / "_" )*2( "=" )
  if (input.empty()) {
    return false;
  }

  while (!input.empty() && IsBase64Char(input[0])) {
    input.remove_prefix(1);
  }

  return input.empty() || (input == "=") || (input == "==");
}

bool CspSourceExpression::HasDefaultPortForScheme(const GoogleUrl& url) {
  int url_scheme_port = GoogleUrl::DefaultPortForScheme(url.Scheme());
  if (url_scheme_port == url::PORT_UNSPECIFIED) {
    return false;
  }

  return (url_scheme_port == url.EffectiveIntPort());
}

std::unique_ptr<CspSourceList> CspSourceList::Parse(StringPiece input) {
  std::unique_ptr<CspSourceList> result(new CspSourceList);

  TrimCspWhitespace(&input);
  StringPieceVector tokens;
  SplitStringPieceToVector(input, " ", &tokens, true);

  // A single token of 'none' is equivalent to an empty list, and means reject.
  //
  // TODO(morlovich): There is some inconsistency with respect to the empty list
  // case in the spec; the grammar doesn't permit one, but the algorithm
  // "Does url match source list in origin with redirect count?" assigns it
  // semantics.
  if (tokens.size() == 1 && StringCaseEqual(tokens[0], "'none'")) {
    return result;
  }

  for (StringPiece token : tokens) {
    TrimCspWhitespace(&token);
    CspSourceExpression expr = CspSourceExpression::Parse(token);
    switch (expr.kind()) {
      case CspSourceExpression::kUnknown:
        // Skip over unknown stuff, it makes no difference anyway.
        break;
      case CspSourceExpression::kUnsafeInline:
        result->saw_unsafe_inline_ = true;
        break;
      case CspSourceExpression::kUnsafeEval:
        result->saw_unsafe_eval_ = true;
        break;
      case CspSourceExpression::kStrictDynamic:
        result->saw_strict_dynamic_ = true;
        break;
      case CspSourceExpression::kUnsafeHashedAttributes:
        result->saw_unsafe_hashed_attributes_ = true;
        break;
      case CspSourceExpression::kHashOrNonce:
        result->saw_hash_or_nonce_ = true;
        break;
      default:
        result->expressions_.push_back(std::move(expr));
        break;
    }
  }

  return result;
}

bool CspSourceList::Matches(
    const GoogleUrl& origin_url, const GoogleUrl& url) const {
  for (const CspSourceExpression& expr : expressions_) {
    if (expr.Matches(origin_url, url)) {
      return true;
    }
  }
  return false;
}

CspPolicy::CspPolicy() {
  policies_.resize(static_cast<size_t>(CspDirective::kNumSourceListDirectives));
}

std::unique_ptr<CspPolicy> CspPolicy::Parse(StringPiece input) {
  std::unique_ptr<CspPolicy> policy;

  TrimCspWhitespace(&input);

  StringPieceVector tokens;
  SplitStringPieceToVector(input, ";", &tokens, true);

  // TODO(morlovich): This will need some extra-careful testing.
  // Essentially the spec has a notion of a policy with an empty directive set,
  // and it basically gets ignored; but is a policy like
  // tasty-chocolate-src: * an empty one, or not? This is particularly
  // relevant since we may not want to parse worker-src or whatever.
  if (tokens.empty()) {
    return policy;
  }

  policy.reset(new CspPolicy);
  for (StringPiece token : tokens) {
    TrimCspWhitespace(&token);
    StringPiece::size_type pos = token.find(' ');
    if (pos != StringPiece::npos) {
      StringPiece name = token.substr(0, pos);
      StringPiece value = token.substr(pos + 1);
      CspDirective dir_name = LookupCspDirective(name);
      int dir_name_num = static_cast<int>(dir_name);
      if (dir_name != CspDirective::kNumSourceListDirectives &&
          policy->policies_[dir_name_num] == nullptr) {
        // Note: repeated directives are ignored per the "Parse a serialized
        // CSP as disposition" algorithm.
        // https://w3c.github.io/webappsec-csp/#parse-serialized-policy
        policy->policies_[dir_name_num] = CspSourceList::Parse(value);
      }
    } else {
      // Empty policy
      CspDirective dir_name = LookupCspDirective(token);
      int dir_name_num = static_cast<int>(dir_name);
      if (dir_name != CspDirective::kNumSourceListDirectives &&
          policy->policies_[dir_name_num] == nullptr) {
        policy->policies_[dir_name_num].reset(new CspSourceList());
      }
    }
  }

  return policy;
}

bool CspPolicy::PermitsEval() const {
  // AKA EnsureCSPDoesNotBlockStringCompilation() from the spec.
  // https://w3c.github.io/webappsec-csp/#can-compile-strings
  const CspSourceList* relevant_list = SourceListFor(CspDirective::kScriptSrc);
  if (relevant_list == nullptr) {
    relevant_list = SourceListFor(CspDirective::kDefaultSrc);
  }

  return (relevant_list == nullptr || relevant_list->saw_unsafe_eval());
}

bool CspPolicy::PermitsInlineScript() const {
  const CspSourceList* script_src = SourceListFor(CspDirective::kScriptSrc);
  if (script_src == nullptr) {
    return true;
  }

  if (script_src->saw_strict_dynamic()) {
    return false;
  }

  return (script_src->saw_unsafe_inline() && !script_src->saw_hash_or_nonce());
}

bool CspPolicy::PermitsInlineScriptAttribute() const {
  const CspSourceList* script_src = SourceListFor(CspDirective::kScriptSrc);
  if (script_src == nullptr) {
    return true;
  }

  if (script_src->saw_strict_dynamic() &&
      !script_src->saw_unsafe_hashed_attributes()) {
    return false;
  }

  return (script_src->saw_unsafe_inline() && !script_src->saw_hash_or_nonce());
}

bool CspPolicy::PermitsInlineStyle() const {
  const CspSourceList* style_src = SourceListFor(CspDirective::kStyleSrc);
  if (style_src == nullptr) {
    return true;
  }

  if (style_src->saw_strict_dynamic()) {
    return false;
  }

  return (style_src->saw_unsafe_inline() && !style_src->saw_hash_or_nonce());
}

bool CspPolicy::PermitsInlineStyleAttribute() const {
  return PermitsInlineStyle();
}

bool CspPolicy::CanLoadUrl(
    CspDirective role, const GoogleUrl& origin_url,
    const GoogleUrl& url) const {
  // AKA: "Does url match source list in origin with redirect count?", combined
  // with the various pre-request checks.
  CHECK(role == CspDirective::kImgSrc || role == CspDirective::kStyleSrc ||
        role == CspDirective::kScriptSrc);
  const CspSourceList* source_list = SourceListFor(role);
  if (source_list == nullptr) {
    source_list = SourceListFor(CspDirective::kDefaultSrc);
  }

  if (source_list == nullptr) {
    // No source list permits loading, empty doesn't.
    return true;
  }

  return source_list->Matches(origin_url, url);
}

bool CspPolicy::IsBasePermitted(
    const GoogleUrl& previous_origin, const GoogleUrl& base_candidate) const {
  const CspSourceList* source_list = SourceListFor(CspDirective::kBaseUri);
  if (source_list != nullptr) {
    if (!source_list->Matches(previous_origin, base_candidate)) {
      return false;
    }
  }
  return true;
}

void CspContext::AddPolicy(std::unique_ptr<CspPolicy> policy) {
  if (policy != nullptr) {
    policies_.push_back(std::move(policy));
  }
}

}  // namespace net_instaweb
