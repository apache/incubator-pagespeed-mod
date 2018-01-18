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


#include "pagespeed/kernel/base/string_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

bool StringToDouble(const char* in, double* out) {
  char* endptr;
  *out = strtod(in, &endptr);
  if (endptr != in) {
    while (IsHtmlSpace(*endptr)) ++endptr;
  }
  // Ignore range errors from strtod.  The values it
  // returns on underflow and overflow are the right
  // fallback in a robust setting.
  return *in != '\0' && *endptr == '\0';
}

GoogleString StrCat(StringPiece a, StringPiece b) {
  GoogleString res;
  res.reserve(a.size() + b.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c,
                    StringPiece d) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c, StringPiece d,
                    StringPiece e) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c, StringPiece d,
                    StringPiece e, StringPiece f) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c, StringPiece d,
                    StringPiece e, StringPiece f, StringPiece g) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  return res;
}
GoogleString StrCat(StringPiece a, StringPiece b, StringPiece c, StringPiece d,
                    StringPiece e, StringPiece f, StringPiece g,
                    StringPiece h) {
  GoogleString res;
  res.reserve(a.size() + b.size() + c.size() + d.size() + e.size() + f.size() +
              g.size() + h.size());
  a.AppendToString(&res);
  b.AppendToString(&res);
  c.AppendToString(&res);
  d.AppendToString(&res);
  e.AppendToString(&res);
  f.AppendToString(&res);
  g.AppendToString(&res);
  h.AppendToString(&res);
  return res;
}

namespace internal {

GoogleString StrCatNineOrMore(const StringPiece* a, ...) {
  GoogleString res;

  va_list args;
  va_start(args, a);
  size_t size = a->size();
  while (const StringPiece* arg = va_arg(args, const StringPiece*)) {
    size += arg->size();
  }
  res.reserve(size);
  va_end(args);
  va_start(args, a);
  a->AppendToString(&res);
  while (const StringPiece* arg = va_arg(args, const StringPiece*)) {
    arg->AppendToString(&res);
  }
  va_end(args);
  return res;
}

}  // namespace internal

void StrAppend(GoogleString* target, StringPiece a, StringPiece b) {
  target->reserve(target->size() +
                  a.size() + b.size());
  a.AppendToString(target);
  b.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d, StringPiece e) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d, StringPiece e, StringPiece f) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d, StringPiece e, StringPiece f,
               StringPiece g) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d, StringPiece e, StringPiece f,
               StringPiece g, StringPiece h) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size() + h.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
  h.AppendToString(target);
}
void StrAppend(GoogleString* target, StringPiece a, StringPiece b,
               StringPiece c, StringPiece d, StringPiece e, StringPiece f,
               StringPiece g, StringPiece h, StringPiece i) {
  target->reserve(target->size() +
                  a.size() + b.size() + c.size() + d.size() + e.size() +
                  f.size() + g.size() + h.size() + i.size());
  a.AppendToString(target);
  b.AppendToString(target);
  c.AppendToString(target);
  d.AppendToString(target);
  e.AppendToString(target);
  f.AppendToString(target);
  g.AppendToString(target);
  h.AppendToString(target);
  i.AppendToString(target);
}

void SplitStringPieceToVector(StringPiece sp, StringPiece separators,
                              StringPieceVector* components,
                              bool omit_empty_strings) {
  size_t prev_pos = 0;
  size_t pos = 0;
  while ((pos = sp.find_first_of(separators, pos)) != StringPiece::npos) {
    if (!omit_empty_strings || (pos > prev_pos)) {
      components->push_back(sp.substr(prev_pos, pos - prev_pos));
    }
    ++pos;
    prev_pos = pos;
  }
  if (!omit_empty_strings || (prev_pos < sp.size())) {
    components->push_back(sp.substr(prev_pos, prev_pos - sp.size()));
  }
}

void SplitStringUsingSubstr(StringPiece full, StringPiece substr,
                            StringPieceVector* result) {
  StringPiece::size_type begin_index = 0;
  while (true) {
    const StringPiece::size_type end_index = full.find(substr, begin_index);
    if (end_index == StringPiece::npos) {
      const StringPiece term = full.substr(begin_index);
      result->push_back(term);
      return;
    }
    const StringPiece term = full.substr(begin_index, end_index - begin_index);
    if (!term.empty()) {
      result->push_back(term);
    }
    begin_index = end_index + substr.size();
  }
}

