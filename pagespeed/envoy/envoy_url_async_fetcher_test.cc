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

#include "pagespeed/envoy/envoy_url_async_fetcher.h"

#include <memory>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/statistics_template.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/gzip_inflater.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "test/pagespeed/kernel/base/gtest.h"
#include "test/pagespeed/kernel/base/mock_message_handler.h"

namespace net_instaweb {

namespace {

// Default domain to test URL fetches from.  If the default site is
// down, the tests can be directed to a backup host by setting the
// environment variable PAGESPEED_TEST_HOST.  Note that this relies on
// 'mod_pagespeed_examples/' and 'do_not_modify/' being available
// relative to the domain, by copying them into /var/www from
// MOD_PAGESPEED_SVN_PATH/src/install.
const char kFetchHost[] = "selfsigned.modpagespeed.com";

// const int kThreadedPollMs = 200;
// const int kFetcherTimeoutMs = 5 * 1000;
const int kFetcherTimeoutValgrindMs = 20 * 1000;

// const int kModpagespeedSite = 0; // TODO(matterbury): These should be an
// enum? const int kGoogleFavicon = 1; const int kGoogleLogo = 2; const int
// kCgiSlowJs = 3; const int kModpagespeedBeacon = 4; const int
// kConnectionRefused = 5; const int kNoContent = 6; const int
// kNextTestcaseIndex = 7; // Should always be last.

// Note: We do not subclass StringAsyncFetch because we want to lock access
// to done_.
class EnvoyTestFetch : public AsyncFetch {
 public:
  explicit EnvoyTestFetch(const RequestContextPtr& ctx, AbstractMutex* mutex)
      : AsyncFetch(ctx), mutex_(mutex), success_(false), done_(false) {}

  bool HandleWrite(const StringPiece& content,
                   MessageHandler* handler) override {
    content.AppendToString(&buffer_);
    return true;
  }
  bool HandleFlush(MessageHandler* handler) override { return true; }
  void HandleHeadersComplete() override {}
  void HandleDone(bool success) override {
    ScopedMutex lock(mutex_);
    EXPECT_FALSE(done_);
    success_ = success;
    done_ = true;
  }

  const GoogleString& buffer() const { return buffer_; }
  bool success() const { return success_; }
  bool IsDone() const {
    ScopedMutex lock(mutex_);
    return done_;
  }

  void Reset() override {
    ScopedMutex lock(mutex_);
    AsyncFetch::Reset();
    done_ = false;
    success_ = false;
    response_headers()->Clear();
  }

 private:
  AbstractMutex* mutex_;
  GoogleString buffer_;
  bool success_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(EnvoyTestFetch);
};

}  // namespace

class EnvoyUrlAsyncFetcherTest : public ::testing::Test {
 protected:
  EnvoyUrlAsyncFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        message_handler_(thread_system_->NewMutex()),
        flaky_retries_(0),
        fetcher_timeout_ms_(FetcherTimeoutMs()) {}

  void SetUp() override { SetUpWithProxy(""); }

  static int64 FetcherTimeoutMs() {
    return kFetcherTimeoutValgrindMs;
    // TODO(oschaaf): figure out substitute for deprecated absl::RunningOnValgrind()
    // return RunningOnValgrind() ? kFetcherTimeoutValgrindMs : kFetcherTimeoutMs;
  }

  void SetUpWithProxy(const char* proxy) {
    const char* env_host = getenv("PAGESPEED_TEST_HOST");
    if (env_host != nullptr) {
      test_host_ = env_host;
    }
    if (test_host_.empty()) {
      test_host_ = kFetchHost;
    }
    GoogleString fetch_test_domain = StrCat("//", test_host_);
    timer_.reset(Platform::CreateTimer());
    statistics_ = std::make_unique<SimpleStats>(thread_system_.get());
    EnvoyUrlAsyncFetcher::InitStats(statistics_.get());
    envoy_url_async_fetcher_ = std::make_unique<EnvoyUrlAsyncFetcher>(
        proxy, thread_system_.get(), statistics_.get(), timer_.get(),
        fetcher_timeout_ms_, &message_handler_);
    mutex_.reset(thread_system_->NewMutex());

    // Set initial timestamp so we don't roll-over monitoring stats right after
    // start.
    statistics_->GetUpDownCounter(EnvoyStats::kEnvoyFetchLastCheckTimestampMs)
        ->Set(timer_->NowMs());
  }

  void TearDown() override {
    // Need to free the fetcher before destroy the pool.
    envoy_url_async_fetcher_->ShutDown();
    STLDeleteElements(&fetches_);
  }

  // Adds a new URL & expected response to the url/response structure, returning
  // its index so it can be passed to StartFetch/StartFetches etc.
  int AddTestUrl(const GoogleString& url, const GoogleString& content_start) {
    urls_.push_back(url);
    content_starts_.push_back(content_start);
    int index = fetches_.size();
    fetches_.push_back(new EnvoyTestFetch(
        RequestContext::NewTestRequestContext(thread_system_.get()),
        mutex_.get()));
    return index;
  }

  void StartFetch(int idx) {
    fetches_[idx]->Reset();
    envoy_url_async_fetcher_->Fetch(urls_[idx], &message_handler_,
                                    fetches_[idx]);
  }

  void StartFetches(size_t first, size_t last) {
    for (size_t idx = first; idx <= last; ++idx) {
      StartFetch(idx);
    }
  }

  std::unique_ptr<EnvoyUrlAsyncFetcher> envoy_url_async_fetcher_;
  std::unique_ptr<ThreadSystem> thread_system_;
  std::unique_ptr<AbstractMutex> mutex_;
  std::unique_ptr<EnvoyTestFetch> envoy_fetch_;
  MockMessageHandler message_handler_;
  int64 flaky_retries_;

 private:
  std::vector<EnvoyTestFetch*> fetches_;
  std::vector<GoogleString> content_starts_;
  std::vector<GoogleString> urls_;
  std::unique_ptr<Timer> timer_;
  std::unique_ptr<SimpleStats> statistics_;

  int64 fetcher_timeout_ms_;
  GoogleString test_host_;
};

TEST_F(EnvoyUrlAsyncFetcherTest, DISABLED_FetchURL) {
  GoogleString starts_with = "<!doctype html";
  envoy_fetch_ = std::make_unique<EnvoyTestFetch>(
      RequestContext::NewTestRequestContext(thread_system_.get()),
      mutex_.get());
  envoy_url_async_fetcher_->Fetch(
      "http://selfsigned.modpagespeed.com/mod_pagespeed_example/index.html",
      &message_handler_, envoy_fetch_.get());
  EXPECT_STREQ(starts_with,
               envoy_fetch_->buffer().substr(0, starts_with.size()));
}

}  // namespace net_instaweb