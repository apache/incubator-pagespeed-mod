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


#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"

#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/rate_controller.h"
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

class RateControllingUrlAsyncFetcherTest : public ::testing::Test {
 protected:
  RateControllingUrlAsyncFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        stats_(thread_system_.get()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        domain1_url1_("http://www.d1.com/url1"),
        domain2_url1_("http://www.d2.com/url1"),
        domain3_url1_("http://www.d3.com/url1"),
        body1_("b1"),
        body2_("b2"),
        ttl_ms_(Timer::kHourMs) {
    RateController::InitStats(&stats_);
    wait_fetcher_.reset(new WaitUrlAsyncFetcher(
        &mock_fetcher_, thread_system_->NewMutex()));
    counting_fetcher_.reset(new CountingUrlAsyncFetcher((wait_fetcher_.get())));

    // At max 10 requests will be queued up, and we will have atmost 2 outgoing
    // requests for a particular domain.
    rate_controlling_fetcher_.reset(new RateControllingUrlAsyncFetcher(
        counting_fetcher_.get(), 10, 2, 4, thread_system_.get(), &stats_));

    SetupResponse(domain1_url1_, body1_);
    SetupResponse(domain2_url1_, body2_);
    SetupResponse(domain3_url1_, body3_);
  }

  void SetupResponse(const GoogleString& url, const GoogleString& body) {
    // Set fetcher result and headers.
    ResponseHeaders headers;
    headers.set_major_version(1);
    headers.set_minor_version(1);
    headers.SetStatusAndReason(HttpStatus::kOK);
    headers.SetDateAndCaching(timer_.NowMs(), ttl_ms_);
    mock_fetcher_.SetResponse(url, headers, body);
  }

  int global_fetch_queue_size() {
    return stats_.GetUpDownCounter(
        RateController::kCurrentGlobalFetchQueueSize)->Get();
  }

  MockUrlFetcher mock_fetcher_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats stats_;
  scoped_ptr<RateControllingUrlAsyncFetcher> rate_controlling_fetcher_;
  scoped_ptr<WaitUrlAsyncFetcher> wait_fetcher_;
  scoped_ptr<CountingUrlAsyncFetcher> counting_fetcher_;

  MockTimer timer_;

  NullMessageHandler handler_;

  const GoogleString domain1_url1_;
  const GoogleString domain2_url1_;
  const GoogleString domain3_url1_;
  const GoogleString body1_;
  const GoogleString body2_;
  const GoogleString body3_;
  const int ttl_ms_;
};

TEST_F(RateControllingUrlAsyncFetcherTest, SingleUrlWorks) {
  MockFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), true);
  rate_controlling_fetcher_->Fetch(domain1_url1_, &handler_, &fetch);
  // Call callback immediately.
  wait_fetcher_->CallCallbacks();
  EXPECT_TRUE(fetch.done());
  EXPECT_TRUE(fetch.success());
  EXPECT_EQ(HttpStatus::kOK, fetch.response_headers()->status_code());
  EXPECT_STREQ(body1_, fetch.content());
}

