// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StrCat, 8Bit) {
  EXPECT_EQ("", StrCat({""}));
  EXPECT_EQ("1", StrCat({"1"}));
  EXPECT_EQ("122", StrCat({"1", "22"}));
  EXPECT_EQ("122333", StrCat({"1", "22", "333"}));
  EXPECT_EQ("1223334444", StrCat({"1", "22", "333", "4444"}));
  EXPECT_EQ("122333444455555", StrCat({"1", "22", "333", "4444", "55555"}));
}

TEST(StrCat, 16Bit) {
  string16 arg1 = ASCIIToUTF16("1");
  string16 arg2 = ASCIIToUTF16("22");
  string16 arg3 = ASCIIToUTF16("333");

  EXPECT_EQ(ASCIIToUTF16(""), StrCat({string16()}));
  EXPECT_EQ(ASCIIToUTF16("1"), StrCat({arg1}));
  EXPECT_EQ(ASCIIToUTF16("122"), StrCat({arg1, arg2}));
  EXPECT_EQ(ASCIIToUTF16("122333"), StrCat({arg1, arg2, arg3}));
}

TEST(StrAppend, 8Bit) {
  std::string result;

  result = "foo";
  StrAppend(&result, {std::string()});
  EXPECT_EQ("foo", result);

  result = "foo";
  StrAppend(&result, {"1"});
  EXPECT_EQ("foo1", result);

  result = "foo";
  StrAppend(&result, {"1", "22", "333"});
  EXPECT_EQ("foo122333", result);
}

TEST(StrAppend, 16Bit) {
  string16 arg1 = ASCIIToUTF16("1");
  string16 arg2 = ASCIIToUTF16("22");
  string16 arg3 = ASCIIToUTF16("333");

  string16 result;

  result = ASCIIToUTF16("foo");
  StrAppend(&result, {string16()});
  EXPECT_EQ(ASCIIToUTF16("foo"), result);

  result = ASCIIToUTF16("foo");
  StrAppend(&result, {arg1});
  EXPECT_EQ(ASCIIToUTF16("foo1"), result);

  result = ASCIIToUTF16("foo");
  StrAppend(&result, {arg1, arg2, arg3});
  EXPECT_EQ(ASCIIToUTF16("foo122333"), result);
}

}  // namespace base