void BackslashEscape(StringPiece src, StringPiece to_escape,
                     GoogleString* dest) {
  dest->reserve(dest->size() + src.size());
  for (const char *p = src.data(), *end = src.data() + src.size();
       p != end; ++p) {
    if (to_escape.find(*p) != StringPiece::npos) {
      dest->push_back('\\');
    }
    dest->push_back(*p);
  }
}

GoogleString CEscape(StringPiece src) {
  int len = src.length();
  const char* read = src.data();
  const char* end = read + len;
  int used = 0;
  char* dest = new char[len * 4 + 1];
  for (; read != end; ++read) {
    unsigned char ch = static_cast<unsigned char>(*read);
    switch (ch) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n'; break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r'; break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't'; break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default:
        if (ch < 32 || ch >= 127) {
          base::snprintf(dest + used, 5, "\\%03o", ch);  // NOLINT
          used += 4;
        } else {
          dest[used++] = ch;
        }
        break;
    }
  }
  GoogleString final(dest, used);
  delete[] dest;
  return final;
}

// From src/third_party/protobuf/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
bool HasPrefixString(StringPiece str, StringPiece prefix) {
  return str.starts_with(prefix);
}

// From src/third_party/protobuf/src/google/protobuf/stubs/strutil.h
// but we don't need any other aspect of protobufs so we don't want to
// incur the link cost.
void UpperString(GoogleString* s) {
  GoogleString::iterator end = s->end();
  for (GoogleString::iterator i = s->begin(); i != end; ++i) {
    *i = UpperChar(*i);
  }
}

void LowerString(GoogleString* s) {
  GoogleString::iterator end = s->end();
  for (GoogleString::iterator i = s->begin(); i != end; ++i) {
    *i = LowerChar(*i);
  }
}

// ----------------------------------------------------------------------
// GlobalReplaceSubstring()
//    Replaces all instances of a substring in a string.  Returns the
//    number of replacements.
//
//    NOTE: The string pieces must not overlap s.
// ----------------------------------------------------------------------
int GlobalReplaceSubstring(StringPiece substring, StringPiece replacement,
                           GoogleString* s) {
  CHECK(s != NULL);
  if (s->empty())
    return 0;
  GoogleString tmp;
  int num_replacements = 0;
  size_t pos = 0;
  for (size_t match_pos = s->find(substring.data(), pos, substring.length());
       match_pos != GoogleString::npos;
       pos = match_pos + substring.length(),
           match_pos = s->find(substring.data(), pos, substring.length())) {
    ++num_replacements;
    // Append the original content before the match.
    tmp.append(*s, pos, match_pos - pos);
    // Append the replacement for the match.
    tmp.append(replacement.begin(), replacement.end());
  }
  // Append the content after the last match. If no replacements were made, the
  // original string is left untouched.
  if (num_replacements > 0) {
    tmp.append(*s, pos, s->length() - pos);
    s->swap(tmp);
  }
  return num_replacements;
}

// Erase shortest substrings in string bracketed by left and right.
int GlobalEraseBracketedSubstring(StringPiece left, StringPiece right,
                                  GoogleString* string) {
  int deletions = 0;
  size_t keep_start = 0;
  size_t keep_end = string->find(left.data(), keep_start, left.size());
  if (keep_end == GoogleString::npos) {
    // Fast path without allocation for no occurrences of left.
    return 0;
  }
  GoogleString result;
  result.reserve(string->size());
  while (keep_end != GoogleString::npos) {
    result.append(*string, keep_start, keep_end - keep_start);
    keep_start =
        string->find(right.data(), keep_end + left.size(), right.size());
    if (keep_start == GoogleString::npos) {
      keep_start = keep_end;
      break;
    }
    keep_start += right.size();
    ++deletions;
    keep_end = string->find(left.data(), keep_start, left.size());
  }
  result.append(*string, keep_start, string->size() - keep_start);
  string->swap(result);
  string->reserve(string->size());  // shrink_to_fit, C++99-style
  return deletions;
}