TEST_F(RateControllingUrlAsyncFetcherTest,
       MultipleBackgroundRequestsForSingleHost) {
  std::vector<MockFetch*> fetch_vector;

  // Trigger 100 background requests all for the same domain.
  for (int i = 0; i < 100; ++i) {
    MockFetch* fetch = new MockFetch(
        RequestContext::NewTestRequestContext(thread_system_.get()), true);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain1_url1_, &handler_, fetch);
  }

  // 2 fetches get triggered, while 4 get queued up since the per host threshold
  // is 4. None of these are done yet.
  for (int i = 0; i < 6; ++i) {
    EXPECT_FALSE(fetch_vector[i]->done());
  }
  EXPECT_EQ(4, global_fetch_queue_size());

  // The next 94 fetches get shedded due to load.
  for (int i = 6; i < 100; ++i) {
    EXPECT_TRUE(fetch_vector[i]->done());
    EXPECT_FALSE(fetch_vector[i]->success());
    EXPECT_EQ("", fetch_vector[i]->content());
    EXPECT_TRUE(fetch_vector[i]->response_headers()->Has(
                    HttpAttributes::kXPsaLoadShed));
  }

  // We need 3 calls to WaitUrlAsyncFetcher::CallCallbacks since the queued
  // fetches haven't been triggered yet.
  for (int i = 0; i < 3; ++i) {
    wait_fetcher_->CallCallbacks();
    for (int j = 0; j < 6; ++j) {
      MockFetch* fetch = fetch_vector[j];
      if (j < 2 * (i + 1)) {
        EXPECT_TRUE(fetch->done());
        EXPECT_TRUE(fetch->success());
        EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
        EXPECT_STREQ(body1_, fetch->content());
        EXPECT_FALSE(fetch_vector[i]->response_headers()->Has(
                         HttpAttributes::kXPsaLoadShed));
      } else {
        EXPECT_FALSE(fetch->done());
        EXPECT_FALSE(fetch->success());
      }
    }
  }

  EXPECT_EQ(4, stats_.GetTimedVariable(
      RateController::kQueuedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(94, stats_.GetTimedVariable(
      RateController::kDroppedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(0, global_fetch_queue_size());

  STLDeleteContainerPointers(fetch_vector.begin(), fetch_vector.end());
}

TEST_F(RateControllingUrlAsyncFetcherTest, MultipleRequestsForSingleHost) {
  std::vector<MockFetch*> fetch_vector;

  // Trigger 100 user-facing requests all for the same domain.
  for (int i = 0; i < 100; ++i) {
    MockFetch* fetch = new MockFetch(  // User-facing requests.
        RequestContext::NewTestRequestContext(thread_system_.get()), false);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain1_url1_, &handler_, fetch);
  }

  // Trigger 200 background requests all for the same domain.
  for (int i = 0; i < 200; ++i) {
    MockFetch* fetch = new MockFetch(  // Background requests.
        RequestContext::NewTestRequestContext(thread_system_.get()), true);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain1_url1_, &handler_, fetch);
  }

  // 100 fetches get triggered, while 4 get queued up. The next 196 requests
  // are dropped.
  for (int i = 0; i < 104; ++i) {
    EXPECT_FALSE(fetch_vector[i]->done());
  }
  EXPECT_EQ(4, global_fetch_queue_size());

  for (int i = 104; i < 300; ++i) {
    EXPECT_TRUE(fetch_vector[i]->done());
    EXPECT_FALSE(fetch_vector[i]->success());
    EXPECT_EQ("", fetch_vector[i]->content());
    EXPECT_TRUE(fetch_vector[i]->response_headers()->Has(
                    HttpAttributes::kXPsaLoadShed));
  }

  wait_fetcher_->CallCallbacks();
  // The first 100 fetches complete.
  for (int i = 0; i < 100; ++i) {
    MockFetch* fetch = fetch_vector[i];
    EXPECT_TRUE(fetch->done());
    EXPECT_TRUE(fetch->success());
    EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
    EXPECT_STREQ(body1_, fetch->content());
    EXPECT_FALSE(fetch_vector[i]->response_headers()->Has(
                     HttpAttributes::kXPsaLoadShed));
  }

  // The next 4 are queued up.
  for (int i = 100; i < 104; ++i) {
    EXPECT_FALSE(fetch_vector[i]->done());
  }

  // We need 2 calls to WaitUrlAsyncFetcher::CallCallbacks since the queued
  // fetches haven't been triggered yet.
  for (int i = 0; i < 2; ++i) {
    wait_fetcher_->CallCallbacks();
    for (int j = 100; j < 104; ++j) {
      MockFetch* fetch = fetch_vector[j];
      if (j < 100 + 2 * (i + 1)) {
        EXPECT_TRUE(fetch->done());
        EXPECT_TRUE(fetch->success());
        EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
        EXPECT_STREQ(body1_, fetch->content());
        EXPECT_FALSE(fetch_vector[i]->response_headers()->Has(
                         HttpAttributes::kXPsaLoadShed));
      } else {
        EXPECT_FALSE(fetch->done());
        EXPECT_FALSE(fetch->success());
      }
    }
  }

  EXPECT_EQ(4, stats_.GetTimedVariable(
      RateController::kQueuedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(196, stats_.GetTimedVariable(
      RateController::kDroppedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(0, global_fetch_queue_size());

  STLDeleteContainerPointers(fetch_vector.begin(), fetch_vector.end());
}


TEST_F(RateControllingUrlAsyncFetcherTest,
       MultipleBackgroundRequestsForMultipleHosts) {
  std::vector<MockFetch*> fetch_vector;

  // Trigger a total of 100 requests, alternately for domain1 and domain2.
  // For each domain, 2 fetches get triggered while 4 get queued up.
  for (int i = 0; i < 50; ++i) {
    MockFetch* fetch = new MockFetch(
        RequestContext::NewTestRequestContext(thread_system_.get()), true);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain1_url1_, &handler_, fetch);
    fetch = new MockFetch(
        RequestContext::NewTestRequestContext(thread_system_.get()), true);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain2_url1_, &handler_, fetch);
  }

  // Send another 10 requests for domain3. 2 fetches get triggered, 2 get
  // enqueued and 6 get dropped.
  for (int i = 0; i < 10; ++i) {
    MockFetch* fetch = new MockFetch(
        RequestContext::NewTestRequestContext(thread_system_.get()), true);
    fetch_vector.push_back(fetch);
    rate_controlling_fetcher_->Fetch(domain3_url1_, &handler_, fetch);
  }

  // 6 fetches get triggered, while 10 get queued up. None of these are done
  // yet.
  for (int i = 0; i < 12; ++i) {
    EXPECT_FALSE(fetch_vector[i]->done());
  }
  for (int i = 100; i < 104; ++i) {
    EXPECT_FALSE(fetch_vector[i]->done());
  }
  EXPECT_EQ(10, global_fetch_queue_size());

  // 94 fetches get shedded due to load.
  for (int i = 12; i < 100; ++i) {
    EXPECT_TRUE(fetch_vector[i]->done());
    EXPECT_FALSE(fetch_vector[i]->success());
    EXPECT_EQ("", fetch_vector[i]->content());
    EXPECT_TRUE(fetch_vector[i]->response_headers()->Has(
                    HttpAttributes::kXPsaLoadShed));
  }
  for (int i = 104; i < 110; ++i) {
    EXPECT_TRUE(fetch_vector[i]->done());
    EXPECT_FALSE(fetch_vector[i]->success());
    EXPECT_EQ("", fetch_vector[i]->content());
    EXPECT_TRUE(fetch_vector[i]->response_headers()->Has(
                    HttpAttributes::kXPsaLoadShed));
  }

  // We need 3 calls to WaitUrlAsyncFetcher::CallCallbacks since the queued
  // fetches haven't been triggered yet.
  for (int i = 0; i < 3; ++i) {
    wait_fetcher_->CallCallbacks();
    for (int j = 0; j < 12; ++j) {
      MockFetch* fetch = fetch_vector[j];
      if (j < 4 * (i + 1)) {
        EXPECT_TRUE(fetch->done());
        EXPECT_TRUE(fetch->success());
        EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
        EXPECT_STREQ(j % 2 == 0 ? body1_ : body2_, fetch->content());
        EXPECT_FALSE(fetch_vector[i]->response_headers()->Has(
                         HttpAttributes::kXPsaLoadShed));
      } else {
        EXPECT_FALSE(fetch->done());
        EXPECT_FALSE(fetch->success());
      }
    }
    for (int j = 100; j < 104; ++j) {
      MockFetch* fetch = fetch_vector[j];
      if (j < 100  + 2 * (i + 1)) {
        EXPECT_TRUE(fetch->done());
        EXPECT_TRUE(fetch->success());
        EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
        EXPECT_STREQ(body3_, fetch->content());
        EXPECT_FALSE(fetch_vector[i]->response_headers()->Has(
                         HttpAttributes::kXPsaLoadShed));
      } else {
        EXPECT_FALSE(fetch->done());
        EXPECT_FALSE(fetch->success());
      }
    }
  }

  // Domain3's request gets fetched correctly.
  MockFetch* fetch = fetch_vector[100];
  EXPECT_TRUE(fetch->done());
  EXPECT_TRUE(fetch->success());
  EXPECT_EQ(HttpStatus::kOK, fetch->response_headers()->status_code());
  EXPECT_STREQ(body3_, fetch->content());
  EXPECT_FALSE(
      fetch->response_headers()->Has(HttpAttributes::kXPsaLoadShed));

  EXPECT_EQ(10, stats_.GetTimedVariable(
      RateController::kQueuedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(94, stats_.GetTimedVariable(
      RateController::kDroppedFetchCount)->Get(
          TimedVariable::START));
  EXPECT_EQ(0, global_fetch_queue_size());

  STLDeleteContainerPointers(fetch_vector.begin(), fetch_vector.end());
}

}  // namespace

}  // namespace net_instaweb
