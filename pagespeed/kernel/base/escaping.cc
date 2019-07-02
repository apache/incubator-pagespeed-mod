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


#include "pagespeed/kernel/base/escaping.h"

#include <cstddef>

#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// We escape backslash, double-quote, CR and LF while forming a string
// from the code. Single quotes are escaped as well, if we don't know we're
// explicitly double-quoting.  Appends to *escaped.
//
// This is /almost/ completely right: U+2028 and U+2029 are
// line terminators as well (ECMA 262-5 --- 7.3, 7.8.4), so should really be
// escaped, too, but we don't have the encoding here.
void EscapeToJsStringLiteral(const StringPiece& original,
                             bool add_quotes,
                             GoogleString* escaped) {
  // Optimistically assume no escaping will be required and reserve enough space
  // for that result.  This assumes that either escaped is empty (or nearly so),
  // or reserve(...) behaves sanely and only vector doubles rather than
  // increasing size linearly.  The latter is true in gcc at least (but not true
  // of some implementations of std::vector, thus the caveat).
  escaped->reserve(escaped->size() + original.size() + (add_quotes ? 2 : 0));
  if (add_quotes) {
    (*escaped) += "\"";
  }
  for (size_t c = 0; c < original.length(); ++c) {
    switch (original[c]) {
      case '\\':
        (*escaped) += "\\\\";
        break;
      case '"':
        (*escaped) += "\\\"";
        break;
      case '\r':
        (*escaped) += "\\r";
        break;
      case '\n':
        (*escaped) += "\\n";
        break;
      case '\'':
        if (!add_quotes) {
          (*escaped) += "\\'";
        } else {
          (*escaped) += '\'';
        }
        break;
      case '<': {
        // Surprisingly, seeing <!-- and <script can affect how parsing
        // of scripts inside HTML works, so we need to escape the <
        // in them.
        // (See the "script data escaped" HTML lexer states in the HTML5 spec).
        StringPiece rest_of_input = original.substr(c);
        if (StringCaseStartsWith(rest_of_input, "<script") ||
            strings::StartsWith(rest_of_input, "<!--")) {
          *(escaped) += "\\u003c";
        } else {
          *(escaped) += '<';
        }
        break;
      }
      case '-': {
        // Similarly to <!-- (see above) --> can be special.
        if (strings::StartsWith(original.substr(c), "-->")) {
          *(escaped) += "\\u002d";
        } else {
          *(escaped) += '-';
        }
        break;
      }
      case '/':
        // Forward slashes are generally OK, but </script> is trouble
        // if it happens inside an inline <script>. We therefore escape the
        // forward slash if we see /script>
        if (StringCaseStartsWith(original.substr(c), "/script")) {
          (*escaped) += '\\';
        }
        FALLTHROUGH_INTENDED;
      default:
        (*escaped) += original[c];
    }
  }
  if (add_quotes) {
    (*escaped) += "\"";
  }
}

void EscapeToJsonStringLiteral(const StringPiece& original,
                               bool add_quotes,
                               GoogleString* escaped) {
  // Optimistically assume no escaping will be required and reserve enough space
  // for that result.
  escaped->reserve(escaped->size() + original.size() + (add_quotes ? 2 : 0));
  if (add_quotes) {
    (*escaped) += "\"";
  }
  for (size_t c = 0; c < original.length(); ++c) {
    unsigned char code = static_cast<unsigned char>(original[c]);

    if (code <= 0x1F || code > 0x7F || code == '<' || code == '>' ||
        code == '"' || code == '\\') {
      *(escaped) += StringPrintf("\\u00%02x", code);
    } else {
      *(escaped) += original[c];
    }
  }
  if (add_quotes) {
    (*escaped) += "\"";
  }
}

}  // namespace net_instaweb
