#pragma once

#include "gmock/gmock.h"
#include "pagespeed/kernel/base/gtest.h"

namespace PageSpeed {

class TestRunner {
 public:
  static int RunTests(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
  }
};

}  // namespace PageSpeed