GoogleString JoinStringStar(const ConstStringStarVector& vector,
                            StringPiece delim) {
  GoogleString result;

  if (vector.size() > 0) {
    // Precompute resulting length so we can reserve() memory in one shot.
    int length = delim.size() * (vector.size() - 1);
    for (ConstStringStarVector::const_iterator iter = vector.begin();
         iter < vector.end(); ++iter) {
      length += (*iter)->size();
    }
    result.reserve(length);

    // Now combine everything.
    for (ConstStringStarVector::const_iterator iter = vector.begin();
         iter < vector.end(); ++iter) {
      if (iter != vector.begin()) {
        result.append(delim.data(), delim.size());
      }
      result.append(**iter);
    }
  }

  return result;
}

bool StringCaseStartsWith(StringPiece str, StringPiece prefix) {
  return ((str.size() >= prefix.size()) &&
          (0 == StringCaseCompare(prefix, str.substr(0, prefix.size()))));
}

bool StringCaseEndsWith(StringPiece str, StringPiece suffix) {
  return ((str.size() >= suffix.size()) &&
          (0 == StringCaseCompare(suffix,
                                  str.substr(str.size() - suffix.size()))));
}

bool StringEqualConcat(StringPiece str, StringPiece first, StringPiece second) {
  return (str.size() == first.size() + second.size()) &&
         strings::StartsWith(str, first) && strings::EndsWith(str, second);
}

StringPiece PieceAfterEquals(StringPiece piece) {
  size_t index = piece.find("=");
  if (index != piece.npos) {
    ++index;
    StringPiece ret = piece;
    ret.remove_prefix(index);
    TrimWhitespace(&ret);
    return ret;
  }
  return StringPiece(piece.data(), 0);
}

int CountCharacterMismatches(StringPiece s1, StringPiece s2) {
  int s1_length = s1.size();
  int s2_length = s2.size();
  int mismatches = 0;
  for (int i = 0, n = std::min(s1_length, s2_length); i < n; ++i) {
    mismatches += s1[i] != s2[i];
  }
  return mismatches + std::abs(s1_length - s2_length);
}

void ParseShellLikeString(StringPiece input,
                          std::vector<GoogleString>* output) {
  output->clear();
  for (size_t index = 0; index < input.size();) {
    const char ch = input[index];
    if (ch == '"' || ch == '\'') {
      // If we see a quoted section, treat it as a single item even if there are
      // spaces in it.
      const char quote = ch;
      ++index;  // skip open quote
      output->push_back("");
      GoogleString& part = output->back();
      for (; index < input.size() && input[index] != quote; ++index) {
        if (input[index] == '\\') {
          ++index;  // skip backslash
          if (index >= input.size()) {
            break;
          }
        }
        part.push_back(input[index]);
      }
      ++index;  // skip close quote
    } else if (!IsHtmlSpace(ch)) {
      // Without quotes, items are whitespace-separated.
      output->push_back("");
      GoogleString& part = output->back();
      for (; index < input.size() && !IsHtmlSpace(input[index]); ++index) {
        part.push_back(input[index]);
      }
    } else {
      // Ignore whitespace (outside of quotes).
      ++index;
    }
  }
}

int CountSubstring(StringPiece text, StringPiece substring) {
  int number_of_occurrences = 0;
  size_t pos = 0;
  for (size_t match_pos = text.find(substring, pos);
       match_pos != StringPiece::npos;
       pos = match_pos + 1, match_pos = text.find(substring, pos)) {
    number_of_occurrences++;
  }
  return number_of_occurrences;
}

stringpiece_ssize_type FindIgnoreCase(
    StringPiece haystack, StringPiece needle) {
  stringpiece_ssize_type pos = 0;
  while (haystack.size() >= needle.size()) {
    if (StringCaseStartsWith(haystack, needle)) {
      return pos;
    }
    ++pos;
    haystack.remove_prefix(1);
  }
  return StringPiece::npos;
}


