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

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/envoy/envoy_url_async_fetcher.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
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
#include "pagespeed/system/tcp_server_thread_for_testing.h"

namespace net_instaweb {
const int kFetcherTimeoutMs = 5 * 1000;
const int kFetcherTimeoutValgrindMs = 20 * 1000;

class EnvoyUrlAsyncFetcherTest : public ::testing::Test {
public:
  static void SetUpTestCase() {}

protected:
  EnvoyUrlAsyncFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        message_handler_(thread_system_->NewMutex()), flaky_retries_(0),
        fetcher_timeout_ms_(FetcherTimeoutMs()) {}

  static int64 FetcherTimeoutMs() {
    return RunningOnValgrind() ? kFetcherTimeoutValgrindMs : kFetcherTimeoutMs;
  }

  virtual void TearDown() { timer_.reset(NULL); }
  scoped_ptr<Timer> timer_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler message_handler_;
  scoped_ptr<SimpleStats> statistics_;
  int64 flaky_retries_;
  int64 fetcher_timeout_ms_;
};

TEST_F(EnvoyUrlAsyncFetcherTest, FetchURL) {
  timer_.reset(Platform::CreateTimer());
  statistics_.reset(new SimpleStats(thread_system_.get()));
  EnvoyUrlAsyncFetcher::InitStats(statistics_.get());

  new EnvoyUrlAsyncFetcher("", thread_system_.get(), statistics_.get(), timer_.get(),
                           fetcher_timeout_ms_, &message_handler_);
}

} // namespace net_instaweb