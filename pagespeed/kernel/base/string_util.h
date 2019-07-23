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

#ifndef PAGESPEED_KERNEL_BASE_STRING_UTIL_H_
#define PAGESPEED_KERNEL_BASE_STRING_UTIL_H_

#include <cctype> // for isascii
#include <cstddef>
#include <map>
#include <set>
#include <vector>

#include <iostream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
// XXX(oschaaf): internal util
#include "absl/strings/internal/memutil.h"
#include "absl/strings/match.h"

//#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

#include "fmt/printf.h"

#include <cstdlib> // NOLINT
#include <string>  // NOLINT

static const int32 kint32max = 0x7FFFFFFF;
static const int32 kint32min = -kint32max - 1;

typedef absl::string_view StringPiece;
using namespace absl;

#include "fmt/format.h"

// XXX(oschaaf): modified copy of what chromium base has. check this one 
// very carefully.
inline void StringAppendV(std::string* dst, const char* format, va_list ap) {
  // First try with a small fixed size buffer.
  // This buffer size should be kept in sync with StringUtilTest.GrowBoundary
  // and StringUtilTest.StringPrintfBounds.
  std::string::value_type stack_buf[1024];

  va_list ap_copy;
  va_copy(ap_copy, ap);

#if !defined(OS_WIN)
  errno = 0;
#endif
  int result = vsnprintf(stack_buf, arraysize(stack_buf), format, ap_copy);
  va_end(ap_copy);

  if (result >= 0 && result < static_cast<int>(arraysize(stack_buf))) {
    // It fit.
    dst->append(stack_buf, result);
    return;
  }

  // Repeatedly increase buffer size until it fits.
  int mem_length = arraysize(stack_buf);
  while (true) {
    if (result < 0) {
#if !defined(OS_WIN)
      // On Windows, vsnprintfT always returns the number of characters in a
      // fully-formatted string, so if we reach this point, something else is
      // wrong and no amount of buffer-doubling is going to fix it.
      if (errno != 0 && errno != EOVERFLOW)
#endif
      {
        // If an error other than overflow occurred, it's never going to work.
        DLOG(WARNING) << "Unable to printf the requested string due to error.";
        return;
      }
      // Try doubling the buffer size.
      mem_length *= 2;
    } else {
      // We need exactly "result + 1" characters.
      mem_length = result + 1;
    }

    if (mem_length > 32 * 1024 * 1024) {
      // That should be plenty, don't try anything larger.  This protects
      // against huge allocations when using vsnprintfT implementations that
      // return -1 for reasons other than overflow without setting errno.
      DLOG(WARNING) << "Unable to printf the requested string due to size.";
      return;
    }

    std::vector<std::string::value_type> mem_buf(mem_length);

    // NOTE: You can only use a va_list once.  Since we're in a while loop, we
    // need to make a new copy each time so we don't use up the original.
    va_copy(ap_copy, ap);
    result = vsnprintf(&mem_buf[0], mem_length, format, ap_copy);
    va_end(ap_copy);

    if ((result >= 0) && (result < mem_length)) {
      // It fit.
      dst->append(&mem_buf[0], result);
      return;
    }
  }
}

// TODO(oschaaf): re-implemented these chromium:base functions
// with variadic template ones leaning upon fmt.
template <typename... Args>
inline void StringAppendF(std::string *dst, const char *format,
                          const Args &... args) {
  dst->append(fmt::sprintf(format, args...));
}

// TODO(oschaaf): changed return type, its never used. voided it.
template <typename... Args>
inline void SStringPrintf(std::string *dst, const char *format,
                          const Args &... args) {
  *dst = fmt::sprintf(format, args...);
}

template <typename... Args>
inline std::string StringPrintf(const char *format, const Args &... args) {
  return fmt::sprintf(format, args...);
}

// XXX(oschaaf): check where ssize_t is used (!!)
typedef size_t stringpiece_ssize_type;

namespace strings {
inline bool StartsWith(StringPiece a, StringPiece b) {
  return absl::StartsWith(a, b);
}
inline bool EndsWith(StringPiece a, StringPiece b) {
  return absl::EndsWith(a, b);
}
} // namespace strings

// Quick macro to get the size of a static char[] without trailing '\0'.
// Note: Cannot be used for char*, std::string, etc.

