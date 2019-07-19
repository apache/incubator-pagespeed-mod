// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ByRef;
using testing::MockFunction;

namespace base {
namespace test {

typedef base::Callback<void(const bool& src, bool* dst)> TestCallback;

void SetBool(const bool& src, bool* dst) {
  *dst = src;
}

TEST(GmockCallbackSupportTest, IsNullCallback) {
  MockFunction<void(const TestCallback&)> check;
  EXPECT_CALL(check, Call(IsNullCallback()));
  check.Call(TestCallback());
}

TEST(GmockCallbackSupportTest, IsNotNullCallback) {
  MockFunction<void(const TestCallback&)> check;
  EXPECT_CALL(check, Call(IsNotNullCallback()));
  check.Call(base::Bind(&SetBool));
}

TEST(GmockCallbackSupportTest, RunClosure) {
  MockFunction<void(const base::Closure&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback())).WillOnce(RunClosure<0>());
  check.Call(base::Bind(&SetBool, true, &dst));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallback0) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(true, &dst));
  check.Call(base::Bind(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallback1) {
  MockFunction<void(int, const TestCallback&)> check;
  bool dst = false;
  EXPECT_CALL(check, Call(0, IsNotNullCallback()))
      .WillOnce(RunCallback<1>(true, &dst));
  check.Call(0, base::Bind(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallbackPassByRef) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  bool src = false;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(ByRef(src), &dst));
  src = true;
  check.Call(base::Bind(&SetBool));
  EXPECT_TRUE(dst);
}

TEST(GmockCallbackSupportTest, RunCallbackPassByValue) {
  MockFunction<void(const TestCallback&)> check;
  bool dst = false;
  bool src = true;
  EXPECT_CALL(check, Call(IsNotNullCallback()))
      .WillOnce(RunCallback<0>(src, &dst));
  src = false;
  check.Call(base::Bind(&SetBool));
  EXPECT_TRUE(dst);
}

}  // namespace test
}  // namespace base
