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


#include "pagespeed/kernel/util/re2.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

class Re2Test : public testing::Test {
};

TEST_F(Re2Test, FullMatch) {
  EXPECT_FALSE(RE2::FullMatch("helo", "h.*oo"));
  EXPECT_TRUE(RE2::FullMatch("helo", "h.*o"));
}

}  // namespace net_instaweb
