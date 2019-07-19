// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ScopedHistogramTimer, TwoTimersOneScope) {
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer0");
  SCOPED_UMA_HISTOGRAM_TIMER("TestTimer1");
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("TestLongTimer0");
  SCOPED_UMA_HISTOGRAM_LONG_TIMER("TestLongTimer1");
}

// Compile tests for UMA_HISTOGRAM_ENUMERATION with the three different types it
// accepts:
// - integral types
// - unscoped enums
// - scoped enums
TEST(HistogramMacro, IntegralPsuedoEnumeration) {
  UMA_HISTOGRAM_ENUMERATION("Test.FauxEnumeration", 1, 1000);
}

TEST(HistogramMacro, UnscopedEnumeration) {
  enum TestEnum : char {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    MAX_ENTRIES,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.UnscopedEnumeration", SECOND_VALUE,
                            MAX_ENTRIES);
}

TEST(HistogramMacro, ScopedEnumeration) {
  enum class TestEnum {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    kMaxValue = THIRD_VALUE,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration", TestEnum::FIRST_VALUE);

  enum class TestEnum2 {
    FIRST_VALUE,
    SECOND_VALUE,
    THIRD_VALUE,
    MAX_ENTRIES,
  };
  UMA_HISTOGRAM_ENUMERATION("Test.ScopedEnumeration2", TestEnum2::SECOND_VALUE,
                            TestEnum2::MAX_ENTRIES);
}

}  // namespace base
