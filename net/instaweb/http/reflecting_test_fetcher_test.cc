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

//
// Unit tests for ReflectingTestFetcher
//

#include "net/instaweb/http/public/reflecting_test_fetcher.h"

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

class ReflectingFetcherTest : public ::testing::Test {
 protected:
  GoogleMessageHandler handler_;
  ReflectingTestFetcher reflecting_fetcher_;
};

TEST_F(ReflectingFetcherTest, ReflectingFetcherWorks) {
  scoped_ptr<ThreadSystem> ts(Platform::CreateThreadSystem());
  ExpectStringAsyncFetch dest(
      true, RequestContext::NewTestRequestContext(ts.get()));
  dest.request_headers()->Add("A", "First letter");
  dest.request_headers()->Add("B", "B#1");
  dest.request_headers()->Add("B", "B#2");
  reflecting_fetcher_.Fetch("url", &handler_, &dest);
  EXPECT_STREQ("url", dest.buffer());
  EXPECT_STREQ("First letter", dest.response_headers()->Lookup1("A"));
  ConstStringStarVector values;
  EXPECT_TRUE(dest.response_headers()->Lookup("B", &values));
  ASSERT_EQ(2, values.size());
  EXPECT_STREQ("B#1", *values[0]);
  EXPECT_STREQ("B#2", *values[1]);
}


}  // namespace

}  // namespace net_instaweb