// In-place StringPiece whitespace trimming.  This mutates the StringPiece.
bool TrimLeadingWhitespace(StringPiece* str) {
  // We use stringpiece_ssize_type to avoid signedness problems.
  stringpiece_ssize_type size = str->size();
  stringpiece_ssize_type trim = 0;
  while (trim != size && IsHtmlSpace(str->data()[trim])) {
    ++trim;
  }
  str->remove_prefix(trim);
  return (trim > 0);
}

bool TrimTrailingWhitespace(StringPiece* str) {
  // We use stringpiece_ssize_type to avoid signedness problems.
  stringpiece_ssize_type rightmost = str->size();
  while (rightmost != 0 && IsHtmlSpace(str->data()[rightmost - 1])) {
    --rightmost;
  }
  if (rightmost != str->size()) {
    str->remove_suffix(str->size() - rightmost);
    return true;
  }
  return false;
}

bool TrimWhitespace(StringPiece* str) {
  // We *must* trim *both* leading and trailing spaces, so we use the
  // non-shortcut bitwise | on the boolean results.
  return TrimLeadingWhitespace(str) | TrimTrailingWhitespace(str);
}

namespace {

bool TrimCasePattern(StringPiece pattern, StringPiece* str) {
  bool did_something = false;
  if (StringCaseStartsWith(*str, pattern)) {
    str->remove_prefix(pattern.size());
    did_something = true;
  }
  if (StringCaseEndsWith(*str, pattern)) {
    str->remove_suffix(pattern.size());
    did_something = false;
  }
  return did_something;
}

}  // namespace

void TrimQuote(StringPiece* str) {
  TrimWhitespace(str);
  if (strings::StartsWith(*str, "\"") || strings::StartsWith(*str, "'")) {
    str->remove_prefix(1);
  }
  if (strings::EndsWith(*str, "\"") || strings::EndsWith(*str, "'")) {
    str->remove_suffix(1);
  }
  TrimWhitespace(str);
}

void TrimUrlQuotes(StringPiece* str) {
  TrimWhitespace(str);

  bool cont = true;

  // Unwrap a string with an arbitrary nesting of real and URL percent-encoded
  // quotes.  We do this one layer at a time, always removing backslashed
  // quotes before removing un-backslashed quotes.
  while (cont) {
    cont = (TrimCasePattern("%5C%27", str) ||    // \"
            TrimCasePattern("%5C%22", str) ||    // \'
            TrimCasePattern("%27", str) ||       // "
            TrimCasePattern("%22", str) ||       // '
            TrimCasePattern("\"", str) ||
            TrimCasePattern("'", str));
  }
  TrimWhitespace(str);
}

// TODO(jmarantz): This is a very slow implementation.  But strncasecmp
// will fail test StringCaseTest.Locale.  If this shows up as a performance
// bottleneck then an SSE implementation would be better.
int StringCaseCompare(StringPiece s1, StringPiece s2) {
  for (int i = 0, n = std::min(s1.size(), s2.size()); i < n; ++i) {
    unsigned char c1 = UpperChar(s1[i]);
    unsigned char c2 = UpperChar(s2[i]);
    if (c1 < c2) {
      return -1;
    } else if (c1 > c2) {
      return 1;
    }
  }
  if (s1.size() < s2.size()) {
    return -1;
  } else if (s1.size() > s2.size()) {
    return 1;
  }
  return 0;
}

bool MemCaseEqual(const char* s1, size_t size1, const char* s2, size_t size2) {
  if (size1 != size2) {
    return false;
  }
  for (; size1 != 0; --size1, ++s1, ++s2) {
    if (UpperChar(*s1) != UpperChar(*s2)) {
      return false;
    }
  }
  return true;
}

bool SplitStringPieceToIntegerVector(StringPiece src, StringPiece separators,
                                     std::vector<int>* ints) {
  StringPieceVector values;
  SplitStringPieceToVector(
      src, separators, &values, true /* omit_empty_strings */);
  ints->clear();
  int v;
  for (int i = 0, n = values.size(); i < n; ++i) {
    if (StringToInt(values[i], &v)) {
      ints->push_back(v);
    } else {
      ints->clear();
      return false;
    }
  }
  return true;
}

}  // namespace net_instaweb
