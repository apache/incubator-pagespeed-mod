// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
void CrashWithBreakDebugger() {
  base::debug::SetSuppressDebugUI(false);
  base::debug::BreakDebugger();

#if defined(OS_WIN)
  // This should not be executed.
  _exit(125);
#endif
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace

// Death tests misbehave on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)

TEST(Debugger, CrashAtBreakpoint) {
  EXPECT_DEATH(CrashWithBreakDebugger(), "");
}

#if defined(OS_WIN)
TEST(Debugger, DoesntExecuteBeyondBreakpoint) {
#if defined(ARCH_CPU_ARM64)
  // brk on aarch64 Windows seems to cause an illegal instruction exception
  EXPECT_EXIT(CrashWithBreakDebugger(),
              ::testing::ExitedWithCode(STATUS_ILLEGAL_INSTRUCTION), "");
#else
  EXPECT_EXIT(CrashWithBreakDebugger(),
              ::testing::ExitedWithCode(STATUS_BREAKPOINT), "");
#endif
}
#endif  // defined(OS_WIN)

#else  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
TEST(Debugger, NoTest) {
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !defined(OS_ANDROID)
