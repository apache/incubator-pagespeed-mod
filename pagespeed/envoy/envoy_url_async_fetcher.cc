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

#include "envoy_url_async_fetcher.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "envoy_fetch.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/pool_element.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

namespace net_instaweb {
const char EnvoyStats::kEnvoyFetchRequestCount[] = "envoy_fetch_request_count";
const char EnvoyStats::kEnvoyFetchByteCount[] = "envoy_fetch_bytes_count";
const char EnvoyStats::kEnvoyFetchTimeDurationMs[] =
    "envoy_fetch_time_duration_ms";
const char EnvoyStats::kEnvoyFetchCancelCount[] = "envoy_fetch_cancel_count";
const char EnvoyStats::kEnvoyFetchActiveCount[] = "envoy_fetch_active_count";
const char EnvoyStats::kEnvoyFetchTimeoutCount[] = "envoy_fetch_timeout_count";
const char EnvoyStats::kEnvoyFetchFailureCount[] = "envoy_fetch_failure_count";
const char EnvoyStats::kEnvoyFetchCertErrors[] = "envoy_fetch_cert_errors";
const char EnvoyStats::kEnvoyFetchReadCalls[] = "envoy_fetch_num_calls_to_read";
const char EnvoyStats::kEnvoyFetchUltimateSuccess[] =
    "envoy_fetch_ultimate_success";
const char EnvoyStats::kEnvoyFetchUltimateFailure[] =
    "envoy_fetch_ultimate_failure";
const char EnvoyStats::kEnvoyFetchLastCheckTimestampMs[] =
    "envoy_fetch_last_check_timestamp_ms";

EnvoyUrlAsyncFetcher::EnvoyUrlAsyncFetcher(const char* proxy,
                                           ThreadSystem* thread_system,
                                           Statistics* statistics, Timer* timer,
                                           int64 timeout_ms,
                                           MessageHandler* handler)
    : fetchers_count_(0),
      shutdown_(false),
      track_original_content_length_(false),
      byte_count_(0),
      thread_system_(thread_system),
      message_handler_(handler),
      mutex_(nullptr) {
  if (!Init()) {
    shutdown_ = true;
    message_handler_->Message(
        kError, "EnvoyUrlAsyncFetcher failed to init, fetching disabled.");
  }
}

EnvoyUrlAsyncFetcher::~EnvoyUrlAsyncFetcher() { CHECK(shutdown_); }

bool EnvoyUrlAsyncFetcher::Init() {
  cluster_manager_ptr_ = std::make_unique<EnvoyClusterManager>();
  envoy_log_sink_ = std::make_unique<PagespeedLogSink>(
      Envoy::Logger::Registry::getSink(), message_handler_);
  return true;
}

void EnvoyUrlAsyncFetcher::InitStats(Statistics* statistics) {
  statistics->AddVariable(EnvoyStats::kEnvoyFetchRequestCount);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchByteCount);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchTimeDurationMs);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchCancelCount);
  statistics->AddUpDownCounter(EnvoyStats::kEnvoyFetchActiveCount);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchTimeoutCount);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchFailureCount);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchCertErrors);
#ifndef NDEBUG
  statistics->AddVariable(EnvoyStats::kEnvoyFetchReadCalls);
#endif
  statistics->AddVariable(EnvoyStats::kEnvoyFetchUltimateSuccess);
  statistics->AddVariable(EnvoyStats::kEnvoyFetchUltimateFailure);
  statistics->AddUpDownCounter(EnvoyStats::kEnvoyFetchLastCheckTimestampMs);
}

void EnvoyUrlAsyncFetcher::ShutDown() {
  if (cluster_manager_ptr_ != nullptr) {
    cluster_manager_ptr_->ShutDown();
    cluster_manager_ptr_ = nullptr;
  }
  shutdown_ = true;
}

void EnvoyUrlAsyncFetcher::Fetch(const GoogleString& url,
                                 MessageHandler* message_handler,
                                 AsyncFetch* async_fetch) {
  std::unique_ptr<EnvoyFetch> envoy_fetch_ptr = std::make_unique<EnvoyFetch>(
      url, async_fetch, message_handler, *cluster_manager_ptr_);
  envoy_fetch_ptr->Start();
}

void EnvoyUrlAsyncFetcher::FetchComplete(EnvoyFetch* fetch) {}

void EnvoyUrlAsyncFetcher::PrintActiveFetches(MessageHandler* handler) const {}

void EnvoyUrlAsyncFetcher::CancelActiveFetches() {}

bool EnvoyUrlAsyncFetcher::ParseUrl() { return false; }

}  // namespace net_instaweb