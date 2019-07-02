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
// Unit tests for LoopbackRouteFetcher
//
#include "pagespeed/system/loopback_route_fetcher.h"

#include <cstdlib>

#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/reflecting_test_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

#include "apr_network_io.h"
#include "apr_pools.h"


namespace net_instaweb {

namespace {

const char kOwnIp[] = "198.51.100.1";

class LoopbackRouteFetcherTest : public RewriteOptionsTestBase<RewriteOptions> {
 public:
  LoopbackRouteFetcherTest()
      : pool_(NULL),
        thread_system_(Platform::CreateThreadSystem()),
        options_(thread_system_.get()),
        loopback_route_fetcher_(&options_, kOwnIp, 42, &reflecting_fetcher_) {
  }

  static void SetUpTestCase() {
    apr_initialize();
    atexit(apr_terminate);
  }

  virtual void SetUp() {
    apr_pool_create(&pool_, NULL);
  }

  virtual void TearDown() {
    apr_pool_destroy(pool_);
  }

  void PrepareDone(bool ok) {
    EXPECT_TRUE(ok);
  }

 protected:
  char* DumpAddr(apr_sockaddr_t* addr) {
    char* dbg = NULL;
    apr_sockaddr_ip_get(&dbg, addr);
    return dbg;  // it's in pool_
  }

  apr_pool_t* pool_;
  GoogleMessageHandler handler_;
  ReflectingTestFetcher reflecting_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  RewriteOptions options_;
  LoopbackRouteFetcher loopback_route_fetcher_;
};

TEST_F(LoopbackRouteFetcherTest, LoopbackRouteFetcherWorks) {
  // As we use the reflecting fetcher as the backend here, the reply
  // messages will contain the URL the fetcher got as payload. Further,
  // the reflecting fetcher will copy all the request headers it got into its
  // response's header, so we can use the result's response_headers to check
  // the request headers we sent.

  ExpectStringAsyncFetch dest(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  loopback_route_fetcher_.Fetch("http://somehost.com/url", &handler_, &dest);
  EXPECT_STREQ(StrCat("http://", kOwnIp, ":42/url"), dest.buffer());
  EXPECT_STREQ("somehost.com",
               dest.response_headers()->Lookup1("Host"));

  // And also test handling of protocol-relative urls.
  ExpectStringAsyncFetch destPR(
      true, RequestContext::NewTestRequestContext(
          thread_system_.get()));
  loopback_route_fetcher_.Fetch("http://somehost.com//foo/bar",
                                &handler_, &destPR);
  EXPECT_STREQ(StrCat("http://", kOwnIp, ":42//foo/bar"),
               destPR.buffer());
  EXPECT_STREQ("somehost.com",
               destPR.response_headers()->Lookup1("Host"));

  // Now make somehost.com known, as well as somehost.cdn.com
  options_.WriteableDomainLawyer()->AddOriginDomainMapping(
      "somehost.cdn.com", "somehost.com", "", &handler_);

  ExpectStringAsyncFetch dest2(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  loopback_route_fetcher_.Fetch("http://somehost.com/url", &handler_, &dest2);
  EXPECT_STREQ("http://somehost.com/url", dest2.buffer());

  ExpectStringAsyncFetch dest3(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com/url",
                                &handler_, &dest3);
  EXPECT_STREQ("http://somehost.cdn.com/url", dest3.buffer());

  // Should still be redirected if the port doesn't match.
  ExpectStringAsyncFetch dest4(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com:123/url",
                                &handler_, &dest4);
  EXPECT_STREQ(StrCat("http://", kOwnIp, ":42/url"), dest4.buffer());
  EXPECT_STREQ("somehost.cdn.com:123",
               dest4.response_headers()->Lookup1("Host"));

  // Now add a session authorization for the CDN's origin. It should now
  // connect directly.
  RequestContextPtr request_context5(
      RequestContext::NewTestRequestContext(thread_system_.get()));
  request_context5->AddSessionAuthorizedFetchOrigin(
      "http://somehost.cdn.com:123");

  ExpectStringAsyncFetch dest5(true, request_context5);
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com:123/url",
                                &handler_, &dest5);
  EXPECT_STREQ("http://somehost.cdn.com:123/url", dest5.buffer());

  // The same authorization doesn't permit a different port, however.
  RequestContextPtr request_context6(
      RequestContext::NewTestRequestContext(thread_system_.get()));
  request_context5->AddSessionAuthorizedFetchOrigin(
      "http://somehost.cdn.com:123");

  ExpectStringAsyncFetch dest6(true, request_context6);
  loopback_route_fetcher_.Fetch("http://somehost.cdn.com:456/url",
                                &handler_, &dest6);
  EXPECT_STREQ(StrCat("http://", kOwnIp, ":42/url"), dest6.buffer());
  EXPECT_STREQ("somehost.cdn.com:456",
               dest6.response_headers()->Lookup1("Host"));
}

TEST_F(LoopbackRouteFetcherTest, CanDetectSelfSrc) {
  apr_sockaddr_t* loopback_1 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_1, "127.0.0.1", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_2 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_2, "127.12.34.45", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_3 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_3, "::1", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* loopback_4 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&loopback_4, "::FFFF:127.0.0.2", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_1 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_1, "128.0.0.1", APR_INET,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_2 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_2, "::1:1", APR_INET6,
                                  80, 0, pool_));

  apr_sockaddr_t* not_loopback_3 = NULL;
  ASSERT_EQ(APR_SUCCESS,
            apr_sockaddr_info_get(&not_loopback_3, "::1:FFFF:127.0.0.1",
                                  APR_INET6, 80, 0, pool_));

  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_1))
      << DumpAddr(loopback_1);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_2))
      << DumpAddr(loopback_2);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_3))
      << DumpAddr(loopback_3);
  EXPECT_TRUE(LoopbackRouteFetcher::IsLoopbackAddr(loopback_4))
      << DumpAddr(loopback_4);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_1))
      << DumpAddr(not_loopback_1);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_2))
      << DumpAddr(not_loopback_2);
  EXPECT_FALSE(LoopbackRouteFetcher::IsLoopbackAddr(not_loopback_3))
      << DumpAddr(not_loopback_3);
}

TEST_F(LoopbackRouteFetcherTest, ProxySuffix) {
  RewriteOptionsManager options_manager;
  DomainLawyer* lawyer = options_.WriteableDomainLawyer();
  lawyer->set_proxy_suffix(".suffix");

  ExpectStringAsyncFetch dest(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));

  GoogleString url("http://www.foo.com.suffix");
  RequestHeaders request_headers;
  options_manager.PrepareRequest(
      &options_, dest.request_context(), &url, &request_headers,
      NewCallback(this, &LoopbackRouteFetcherTest::PrepareDone));
  loopback_route_fetcher_.Fetch("http://www.foo.com", &handler_, &dest);
  EXPECT_STREQ("http://www.foo.com", dest.buffer());
  EXPECT_EQ(NULL, dest.response_headers()->Lookup1("Host"));
}

}  // namespace

}  // namespace net_instaweb