#define STATIC_STRLEN(static_string) (arraysize(static_string) - 1)

namespace net_instaweb {

struct StringCompareInsensitive;

typedef std::map<GoogleString, GoogleString> StringStringMap;
typedef std::map<GoogleString, int> StringIntMap;
typedef std::set<GoogleString> StringSet;
typedef std::set<GoogleString, StringCompareInsensitive> StringSetInsensitive;
typedef std::vector<GoogleString> StringVector;
typedef std::vector<StringPiece> StringPieceVector;
typedef std::vector<const GoogleString *> ConstStringStarVector;
typedef std::vector<GoogleString *> StringStarVector;
typedef std::vector<const char *> CharStarVector;

inline GoogleString IntegerToString(int i) { return fmt::format("{}", i); }

inline GoogleString UintToString(unsigned int i) {
  return fmt::format("{}", i);
}

inline GoogleString Integer64ToString(int64 i) { return fmt::format("{}", i); }

inline GoogleString PointerToString(void *pointer) {
  return fmt::format("{}", pointer);
}

// NOTE: For a string of the form "45x", this sets *out = 45 but returns false.
// It sets *out = 0 given "Junk45" or "".
inline bool StringToInt(absl::string_view in, int *out) {
  return absl::SimpleAtoi<int>(in, out);
}

inline bool StringToInt64(absl::string_view in, int64 *out) {
  return absl::SimpleAtoi<int64>(in, out);
}

inline bool StringToInt(const GoogleString &in, int *out) {
  return absl::SimpleAtoi<int>(in, out);
}

inline bool StringToInt64(const GoogleString &in, int64 *out) {
  return absl::SimpleAtoi<int64>(in, out);
}

// Parses valid floating point number and returns true if string contains only
// that floating point number (ignoring leading/trailing whitespace).
// Note: This also parses hex and exponential float notation.
bool StringToDouble(const char *in, double *out);

inline bool StringToDouble(GoogleString in, double *out) {
  const char *in_c_str = in.c_str();
  if (strlen(in_c_str) != in.size()) {
    // If there are embedded nulls, always fail.
    return false;
  }
  return StringToDouble(in_c_str, out);
}

inline bool StringToDouble(StringPiece in, double *out) {
  return StringToDouble(GoogleString(in), out);
}

// Returns the part of the piece after the first '=', trimming any
// white space found at the beginning or end of the resulting piece.
// Returns an empty string if '=' was not found.
StringPiece PieceAfterEquals(StringPiece piece);

// Split sp into pieces that are separated by any character in the given string
// of separators, and push those pieces in order onto components.
void SplitStringPieceToVector(StringPiece sp, StringPiece separators,
                              StringPieceVector *components,
                              bool omit_empty_strings);

// Splits string 'full' using substr by searching it incrementally from
// left. Empty tokens are removed from the final result.
void SplitStringUsingSubstr(StringPiece full, StringPiece substr,
                            StringPieceVector *result);

void BackslashEscape(StringPiece src, StringPiece to_escape,
                     GoogleString *dest);

GoogleString CEscape(StringPiece src);

// TODO(jmarantz): Eliminate these definitions of HasPrefixString,
// UpperString, and LowerString, and re-add dependency on protobufs
// which also provide definitions for these.

bool HasPrefixString(StringPiece str, StringPiece prefix);

void UpperString(GoogleString *str);

void LowerString(GoogleString *str);

inline bool OnlyWhitespace(const GoogleString &str) {
  return absl::StripAsciiWhitespace(str).empty();
}

// Replaces all instances of 'substring' in 's' with 'replacement'.
// Returns the number of instances replaced.  Replacements are not
// subject to re-matching.
//
// NOTE: The string pieces must not overlap 's'.
int GlobalReplaceSubstring(StringPiece substring, StringPiece replacement,
                           GoogleString *s);

// Returns the index of the start of needle in haystack, or
// StringPiece::npos if it's not present.
StringPiece::size_type FindIgnoreCase(StringPiece haystack, StringPiece needle);

// Erase shortest substrings in string bracketed by left and right, working
// from the left.
// ("[", "]", "abc[def]g[h]i]j[k") -> "abcgi]j[k"
// Returns the number of substrings erased.
int GlobalEraseBracketedSubstring(StringPiece left, StringPiece right,
                                  GoogleString *string);

// Output a string which is the combination of all values in vector, separated
// by delim. Does not ignore empty strings in vector. So:
// JoinStringStar({"foo", "", "bar"}, ", ") == "foo, , bar". (Pseudocode)
GoogleString JoinStringStar(const ConstStringStarVector &vector,
                            StringPiece delim);

// See also: ./src/third_party/css_parser/src/strings/ascii_ctype.h
// We probably don't want our core string header file to have a
// dependecy on the Google CSS parser, so for now we'll write this here:

// upper-case a single character and return it.
// toupper() changes based on locale.  We don't want this!
inline char UpperChar(char c) {
  if ((c >= 'a') && (c <= 'z')) {
    c += 'A' - 'a';
  }
  return c;
}

// lower-case a single character and return it.
// tolower() changes based on locale.  We don't want this!
inline char LowerChar(char c) {
  if ((c >= 'A') && (c <= 'Z')) {
    c += 'a' - 'A';
  }
  return c;
}

// Check if given character is an HTML (or CSS) space (not the same as isspace,
// and not locale-dependent!).  Note in particular that isspace always includes
// '\v' and HTML does not.  See:
//    http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#space-character
//    http://www.w3.org/TR/CSS21/grammar.html
inline char IsHtmlSpace(char c) {
  return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n') || (c == '\f');
}

/* inline char* strdup(const char* str) {
  return absl::strdup(str);
}*/

// Case-insensitive string comparison that is locale-independent.
int StringCaseCompare(StringPiece s1, StringPiece s2);

// Determines whether the character is a US Ascii number or letter.  This
// is preferable to isalnum() for working with computer languages, as
// opposed to human languages.
inline bool IsAsciiAlphaNumeric(char ch) {
  return (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) ||
          ((ch >= '0') && (ch <= '9')));
}

