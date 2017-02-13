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
// Content-Security-Policy that's relevant for PageSpeed Automatic

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

inline bool IsAsciiDigit(char ch) {
  return ((ch >= '0') && (ch <= '9'));
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
  while (pos < input->size() &&
         (IsAsciiAlphaNumeric((*input)[pos]) 
          || (*input)[pos] == '+' 
          || (*input)[pos] == '-' 
          || (*input)[pos] == '.')) {
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
    return true;
  }
  
  // We now want to see if it's actually foo://
  if ((pos + 2 < input->size())
       && ((*input)[pos + 1] == '/') && ((*input)[pos + 2] == '/')) {
    input->substr(0, pos).CopyToString(&mutable_url_data()->scheme_part);
    input->remove_prefix(pos + 3);
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
    
    // Verify accumulated host-part is valid.
    StringPiece host_part(result.url_data().host_part);    
    if (host_part.empty()) {
      return CspSourceExpression();
    }
    
    if (host_part.starts_with("*")) {
      if (!host_part.starts_with("*.") && (host_part != "*")) {
        return CspSourceExpression();
      }
    }
    
    // Start on port-part, if any
    if (input.starts_with(":")) {
      input.remove_prefix(1);
      if (input.empty()) {
        return CspSourceExpression();
      }
      
      if (IsAsciiDigit(input[0])) {
        while (!input.empty() && IsAsciiDigit(input[0])) {
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
      
    input.CopyToString(&result.mutable_url_data()->path_part);
  }
  
  return result;
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
  }
  return CspSourceExpression(kUnknown);
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
    if (expr.kind() != CspSourceExpression::kUnknown) {
      result->expressions_.push_back(std::move(expr));
    }
  }

  return result;
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
      if (dir_name != CspDirective::kNumSourceListDirectives &&
          policy->policies_[static_cast<int>(dir_name)] == nullptr) {
        // Note: repeated directives are ignored per the "Parse a serialized
        // CSP as disposition" algorith,
        policy->policies_[static_cast<int>(dir_name)]
            = CspSourceList::Parse(value);
      }
    }
  }

  return policy;
}

}  // namespace net_instaweb
