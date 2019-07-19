// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_clear_last_error.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

TEST(ScopedClearLastError, TestNoError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    EXPECT_EQ(0, errno);
  }
  EXPECT_EQ(1, errno);
}

TEST(ScopedClearLastError, TestError) {
  errno = 1;
  {
    ScopedClearLastError clear_error;
    errno = 2;
  }
  EXPECT_EQ(1, errno);
}

#if defined(OS_WIN)

TEST(ScopedClearLastError, TestNoErrorWin) {
  ::SetLastError(1);
  {
    ScopedClearLastError clear_error;
    EXPECT_EQ(logging::SystemErrorCode(0), ::GetLastError());
  }
  EXPECT_EQ(logging::SystemErrorCode(1), ::GetLastError());
}

TEST(ScopedClearLastError, TestErrorWin) {
  ::SetLastError(1);
  {
    ScopedClearLastError clear_error;
    ::SetLastError(2);
  }
  EXPECT_EQ(logging::SystemErrorCode(1), ::GetLastError());
}

#endif  // defined(OS_WIN)

}  // namespace internal
}  // namespace base
