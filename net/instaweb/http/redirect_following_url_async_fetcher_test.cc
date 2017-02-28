/*
 * Copyright 2016 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: oschaaf@we-amp.com (Otto van der Schaaf)

#include "net/instaweb/http/public/redirect_following_url_async_fetcher.h"

#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"

namespace net_instaweb {

namespace {

const int kMaxRedirects = 10;

struct SimpleResponse {
  GoogleString url;
  HttpStatus::Code status_code;
  bool set_location;
  GoogleString location;
  GoogleString body;
};

#define SETUPRESPONSECHAIN(x) SetupResponseChain(x, sizeof(x) / sizeof(x[0]))

class MockFetch : public AsyncFetch {
 public:
  explicit MockFetch(const RequestContextPtr& ctx, bool is_background_fetch)
      : AsyncFetch(ctx),
        is_background_fetch_(is_background_fetch),
        done_(false),
        success_(false) {}
  virtual ~MockFetch() {}
  virtual void HandleHeadersComplete() {}
  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    content.AppendToString(&content_);
    return true;
  }
  virtual bool HandleFlush(MessageHandler* handler) { return true; }
  virtual void HandleDone(bool success) {
    success_ = success;
    done_ = true;
  }

  virtual bool IsBackgroundFetch() const { return is_background_fetch_; }

  const GoogleString& content() { return content_; }
  bool done() { return done_; }
  bool success() { return success_; }

 private:
  GoogleString content_;
  bool is_background_fetch_;
  bool done_;
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(MockFetch);
};

class RedirectFollowingUrlAsyncFetcherTest : public ::testing::Test {
 protected:
  RedirectFollowingUrlAsyncFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        max_redirects_(kMaxRedirects),
        ttl_ms_(Timer::kHourMs) {
    counting_fetcher_.reset(new CountingUrlAsyncFetcher(&mock_fetcher_));
    rewrite_options_.reset(new RewriteOptions(thread_system_.get()));
    domain_lawyer_ = rewrite_options_->WriteableDomainLawyer();
    NullMessageHandler handler;
    rewrite_options_manager_.reset(new RewriteOptionsManager());
    redirect_following_fetcher_.reset(new RedirectFollowingUrlAsyncFetcher(
        counting_fetcher_.get(), "http://context.url/", thread_system_.get(),
        &stats_, max_redirects_, false /* follow temporary redirects */,
        rewrite_options_.get(), rewrite_options_manager_.get()));

    // single redirect
    SimpleResponse singleredirect[] = {
        {"http://singleredirect.com/", HttpStatus::kMovedPermanently, true,
         "http://singleredirect.com/foo", ""},
        {"http://singleredirect.com/foo", HttpStatus::kOK, false, "",
         "singleredirect"},
    };
    SETUPRESPONSECHAIN(singleredirect);
    domain_lawyer_->AddDomain("http://singleredirect.com/", &handler);

    // single redirect on the context domain, should not require explicit
    // authorization
    SimpleResponse singleredirect_in_context[] = {
        {"http://context.url/foo", HttpStatus::kMovedPermanently, true,
         "http://context.url/bar", ""},
        {"http://context.url/bar", HttpStatus::kOK, false, "",
         "SingleRedirectInContextWithoutExplicitAuth"},
        {"http://context.url/todisallowed", HttpStatus::kMovedPermanently, true,
         "http://context.url/disallowed", ""},
        {"http://context.url/disallowed", HttpStatus::kOK, false, "",
         "disallowed body"},
    };
    SETUPRESPONSECHAIN(singleredirect_in_context);
    rewrite_options_->Disallow("http://context.url/disallow*");

    // single redirect on the context domain, should not require explicit
    // authorization
    SimpleResponse singleredirect_unauthorized[] = {
        {"http://context.url/tounauth", HttpStatus::kMovedPermanently, true,
         "http://unauthorized.url/bar", ""},
        {"http://unauthorized.url/bar", HttpStatus::kOK, false, "",
         "Should not have fetched this!!"},
    };
    SETUPRESPONSECHAIN(singleredirect_unauthorized);

    // direct cycle
    SimpleResponse directcycle[] = {{"http://directcycle.com/",
                                     HttpStatus::kMovedPermanently, true,
                                     "http://directcycle.com/", ""}};
    SETUPRESPONSECHAIN(directcycle);
    domain_lawyer_->AddDomain("http://directcycle.com/", &handler);

    // lots of redirects ending in a cycle
    SimpleResponse longcycle[] = {
        {"http://longcycle.com/foo", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo2", ""},
        {"http://longcycle.com/foo2", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo3", ""},
        {"http://longcycle.com/foo3", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo4", ""},
        {"http://longcycle.com/foo4", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo5", ""},
        {"http://longcycle.com/foo5", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo6", ""},
        {"http://longcycle.com/foo6", HttpStatus::kMovedPermanently, true,
         "http://longcycle.com/foo2", ""}};
    SETUPRESPONSECHAIN(longcycle);
    domain_lawyer_->AddDomain("http://longcycle.com/", &handler);

    SimpleResponse toomany[] = {
        {"http://toomany.com/foo1", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo2", ""},
        {"http://toomany.com/foo2", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo3", ""},
        {"http://toomany.com/foo3", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo4", ""},
        {"http://toomany.com/foo4", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo5", ""},
        {"http://toomany.com/foo5", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo6", ""},
        {"http://toomany.com/foo6", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo7", ""},
        {"http://toomany.com/foo7", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo8", ""},
        {"http://toomany.com/foo8", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo9", ""},
        {"http://toomany.com/foo9", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo10", ""},
        {"http://toomany.com/foo10", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo11", ""},
        {"http://toomany.com/foo11", HttpStatus::kMovedPermanently, true,
         "http://toomany.com/foo12", ""},
        {"http://toomany.com/foo12", HttpStatus::kOK, false, "", "response!"}};
    SETUPRESPONSECHAIN(toomany);
    domain_lawyer_->AddDomain("http://toomany.com/", &handler);

    // lots of redirects, but less then kMaxRedirects
    SimpleResponse longchain[] = {
        {"http://longchain.com/foo", HttpStatus::kMovedPermanently, true,
         "http://longchain.com/foo2", ""},
        {"http://longchain.com/foo2", HttpStatus::kMovedPermanently, true,
         "http://longchain.com/foo3", ""},
        {"http://longchain.com/foo3", HttpStatus::kMovedPermanently, true,
         "http://longchain.com/foo4", ""},
        {"http://longchain.com/foo4", HttpStatus::kMovedPermanently, true,
         "http://longchain.com/foo5", ""},
        {"http://longchain.com/foo5", HttpStatus::kMovedPermanently, true,
         "http://longchain.com/foo6", ""},
        {"http://longchain.com/foo6", HttpStatus::kOK, false, "", "response!"}};
    SETUPRESPONSECHAIN(longchain);
    domain_lawyer_->AddDomain("http://longchain.com/", &handler);


    SimpleResponse missinglocation[] = {{"http://missinglocation.com",
                                         HttpStatus::kMovedPermanently, false,
                                         "", ""}};
    SETUPRESPONSECHAIN(missinglocation);
    domain_lawyer_->AddDomain("http://missinglocation.com/", &handler);

    SimpleResponse emptylocation[] = {{"http://emptylocation.com",
                                       HttpStatus::kMovedPermanently, true, "",
                                       ""}};
    SETUPRESPONSECHAIN(emptylocation);
    domain_lawyer_->AddDomain("http://emptylocation.com/", &handler);

    SimpleResponse badlocation[] = {
        {"http://urlsanitize.com", HttpStatus::kMovedPermanently, true,
         "asdf\fasdf", ""},
        {"http://urlsanitize.com/asdf%0Casdf", HttpStatus::kOK, true, "",
         "sanitized"}};
    SETUPRESPONSECHAIN(badlocation);
    domain_lawyer_->AddDomain("http://urlsanitize.com/", &handler);

    SimpleResponse multilocation[] = {{"http://multilocation.com",
                                       HttpStatus::kMovedPermanently, true,
                                       "http://multilocation.com/loc1", ""}};
    domain_lawyer_->AddDomain("http://multilocation.com/", &handler);

    SETUPRESPONSECHAIN(multilocation);
    mock_fetcher_.AddToResponse("http://multilocation.com", "Location",
                                "http://multilocation.com/loc2");

    SimpleResponse relativeredirect[] = {
        {"http://relativeredirect.com", HttpStatus::kMovedPermanently, true,
         "relative", ""},
        {"http://relativeredirect.com/relative", HttpStatus::kMovedPermanently,
         true, "/relative/", ""},
        {"http://relativeredirect.com/relative/", HttpStatus::kOK, false, "",
         "relative response"}};
    SETUPRESPONSECHAIN(relativeredirect);
    domain_lawyer_->AddDomain("http://relativeredirect.com/", &handler);

    SimpleResponse dataredirect[] = {{"http://dataredirect.com/",
                                      HttpStatus::kMovedPermanently, true,
                                      "data:text/html,%3Chtml/%3E", ""}};
    SETUPRESPONSECHAIN(dataredirect);
    domain_lawyer_->AddDomain("http://dataredirect.com/", &handler);

    SimpleResponse neworigin[] = {
        {"http://redirectmapped.com/", HttpStatus::kMovedPermanently, true,
         "http://redirectmapped.com/mapped-origin", "OK"},
        {"http://neworigin.com/mapped-origin", HttpStatus::kOK, true, "",
         "mappedredirect"}};
    SETUPRESPONSECHAIN(neworigin);
    domain_lawyer_->AddDomain("http://redirectmapped.com/", &handler);

    SimpleResponse fragmentredirect[] = {{"http://fragmentredirect.com/",
                                          HttpStatus::kMovedPermanently, true,
                                          "/foo#bar", ""}};
    SETUPRESPONSECHAIN(fragmentredirect);
    domain_lawyer_->AddDomain("http://fragmentredirect.com/", &handler);

    SimpleResponse protocolrelativeredirect[] = {
        {"http://protocolrelative.com/", HttpStatus::kMovedPermanently, true,
         "//protocolrelative.com/redir", ""},
        {"http://protocolrelative.com/redir", HttpStatus::kOK, true, "",
         "protocolrelativebody"}};
    SETUPRESPONSECHAIN(protocolrelativeredirect);
    domain_lawyer_->AddDomain("http://protocolrelative.com/", &handler);
  }

  void SetupResponseChain(const SimpleResponse responses[], size_t size) {
    for (size_t i = 0; i < size; i++) {
      SimpleResponse response = responses[i];
      // Set fetcher result and headers.
      ResponseHeaders headers;
      headers.set_major_version(1);
      headers.set_minor_version(1);
      headers.SetStatusAndReason(response.status_code);
      headers.SetDateAndCaching(timer_.NowMs(), ttl_ms_);
      if (response.set_location) {
        headers.Add("Location", response.location);
      }
      mock_fetcher_.SetResponse(response.url, headers, response.body);
    }
  }

  static void SetUpTestCase() { RewriteOptions::Initialize(); }
  static void TearDownTestCase() { RewriteOptions::Terminate(); }

  MockUrlFetcher mock_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<RedirectFollowingUrlAsyncFetcher> redirect_following_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher_;
  scoped_ptr<CountingUrlAsyncFetcher> counting_fetcher_;
  scoped_ptr<RewriteOptions> rewrite_options_;
  scoped_ptr<RewriteOptionsManager> rewrite_options_manager_;
  // not owned.
  DomainLawyer* domain_lawyer_;
  MockTimer timer_;

  NullMessageHandler handler_;

  int max_redirects_;
  const GoogleString url_missing_location_;
  const GoogleString url_bad_location_;
  const int ttl_ms_;
};

TEST_F(RedirectFollowingUrlAsyncFetcherTest, SingleRedirect) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://singleredirect.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("singleredirect", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, SingleRedirectOriginMapped) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  // TODO(oschaaf): test the origin host override separately, seems a no-op here
  // as the mock responses
  // do not seem to care about the host header.
  domain_lawyer_->AddOriginDomainMapping("neworigin.com", "redirectmapped.com",
                                         "originhostoverride", &handler);
  redirect_following_fetcher_->Fetch("http://redirectmapped.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("mappedredirect", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       SingleRedirectInContextWithoutExplicitAuth) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://context.url/foo", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("SingleRedirectInContextWithoutExplicitAuth", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       SingleRedirectInContextToUnauthorized) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://context.url/tounauth", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       SingleRedirectInContextAuthorizedButDisallowed) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://context.url/todisallowed",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectChainWorks) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://longchain.com/foo", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(6, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("response!", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectChainGivesSmallestTTL) {
  HttpOptions http_options(kDefaultHttpOptionsForTests);
  MockFetch fetch(RequestContextPtr(new RequestContext(
      http_options, thread_system_->NewMutex(), NULL)), true);


  // lots of redirects, but less then kMaxRedirects
  SimpleResponse ttlchain[] = {
      {"http://ttlchain.com/foo", HttpStatus::kMovedPermanently, true,
        "http://ttlchain.com/foo2", ""},
      {"http://ttlchain.com/foo2", HttpStatus::kMovedPermanently, true,
        "http://ttlchain.com/foo3", ""},
      {"http://ttlchain.com/foo3", HttpStatus::kOK, false, "", "response!"}};
  NullMessageHandler handler;
  domain_lawyer_->AddDomain("http://ttlchain.com/", &handler);
  size_t size = sizeof(ttlchain) / sizeof(ttlchain[0]);
  int two_seconds_ttl_ms = 1000 * 200;
  for (size_t i = 0; i < size; i++) {
    SimpleResponse response = ttlchain[i];
    // Set fetcher result and headers.
    ResponseHeaders headers;
    headers.set_major_version(1);
    headers.set_minor_version(1);
    headers.SetStatusAndReason(response.status_code);

    // Give the second redirect a small TTL.
    // This is the TTL that we want to see in the final 200 response.
    if (i == 1) {
      headers.SetDateAndCaching(timer_.NowMs(), two_seconds_ttl_ms);
    } else{
      headers.SetDateAndCaching(timer_.NowMs(), ttl_ms_);
    }
    if (response.set_location) {
      headers.Add("Location", response.location);
    }
    headers.SetCacheControlPublic();
    mock_fetcher_.SetResponse(response.url, headers, response.body);
  }

  redirect_following_fetcher_->Fetch("http://ttlchain.com/foo", &handler_,
                                     &fetch);
  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(3, counting_fetcher_->fetch_count());
  EXPECT_EQ(two_seconds_ttl_ms, fetch.response_headers()->cache_ttl_ms());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("response!", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectTempChainGivesSmallestTTL) {
  HttpOptions http_options(kDefaultHttpOptionsForTests);
  MockFetch fetch(RequestContextPtr(new RequestContext(
      http_options, thread_system_->NewMutex(), NULL)), true);


  // lots of redirects, but less then kMaxRedirects
  SimpleResponse ttlchain[] = {
      {"http://ttlchain.com/foo", HttpStatus::kFound, true,
        "http://ttlchain.com/foo2", ""},
      {"http://ttlchain.com/foo2", HttpStatus::kFound, true,
        "http://ttlchain.com/foo3", ""},
      {"http://ttlchain.com/foo3", HttpStatus::kOK, false, "", "response!"}};
  NullMessageHandler handler;
  domain_lawyer_->AddDomain("http://ttlchain.com/", &handler);
  size_t size = sizeof(ttlchain) / sizeof(ttlchain[0]);
  int two_seconds_ttl_ms = 1000 * 200;
  for (size_t i = 0; i < size; i++) {
    SimpleResponse response = ttlchain[i];
    // Set fetcher result and headers.
    ResponseHeaders headers;
    headers.set_major_version(1);
    headers.set_minor_version(1);
    headers.SetStatusAndReason(response.status_code);

    // Give the second redirect a small TTL.
    // This is the TTL that we want to see in the final 200 response.
    if (i == 1) {
      headers.SetDateAndCaching(timer_.NowMs(), two_seconds_ttl_ms);
    } else{
      headers.SetDateAndCaching(timer_.NowMs(), ttl_ms_);
    }
    if (response.set_location) {
      headers.Add("Location", response.location);
    }
    headers.SetCacheControlPublic();
    headers.set_cache_temp_redirects(true);
    mock_fetcher_.SetResponse(response.url, headers, response.body);
  }

  redirect_following_fetcher_->Fetch("http://ttlchain.com/foo", &handler_,
                                     &fetch);
  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(3, counting_fetcher_->fetch_count());
  EXPECT_EQ(two_seconds_ttl_ms, fetch.response_headers()->cache_ttl_ms());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("response!", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, DirectCycleFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://directcycle.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, LongerCycleFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://longcycle.com/foo", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(6, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, TooManyFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://toomany.com/foo1", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(kMaxRedirects + 1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, NoLocationHeaderFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://missinglocation.com", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, EmptyLocationHeaderFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://emptylocation.com", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, DeclineBadUrlInput) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://doesnotexist.com\nfoo:bar1\foo",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(0, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, LocationSanitization) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://urlsanitize.com", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_STREQ("sanitized", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, MultiLocationHeaderFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://multilocation.com", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RelativeRedirectSucceeds) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  redirect_following_fetcher_->Fetch("http://relativeredirect.com", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(3, counting_fetcher_->fetch_count());
  EXPECT_STREQ("relative response", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectDataUriFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://dataredirect.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       RedirectPermanentUncacheableNotFollowed) {
  ResponseHeaders headers;
  headers.set_major_version(1);
  headers.set_minor_version(1);
  headers.SetStatusAndReason(HttpStatus::kMovedPermanently);
  headers.Add("Location", "http://context.url/test");
  headers.Add(HttpAttributes::kCacheControl, "private");
  mock_fetcher_.SetResponse("http://context.url/uncacheable", headers, "redir");

  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://context.url/uncacheable",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       RedirectTemporaryUncacheableNotFollowed) {
  ResponseHeaders headers;
  headers.set_major_version(1);
  headers.set_minor_version(1);
  headers.SetStatusAndReason(HttpStatus::kFound);
  headers.Add("Location", "http://context.url/test");
  headers.Add(HttpAttributes::kCacheControl, "private");
  mock_fetcher_.SetResponse("http://context.url/uncacheable", headers, "redir");

  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://context.url/uncacheable",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       RedirectPermanentUnspecifiedCacheabilityFollowed) {
  ResponseHeaders headers;
  headers.set_major_version(1);
  headers.set_minor_version(1);
  headers.SetStatusAndReason(HttpStatus::kMovedPermanently);
  headers.FixDateHeaders(0);
  headers.Add("Location", "http://context.url/bar");

  mock_fetcher_.SetResponse("http://context.url/unspecified", headers, "redir");

  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://context.url/unspecified",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("SingleRedirectInContextWithoutExplicitAuth", fetch.content());
  // We should get the minimum TTL encountered along the chain, which is the default
  // unspecified TTL in this case (300 seconds)
  EXPECT_EQ(300000, fetch.response_headers()->cache_ttl_ms());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest,
       RedirectTemporaryUnspecifiedCacheabilityNotFollowed) {
  ResponseHeaders headers;
  headers.set_major_version(1);
  headers.set_minor_version(1);
  headers.SetStatusAndReason(HttpStatus::kFound);
  headers.FixDateHeaders(0);
  headers.Add("Location", "http://context.url/bar");

  mock_fetcher_.SetResponse("http://context.url/unspecified", headers, "redir");

  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://context.url/unspecified",
                                     &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

// We do not support redirecting to fragments
TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectToFragmentFails) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://fragmentredirect.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

// Protocol relative redirects work.
TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectToProtocolRelativeWorks) {
  MockFetch fetch(RequestContext::NewTestRequestContext(thread_system_.get()),
                  true);
  NullMessageHandler handler;
  redirect_following_fetcher_->Fetch("http://protocolrelative.com/", &handler_,
                                     &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("protocolrelativebody", fetch.content());
}

}  // namespace

}  // namespace net_instaweb
