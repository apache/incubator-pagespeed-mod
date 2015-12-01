/*
 * Copyright 2015 Google Inc.
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
#include "pagespeed/kernel/base/gtest.h"
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
  virtual bool HandleFlush(MessageHandler* handler) {
    return true;
  }
  virtual void HandleDone(bool success) {
    success_ = success;
    done_ = true;
  }

  virtual bool IsBackgroundFetch() const {
    return is_background_fetch_;
  }

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

    redirect_following_fetcher_.reset(new RedirectFollowingUrlAsyncFetcher(
        counting_fetcher_.get(), thread_system_.get(), &stats_, max_redirects_));

    // single redirect
    SimpleResponse singleredirect[] = {
      {"http://singleredirect.com/", HttpStatus::kMovedPermanently, true, "http://singleredirect.com/foo", "" },
      {"http://singleredirect.com/foo", HttpStatus::kOK, false, "", "singleredirect" },
    };
    SETUPRESPONSECHAIN(singleredirect);

    // direct cycle
    SimpleResponse directcycle[] = {
      {"http://directcycle.com/", HttpStatus::kMovedPermanently, true, "http://directcycle.com/", "" }
    };
    SETUPRESPONSECHAIN(directcycle);

    // lots of redirects ending in a cycle
    SimpleResponse longcycle[] = {
      {"http://longcycle.com/foo", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo2", "" },
      {"http://longcycle.com/foo2", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo3", "" },
      {"http://longcycle.com/foo3", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo4", "" },
      {"http://longcycle.com/foo4", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo5", "" },
      {"http://longcycle.com/foo5", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo6", "" },
      {"http://longcycle.com/foo6", HttpStatus::kMovedPermanently, true, "http://longcycle.com/foo2", "" }
    };
    SETUPRESPONSECHAIN(longcycle);

    SimpleResponse toomany[] = {
      {"http://toomany.com/foo1", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo2", "" },
      {"http://toomany.com/foo2", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo3", "" },
      {"http://toomany.com/foo3", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo4", "" },
      {"http://toomany.com/foo4", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo5", "" },
      {"http://toomany.com/foo5", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo6", "" },
      {"http://toomany.com/foo6", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo7", "" },
      {"http://toomany.com/foo7", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo8", "" },
      {"http://toomany.com/foo8", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo9", "" },
      {"http://toomany.com/foo9", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo10", "" },
      {"http://toomany.com/foo10", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo11", "" },
      {"http://toomany.com/foo11", HttpStatus::kMovedPermanently, true, "http://toomany.com/foo12", "" },
      {"http://toomany.com/foo12", HttpStatus::kOK, false, "", "response!" }
    };
    SETUPRESPONSECHAIN(toomany);

    // lots of redirects, but less then kMaxRedirects
    SimpleResponse longchain[] = {
      {"http://longchain.com/foo", HttpStatus::kMovedPermanently, true, "http://longchain.com/foo2", "" },
      {"http://longchain.com/foo2", HttpStatus::kMovedPermanently, true, "http://longchain.com/foo3", "" },
      {"http://longchain.com/foo3", HttpStatus::kMovedPermanently, true, "http://longchain.com/foo4", "" },
      {"http://longchain.com/foo4", HttpStatus::kMovedPermanently, true, "http://longchain.com/foo5", "" },
      {"http://longchain.com/foo5", HttpStatus::kMovedPermanently, true, "http://longchain.com/foo6", "" },
      {"http://longchain.com/foo6", HttpStatus::kOK, false, "", "response!" }
    };
    SETUPRESPONSECHAIN(longchain);

    SimpleResponse missinglocation[] = {
      {"http://missinglocation.com", HttpStatus::kMovedPermanently, false, "", "" }
    };
    SETUPRESPONSECHAIN(missinglocation);

    SimpleResponse emptylocation[] = {
      {"http://emptylocation.com", HttpStatus::kMovedPermanently, true, "", "" }
    };
    SETUPRESPONSECHAIN(emptylocation);
    
    SimpleResponse badlocation[] = {
      {"http://badlocation.com", HttpStatus::kMovedPermanently, true, "asdf\nasdf", "" }
    };
    SETUPRESPONSECHAIN(badlocation);

    SimpleResponse multilocation[] = {
      {"http://multilocation.com", HttpStatus::kMovedPermanently, true, "http://multilocation.com/loc1", "" }
    };
    SETUPRESPONSECHAIN(multilocation);
    mock_fetcher_.AddToResponse("http://multilocation.com", "Location", "http://multilocation.com/loc2");          

    SimpleResponse relativeredirect[] = {
      {"http://relativeredirect.com", HttpStatus::kMovedPermanently, true, "relative", "" },
      {"http://relativeredirect.com/relative", HttpStatus::kMovedPermanently, true, "/relative/", "" },
      {"http://relativeredirect.com/relative/", HttpStatus::kOK, false, "", "relative response" }
    };
    SETUPRESPONSECHAIN(relativeredirect);
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

  MockUrlFetcher mock_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<RedirectFollowingUrlAsyncFetcher> redirect_following_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher_;
  scoped_ptr<CountingUrlAsyncFetcher> counting_fetcher_;

  MockTimer timer_;

  NullMessageHandler handler_;

  int max_redirects_;
  const GoogleString url_missing_location_;
  const GoogleString url_bad_location_;  
  const int ttl_ms_;
};

TEST_F(RedirectFollowingUrlAsyncFetcherTest, SingleRedirect) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://singleredirect.com/", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(2, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("singleredirect", fetch.content());
}

  // TODO(oschaaf): more tests.
  // test relative locations
  // test we respect domain lawyer
  // test we don't follow non-cacheable redirects
  // test shutdown?

  // NOTE(oschaaf): we might want to just store the aggregated cacheability
  // and content of the original request to avoid adding lots of extra cache
  // entries/lookups
  // TODO(oschaaf): validate the assumption that it's OK to treat 301/302
  // responses the same as 200 responses w/respect to their cacheability,
  // like pagespeed resources.
  // TODO(oschaaf): figure out how we should behave w/regard to PERMANENT redirects
  // and cacheability, especially if confliciting information if advertised via the
  // repsonse headers. My guess is that permanent means permanent, but let's check
  // the RFC's on that.

  // TODO(oschaaf): upgrade the cache to store 301/302

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RedirectChainWorks) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://longchain.com/foo", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(6, counting_fetcher_->fetch_count());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ("response!", fetch.content());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, DirectCycleFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://directcycle.com/", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, LongerCycleFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://longcycle.com/foo", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(6, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, TooManyFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://toomany.com/foo1", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(kMaxRedirects + 1, counting_fetcher_->fetch_count());
}


TEST_F(RedirectFollowingUrlAsyncFetcherTest, NoLocationHeaderFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://missinglocation.com", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

// http://greenbytes.de/tech/tc/httpredirects/
// TODO(oschaaf): make sure we want to fail on empty location headers.
// I think we do, but double check it.
TEST_F(RedirectFollowingUrlAsyncFetcherTest, EmptyLocationHeaderFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://emptylocation.com", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

// TODO(oschaaf): check the required paranoia level. Avoid stuff like
// http://telussecuritylabs.com/threats/show/TSL20111005-01
TEST_F(RedirectFollowingUrlAsyncFetcherTest, BadLocationHeaderFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://ugh.com\nLocation:inject\foo", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, LocationSanitization) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://badlocation.com", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}


// TODO(oschaaf): check spec to see if this is desireable.
// Also, mind http://telussecuritylabs.com/threats/show/TSL20111005-01
// Some browers will pick the first one, picking the last one should behave
// avoided to make newline injection ineffective.
// TODO(oschaaf): for 300/multiple choice responses, this should be allowed, 
// regardless of how we handle 301/302
TEST_F(RedirectFollowingUrlAsyncFetcherTest, MultiLocationHeaderFails) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://multilocation.com", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_FALSE(fetch.success());
  EXPECT_EQ(1, counting_fetcher_->fetch_count());
}

TEST_F(RedirectFollowingUrlAsyncFetcherTest, RelativeRedirectSucceeds) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  redirect_following_fetcher_->Fetch("http://relativeredirect.com", &handler_, &fetch);

  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(3, counting_fetcher_->fetch_count());
  EXPECT_STREQ("relative response", fetch.content());  
}

}  // namespace

}  // namespace net_instaweb
