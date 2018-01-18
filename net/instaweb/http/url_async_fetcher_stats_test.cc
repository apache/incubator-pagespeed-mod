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

#include "net/instaweb/http/public/url_async_fetcher_stats.h"

#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/mock_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/sharedmem/inprocess_shared_mem.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {
namespace {

const char kUrl[] = "http://www.example.com/";

// A little helper class that manages all the objects we need to
// set up full-fledged histogram-capable statistics in-process for testing.
class StatsMaker {
 public:
  StatsMaker()
      : threads_(Platform::CreateThreadSystem()),
        timer_(threads_->NewMutex(), MockTimer::kApr_5_2010_ms),
        fs_(threads_.get(), &timer_),
        mem_runtime_(new InProcessSharedMem(threads_.get())),
        stats_(new SharedMemStatistics(3000 /* log dump interval */,
                                       100000 /* max log size kb */,
                                       "/stats.log", false /* no log */,
                                       "in_mem", mem_runtime_.get(),
                                       &message_handler_, &fs_, &timer_)) {
    UrlAsyncFetcherStats::InitStats("test", stats_.get());
    stats_->Init(true, &message_handler_);
  }

  ~StatsMaker() {
    stats_->GlobalCleanup(&message_handler_);
  }

  Statistics* stats() { return stats_.get(); }

 protected:
  scoped_ptr<ThreadSystem> threads_;
  MockTimer timer_;
  MemFileSystem fs_;
  GoogleMessageHandler message_handler_;
  scoped_ptr<InProcessSharedMem> mem_runtime_;
  scoped_ptr<SharedMemStatistics> stats_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatsMaker);
};

class UrlAsyncFetcherStatsTest : public testing::Test {
 protected:
  UrlAsyncFetcherStatsTest()
      : thread_system_(Platform::CreateThreadSystem()),
        timer_(thread_system_->NewMutex(), MockTimer::kApr_5_2010_ms),
        wait_fetcher_(&mock_fetcher_, new NullMutex),
        stats_fetcher_("test", &wait_fetcher_, &timer_, stats_) {
    // We don't want delays unless we're testing timing stuff.
    wait_fetcher_.SetPassThroughMode(true);
  }

  // We use per-fixture (rather than per-test) setup and teardown to
  // manage the stats to better model their real-life use and better cover
  // ::Initialize.
  static void SetUpTestCase() {
    testing::Test::SetUpTestCase();
    stats_maker_ = new StatsMaker();
    stats_ = stats_maker_->stats();
  }

  static void TearDownTestCase() {
    delete stats_maker_;
    stats_maker_ = NULL;
    stats_ = NULL;
    testing::Test::TearDownTestCase();
  }

  static StatsMaker* stats_maker_;
  static Statistics* stats_;

  GoogleMessageHandler message_handler_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer timer_;
  MockUrlFetcher mock_fetcher_;
  WaitUrlAsyncFetcher wait_fetcher_;
  UrlAsyncFetcherStats stats_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlAsyncFetcherStatsTest);
};

StatsMaker* UrlAsyncFetcherStatsTest::stats_maker_;
Statistics* UrlAsyncFetcherStatsTest::stats_;

