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


#include "pagespeed/kernel/util/url_to_filename_encoder.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

// The escape character choice is made here -- all code and tests in this
// directory are based off of this constant.  However, lots of tests
// have dependencies on this, so it cannot be changed without re-running those
// tests and fixing them.
const char UrlToFilenameEncoder::kEscapeChar = ',';
const char UrlToFilenameEncoder::kTruncationChar = '-';
const size_t UrlToFilenameEncoder::kMaximumSubdirectoryLength = 128;

void UrlToFilenameEncoder::AppendSegment(GoogleString* segment,
                                         GoogleString* dest) {
  CHECK(!segment->empty());
  if ((*segment == ".") || (*segment == "..")) {
    dest->push_back(kEscapeChar);
    dest->append(*segment);
    segment->clear();
  } else {
    size_t segment_size = segment->size();
    if (segment_size > kMaximumSubdirectoryLength) {
      // We need to inject ",-" at the end of the segment to signify that
      // we are inserting an artificial '/'.  This means we have to chop
      // off at least two characters to make room.
      segment_size = kMaximumSubdirectoryLength - 2;

      // But we don't want to break up an escape sequence that happens to lie at
      // the end.  Escape sequences are at most 2 characters.
      if ((*segment)[segment_size - 1] == kEscapeChar) {
        segment_size -= 1;
      } else if ((*segment)[segment_size - 2] == kEscapeChar) {
        segment_size -= 2;
      }
      dest->append(segment->data(), segment_size);
      dest->push_back(kEscapeChar);
      dest->push_back(kTruncationChar);
      segment->erase(0, segment_size);

      // At this point, if we had segment_size=3, and segment="abcd",
      // then after this erase, we will have written "abc,-" and set segment="d"
    } else {
      dest->append(*segment);
      segment->clear();
    }
  }
}

void UrlToFilenameEncoder::EncodeSegment(const StringPiece& filename_prefix,
                                         const StringPiece& escaped_ending,
                                         char dir_separator,
                                         GoogleString* encoded_filename) {
  // We want to unescape URLs so that an %-encodings are cleaned up. However,
  // we do not want to convert "+" to " " in this context, since
  // "+" is fine in a filename, and " " will be escaped here to ",20" below.
  GoogleString filename_ending = GoogleUrl::UnescapeIgnorePlus(escaped_ending);

  char encoded[3];
  int encoded_len;
  GoogleString segment;

  // TODO(jmarantz): This code would be a bit simpler if we disallowed
  // Instaweb allowing filename_prefix to not end in "/".  We could
  // then change the is routine to just take one input string.
  size_t start_of_segment = filename_prefix.find_last_of(dir_separator);
  if (start_of_segment == GoogleString::npos) {
    filename_prefix.CopyToString(&segment);
  } else {
    filename_prefix.substr(start_of_segment + 1).CopyToString(&segment);
    filename_prefix.substr(0, start_of_segment + 1).CopyToString(
        encoded_filename);
  }

  size_t index = 0;
  // Special case the first / to avoid adding a leading kEscapeChar.
  if (!filename_ending.empty() && (filename_ending[0] == dir_separator)) {
    encoded_filename->append(segment);
    segment.clear();
    encoded_filename->push_back(dir_separator);
    ++index;
  }

  for (; index < filename_ending.length(); ++index) {
    unsigned char ch = static_cast<unsigned char>(filename_ending[index]);

    // Note: instead of outputting an empty segment, we let the second slash
    // be escaped below.
    if ((ch == dir_separator) && !segment.empty()) {
      AppendSegment(&segment, encoded_filename);
      encoded_filename->push_back(dir_separator);
      segment.clear();
    } else {
      // After removing unsafe chars the only safe ones are _.=+- and alphanums.
      if ((ch == '_') || (ch == '.') || (ch == '=') || (ch == '+') ||
          (ch == '-') || (('0' <= ch) && (ch <= '9')) ||
          (('A' <= ch) && (ch <= 'Z')) || (('a' <= ch) && (ch <= 'z'))) {
        encoded[0] = ch;
        encoded_len = 1;
      } else {
        encoded[0] = kEscapeChar;
        encoded[1] = ch / 16;
        encoded[1] += (encoded[1] >= 10) ? 'A' - 10 : '0';
        encoded[2] = ch % 16;
        encoded[2] += (encoded[2] >= 10) ? 'A' - 10 : '0';
        encoded_len = 3;
      }
      segment.append(encoded, encoded_len);

      // If segment is too big, we must chop it into chunks.
      if (segment.size() > kMaximumSubdirectoryLength) {
        AppendSegment(&segment, encoded_filename);
        encoded_filename->push_back(dir_separator);
      }
    }
  }

  // Append "," to the leaf filename so the leaf can also be a branch., e.g.
  // allow http://a/b/c and http://a/b/c/d to co-exist as files "/a/b/c," and
  // /a/b/c/d".  So we will rename the "d" here to "d,".  If doing that pushed
  // us over the 128 char limit, then we will need to append "/" and the
  // remaining chars.
  segment += kEscapeChar;
  AppendSegment(&segment, encoded_filename);
  if (!segment.empty()) {
    // The last overflow segment is special, because we appended in
    // kEscapeChar above.  We won't need to check it again for size
    // or further escaping.
    encoded_filename->push_back(dir_separator);
    encoded_filename->append(segment);
  }
}

