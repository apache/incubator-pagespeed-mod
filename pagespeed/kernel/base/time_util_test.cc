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


#include "pagespeed/kernel/base/time_util.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"

namespace {

const char kApr5[] = "Mon, 05 Apr 2010 18:49:46 GMT";

// The time-conversion functions are only accurate to the second,
// and we will not be able to test for identity transforms if we
// are not using a multiple of 1000.
const int64 kTimestampMs = 718981 * 1000;

}  // namespace

namespace net_instaweb {

class TimeUtilTest : public testing::Test {
 protected:
  GoogleString GetTimeString(int64 time_ms) {
    GoogleString out;
    EXPECT_TRUE(ConvertTimeToString(time_ms, &out));
    return out;
  }

  int64 GetTimeValue(const GoogleString& time_str) {
    int64 val;
    CHECK(ConvertStringToTime(time_str, &val)) << "Time conversion failed";
    return val;
  }

  GoogleString out_;
};

TEST_F(TimeUtilTest, Test1970) {
  EXPECT_EQ("Thu, 01 Jan 1970 00:00:00 GMT", GetTimeString(0));
  EXPECT_EQ(1270493386000LL, GetTimeValue(kApr5));
}

TEST_F(TimeUtilTest, TestIdentity) {
  EXPECT_EQ(kTimestampMs, GetTimeValue(GetTimeString(kTimestampMs)));
  EXPECT_EQ(kApr5, GetTimeString(GetTimeValue(kApr5)));
}

}  // namespace net_instaweb