TEST_F(UrlAsyncFetcherStatsTest, BasicOperation) {
  ResponseHeaders headers;
  headers.set_first_line(1, 1, 200, "OK");
  const char kBody[] = "payload!";
  mock_fetcher_.SetResponse(kUrl, headers, kBody);

  ExpectStringAsyncFetch target(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  stats_fetcher_.Fetch(kUrl, &message_handler_, &target);
  EXPECT_STREQ(kBody, target.buffer());

  // Make sure we update stats OK.
  EXPECT_EQ(1, stats_->GetVariable("test_fetches")->Get());
  EXPECT_EQ(STATIC_STRLEN(kBody),
            stats_->GetVariable("test_bytes_fetched")->Get());
  EXPECT_EQ(target.response_headers()->SizeEstimate(),
            stats_->GetVariable("test_approx_header_bytes_fetched")->Get());

  ExpectStringAsyncFetch target2(
      false, RequestContext::NewTestRequestContext(thread_system_.get()));
  mock_fetcher_.set_fail_on_unexpected(false);
  stats_fetcher_.Fetch(StrCat(kUrl, "Not"), &message_handler_, &target2);

  // 1 more response, but no additional payload bytes.
  EXPECT_EQ(2, stats_->GetVariable("test_fetches")->Get());
  EXPECT_EQ(STATIC_STRLEN(kBody),
            stats_->GetVariable("test_bytes_fetched")->Get());
  EXPECT_EQ(target.response_headers()->SizeEstimate() +
                target2.response_headers()->SizeEstimate(),
            stats_->GetVariable("test_approx_header_bytes_fetched")->Get());
}

TEST_F(UrlAsyncFetcherStatsTest, GzipHandling) {
  stats_->Clear();

  // Make sure we measure what's transferred, not after gunzip'ing, and that
  // we decompress right.
  const char kOriginal[] = "Hello, gzip!";

  // This was gotten by sniffing a gzip'd transfer of the text above.
  const char kCompressed[] =
      "\x1f\x8b\x08\x00\x00\x00\x00\x00"
      "\x00\x03\xf3\x48\xcd\xc9\xc9\xd7"
      "\x51\x48\xaf\xca\x2c\x50\x04\x00"
      "\x3e\x3d\x0f\x10\x0c\x00\x00\x00";

  // The test isn't usable if this doesn't hold.
  ASSERT_NE(STATIC_STRLEN(kCompressed), STATIC_STRLEN(kOriginal));

  StringPiece compressed(kCompressed, STATIC_STRLEN(kCompressed));

  ResponseHeaders headers;
  headers.set_first_line(1, 1, 200, "OK");
  headers.Add(HttpAttributes::kContentEncoding, "gzip");
  mock_fetcher_.SetResponse(kUrl, headers, compressed);

  stats_fetcher_.set_fetch_with_gzip(true);
  ExpectStringAsyncFetch target(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  stats_fetcher_.Fetch(kUrl, &message_handler_, &target);
  EXPECT_STREQ(kOriginal, target.buffer());

  EXPECT_EQ(1, stats_->GetVariable("test_fetches")->Get());
  EXPECT_EQ(STATIC_STRLEN(kCompressed),
            stats_->GetVariable("test_bytes_fetched")->Get());
}

TEST_F(UrlAsyncFetcherStatsTest, TimeMeasurement) {
  // Test the we collect timing measurements properly.
  stats_->Clear();
  wait_fetcher_.SetPassThroughMode(false);

  ResponseHeaders headers;
  headers.set_first_line(1, 1, 200, "OK");
  const char kBody[] = "payload!";
  mock_fetcher_.SetResponse(kUrl, headers, kBody);

  ExpectStringAsyncFetch target(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  stats_fetcher_.Fetch(kUrl, &message_handler_, &target);
  EXPECT_FALSE(target.done());

  Histogram* timings = stats_->GetHistogram("test_fetch_latency_us");
  EXPECT_EQ(0, timings->Count());

  timer_.AdvanceUs(42);
  wait_fetcher_.CallCallbacks();
  EXPECT_TRUE(target.done());
  EXPECT_EQ(1, timings->Count());
  EXPECT_DOUBLE_EQ(42, timings->Average());

  // Now do an another fetch, this with a 2 us.
  ExpectStringAsyncFetch target2(
      true, RequestContext::NewTestRequestContext(thread_system_.get()));
  stats_fetcher_.Fetch(kUrl, &message_handler_, &target2);
  EXPECT_FALSE(target2.done());
  timer_.AdvanceUs(2);
  wait_fetcher_.CallCallbacks();
  EXPECT_TRUE(target2.done());
  EXPECT_EQ(2, timings->Count());
  EXPECT_DOUBLE_EQ(22, timings->Average());  // (42 + 2) / 2 = 22
}

}  // namespace
}  // namespace net_instaweb