bool UrlToFilenameEncoder::Decode(const StringPiece& encoded_filename,
                                  GoogleString* decoded_url) {
  const char kDirSeparator = '/';
  enum State {
    kStart,
    kEscape,
    kFirstDigit,
    kTruncate,
    kEscapeDot
  };
  State state = kStart;
  char hex_buffer[3] = { '\0', '\0', '\0' };
  for (int i = 0, n = encoded_filename.size(); i < n; ++i) {
    char ch = encoded_filename[i];
    switch (state) {
      case kStart:
        if (ch == UrlToFilenameEncoder::kEscapeChar) {
          state = kEscape;
        } else if (ch == kDirSeparator) {
          decoded_url->push_back('/');  // URLs only use '/' not '\\'
        } else {
          decoded_url->push_back(ch);
        }
        break;
      case kEscape:
        if (IsHexDigit(ch)) {
          hex_buffer[0] = ch;
          state = kFirstDigit;
        } else if (ch == UrlToFilenameEncoder::kTruncationChar) {
          state = kTruncate;
        } else if (ch == '.') {
          decoded_url->push_back('.');
          state = kEscapeDot;  // Look for at most one more dot.
        } else if (ch == kDirSeparator) {
          // Consider url "//x".  This was once encoded to "/,/x,".
          // This code is what skips the first Escape.
          decoded_url->push_back('/');  // URLs only use '/' not '\\'
          state = kStart;
        } else {
          return false;
        }
        break;
      case kFirstDigit:
        if (IsHexDigit(ch)) {
          hex_buffer[1] = ch;
          uint32 hex_value = 0;
          bool ok = AccumulateHexValue(hex_buffer[0], &hex_value);
          ok = ok && AccumulateHexValue(hex_buffer[1], &hex_value);
          DCHECK(ok) << "Should not have gotten here unless both were hex";
          decoded_url->push_back(static_cast<char>(hex_value));
          state = kStart;
        } else {
          return false;
        }
        break;
      case kTruncate:
        if (ch == kDirSeparator) {
          // Skip this separator, it was only put in to break up long
          // path segments, but is not part of the URL.
          state = kStart;
        } else {
          return false;
        }
        break;
      case kEscapeDot:
        decoded_url->push_back(ch);
        state = kStart;
        break;
    }
  }

  // All legal encoded filenames end in kEscapeChar.
  return (state == kEscape);
}

}  // namespace net_instaweb
