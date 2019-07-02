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
// Unit tests for AddHeadersFetcher.
//
#include "pagespeed/system/add_headers_fetcher.h"

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/reflecting_test_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"  // for ThreadSystem
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

class AddHeadersFetcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 public:
  AddHeadersFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        options_(thread_system_.get()) {
    options_.AddCustomFetchHeader("Custom", "custom-header");
    options_.AddCustomFetchHeader("Extra", "extra-header");
    add_headers_fetcher_.reset(new AddHeadersFetcher(
        &options_, &reflecting_fetcher_));
  }

 protected:
  scoped_ptr<AddHeadersFetcher> add_headers_fetcher_;
  GoogleMessageHandler handler_;
  scoped_ptr<ThreadSystem> thread_system_;
  RewriteOptions options_;
  ReflectingTestFetcher reflecting_fetcher_;
};

TEST_F(AddHeadersFetcherTest, AddsHeaders) {
  ExpectStringAsyncFetch dest(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  add_headers_fetcher_->Fetch("http://example.com/path", &handler_, &dest);
  EXPECT_STREQ("http://example.com/path", dest.buffer());
  EXPECT_STREQ("custom-header", dest.response_headers()->Lookup1("Custom"));
  EXPECT_STREQ("extra-header", dest.response_headers()->Lookup1("Extra"));
}

TEST_F(AddHeadersFetcherTest, ReplacesHeaders) {
  RequestHeaders request_headers;
  ExpectStringAsyncFetch dest(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  request_headers.Add("Custom", "original");
  request_headers.Add("AlsoCustom", "original");
  dest.set_request_headers(&request_headers);
  add_headers_fetcher_->Fetch("http://example.com/path", &handler_, &dest);
  EXPECT_STREQ("http://example.com/path", dest.buffer());

  // Overwritten by the add headers fetcher.
  EXPECT_STREQ("custom-header", dest.response_headers()->Lookup1("Custom"));

  // Passed through unmodified.
  EXPECT_STREQ("original", dest.response_headers()->Lookup1("AlsoCustom"));
}

}  // namespace

}  // namespace net_instaweb

