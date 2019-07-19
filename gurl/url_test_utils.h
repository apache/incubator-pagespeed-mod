// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_TEST_UTILS_H_
#define URL_URL_TEST_UTILS_H_

// Convenience functions for string conversions.
// These are mostly intended for use in unit tests.

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_canon_internal.h"

namespace url {

namespace test_utils {

// Converts a UTF-16 string from native wchar_t format to char16 by
// truncating the high 32 bits. This is different than the conversion function
// in base bacause it passes invalid UTF-16 characters which is important for
// test purposes. As a result, this is not meant to handle true UTF-32 encoded
// strings.
inline base::string16 TruncateWStringToUTF16(const wchar_t* src) {
  base::string16 str;
  int length = static_cast<int>(wcslen(src));
  for (int i = 0; i < length; ++i) {
    str.push_back(static_cast<base::char16>(src[i]));
  }
  return str;
}

}  // namespace test_utils

}  // namespace url

#endif  // URL_URL_TEST_UTILS_H_
