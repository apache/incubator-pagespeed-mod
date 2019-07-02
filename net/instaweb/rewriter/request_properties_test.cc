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


#include "net/instaweb/rewriter/public/request_properties.h"

#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

class RequestPropertiesTest: public testing::Test {
 protected:
  UserAgentMatcher user_agent_matcher_;
};

TEST_F(RequestPropertiesTest, SupportsWebpRewrittenUrls) {
  RequestProperties request_properties(&user_agent_matcher_);
  request_properties.SetUserAgent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  RequestHeaders headers;
  headers.Add(HttpAttributes::kAccept, "image/webp");
  request_properties.ParseRequestHeaders(headers);
  EXPECT_TRUE(request_properties.SupportsWebpRewrittenUrls());
}

TEST_F(RequestPropertiesTest, SupportsImageInliningNoRequestHeaders) {
  RequestProperties request_properties(&user_agent_matcher_);
  request_properties.SetUserAgent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  EXPECT_TRUE(request_properties.SupportsImageInlining());
}

TEST_F(RequestPropertiesTest, SupportsImageInliningEmptyRequestHeaders) {
  RequestProperties request_properties(&user_agent_matcher_);
  request_properties.SetUserAgent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  RequestHeaders request_headers;
  request_headers.Add(kPsaCapabilityList, "");
  request_properties.ParseRequestHeaders(request_headers);
  EXPECT_FALSE(request_properties.SupportsImageInlining());
}

TEST_F(RequestPropertiesTest, SupportsImageInliningViaRequestHeaders) {
  RequestProperties request_properties(&user_agent_matcher_);
  request_properties.SetUserAgent(
      UserAgentMatcherTestBase::kChrome18UserAgent);
  RequestHeaders request_headers;
  request_properties.ParseRequestHeaders(request_headers);
  request_headers.Add(kPsaCapabilityList,
                      RewriteOptions::FilterId(RewriteOptions::kInlineImages));
  EXPECT_TRUE(request_properties.SupportsImageInlining());
}

}  // namespace net_instaweb