// Convenience functions.
inline bool IsHexDigit(char c) {
  return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') ||
         ('a' <= c && c <= 'f');
}

inline bool IsDecimalDigit(char c) { return (c >= '0' && c <= '9'); }

// In-place removal of leading and trailing HTML whitespace.  Returns true if
// any whitespace was trimmed.
bool TrimWhitespace(StringPiece *str);

// In-place removal of leading and trailing quote.  Removes whitespace as well.
void TrimQuote(StringPiece *str);

// In-place removal of multiple levels of leading and trailing quotes,
// include url-escaped quotes, optionally backslashed.  Removes
// whitespace as well.
void TrimUrlQuotes(StringPiece *str);

// Trims leading HTML whitespace.  Returns true if any whitespace was trimmed.
bool TrimLeadingWhitespace(StringPiece *str);

// Trims trailing HTML whitespace.  Returns true if any whitespace was trimmed.
bool TrimTrailingWhitespace(StringPiece *str);

// Non-destructive TrimWhitespace.
// WARNING: in should not point inside output!
inline void TrimWhitespace(StringPiece in, GoogleString *output) {
  DCHECK((in.data() < output->data()) ||
         (in.data() >= (output->data() + output->length())))
      << "Illegal argument aliasing in TrimWhitespace";
  StringPiece temp(in);  // Mutable copy
  TrimWhitespace(&temp); // Modifies temp
  *output = GoogleString(temp);
}

// Accumulates a decimal value from 'c' into *value.
// Returns false and leaves *value unchanged if c is not a decimal digit.
bool AccumulateDecimalValue(char c, uint32 *value);

// Accumulates a hex value from 'c' into *value
// Returns false and leaves *value unchanged if c is not a hex digit.
bool AccumulateHexValue(char c, uint32 *value);

// Return true iff the two strings are equal, ignoring case.
bool MemCaseEqual(const char *s1, size_t size1, const char *s2, size_t size2);
inline bool StringCaseEqual(StringPiece s1, StringPiece s2) {
  return MemCaseEqual(s1.data(), s1.size(), s2.data(), s2.size());
}

// Return true iff str starts with prefix, ignoring case.
bool StringCaseStartsWith(StringPiece str, StringPiece prefix);
// Return true iff str ends with suffix, ignoring case.
bool StringCaseEndsWith(StringPiece str, StringPiece suffix);

// Return true if str is equal to the concatenation of first and second. Note
// that this respects case.
bool StringEqualConcat(StringPiece str, StringPiece first, StringPiece second);

// Return the number of mismatched chars in two strings. Useful for string
// comparisons without short-circuiting to prevent timing attacks.
// See http://codahale.com/a-lesson-in-timing-attacks/
int CountCharacterMismatches(StringPiece s1, StringPiece s2);

struct CharStarCompareInsensitive {
  bool operator()(const char *s1, const char *s2) const {
    return (StringCaseCompare(s1, s2) < 0);
  }
};

struct CharStarCompareSensitive {
  bool operator()(const char *s1, const char *s2) const {
    return (strcmp(s1, s2) < 0);
  }
};

struct StringCompareSensitive {
  bool operator()(StringPiece s1, StringPiece s2) const { return s1 < s2; }
};

struct StringCompareInsensitive {
  bool operator()(StringPiece s1, StringPiece s2) const {
    return (StringCaseCompare(s1, s2) < 0);
  }
};

// Parse a list of integers into a vector. Empty values are ignored.
// Returns true if all non-empty values are converted into integers.
bool SplitStringPieceToIntegerVector(StringPiece src, StringPiece separators,
                                     std::vector<int> *ints);

// Does a path end in slash?
inline bool EndsInSlash(StringPiece path) {
  return strings::EndsWith(path, "/");
}

// Make sure directory's path ends in '/'.
inline void EnsureEndsInSlash(GoogleString *dir) {
  if (!EndsInSlash(*dir)) {
    dir->append("/");
  }
}

// Given a string such as:  a b "c d" e 'f g'
// Parse it into a vector:  ["a", "b", "c d", "e", "f g"]
// NOTE: actually used for html doctype recognition,
// so assumes HtmlSpace separation.
void ParseShellLikeString(StringPiece input, std::vector<GoogleString> *output);

// Counts the number of times that substring appears in text
// Note: for a substring that can overlap itself, it counts not necessarily
// disjoint occurrences of the substring.
// For example: "aaa" appears in "aaaaa" 3 times, not once
int CountSubstring(StringPiece text, StringPiece substring);

// Appends new empty string to a StringVector and returns a pointer to it.
inline GoogleString *StringVectorAdd(StringVector *v) {
  v->push_back(GoogleString());
  return &v->back();
}

// Append string-like objects accessed through an iterator.
template <typename I>
void AppendJoinIterator(GoogleString *dest, I start, I end, StringPiece sep) {
  if (start == end) {
    // Skip a lot of set-up and tear-down in empty case.
    return;
  }
  size_t size = dest->size();
  size_t sep_size = 0; // No separator before initial element
  for (I str = start; str != end; ++str) {
    size += str->size() + sep_size;
    sep_size = sep.size();
  }
  dest->reserve(size);
  StringPiece to_prepend("");
  for (I str = start; str != end; ++str) {
    StrAppend(dest, to_prepend, *str);
    to_prepend = sep;
  }
}

// Append an arbitrary iterable collection of strings such as a StringSet,
// StringVector, or StringPieceVector, separated by a given separator, with
// given initial and final strings.  Argument order chosen to be consistent
// with StrAppend.
template <typename C>
void AppendJoinCollection(GoogleString *dest, const C &collection,
                          StringPiece sep) {
  AppendJoinIterator(dest, collection.begin(), collection.end(), sep);
}

template <typename C>
GoogleString JoinCollection(const C &collection, StringPiece sep) {
  GoogleString result;
  AppendJoinCollection(&result, collection, sep);
  return result;
}

// Converts a boolean to string.
inline const char *BoolToString(bool b) { return (b ? "true" : "false"); }

// Using isascii with signed chars is unfortunately undefined.
inline bool IsAscii(char c) { return isascii(static_cast<unsigned char>(c)); }

// Tests if c is a standard (non-control) ASCII char 0x20-0x7E.
// Note: This does not include TAB (0x09), LF (0x0A) or CR (0x0D).
inline bool IsNonControlAscii(char c) { return ('\x20' <= c) && (c <= '\x7E'); }

} // namespace net_instaweb

#endif // PAGESPEED_KERNEL_BASE_STRING_UTIL_H_
