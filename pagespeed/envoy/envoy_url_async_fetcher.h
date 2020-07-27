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
// Fetch the resources asynchronously using envoy. The fetcher is called in
// the rewrite thread.
//

#pragma once
#include <vector>

#include "envoy_cluster_manager.h"
#include "envoy_fetch.h"
#include "envoy_logger.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class Variable;
class EnvoyFetch;

struct EnvoyStats {
  static const char kEnvoyFetchRequestCount[];
  static const char kEnvoyFetchByteCount[];
  static const char kEnvoyFetchTimeDurationMs[];
  static const char kEnvoyFetchCancelCount[];
  static const char kEnvoyFetchActiveCount[];
  static const char kEnvoyFetchTimeoutCount[];
  static const char kEnvoyFetchFailureCount[];
  static const char kEnvoyFetchCertErrors[];
  static const char kEnvoyFetchReadCalls[];

  // A fetch that finished with a 2xx or a 3xx code --- and not just a
  // mechanically successful one that's a 4xx or such.
  static const char kEnvoyFetchUltimateSuccess[];

  // A failure or an error status. Doesn't include fetches dropped due to
  // process exit and the like.
  static const char kEnvoyFetchUltimateFailure[];

  // When we last checked the ultimate failure/success numbers for a
  // possible concern.
  static const char kEnvoyFetchLastCheckTimestampMs[];
};

class EnvoyUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  EnvoyUrlAsyncFetcher(const char* proxy, ThreadSystem* thread_system,
                       Statistics* statistics, Timer* timer, int64 timeout_ms,
                       MessageHandler* handler);

  ~EnvoyUrlAsyncFetcher() override;
  static void InitStats(Statistics* statistics);

  // It should be called in the module init_process callback function. Do some
  // intializations which can't be done in the master process
  bool Init();

  // shutdown all the fetches.
  void ShutDown() override;

  bool SupportsHttps() const override { return false; }

  void Fetch(const GoogleString& url, MessageHandler* message_handler,
             AsyncFetch* callback) override;

  // Remove the completed fetch from the active fetch set, and put it into a
  // completed fetch list to be cleaned up.
  void FetchComplete(EnvoyFetch* fetch);
  void PrintActiveFetches(MessageHandler* handler) const;

  // Indicates that it should track the original content length for
  // fetched resources.
  bool track_original_content_length() {
    return track_original_content_length_;
  }
  void set_track_original_content_length(bool x) {
    track_original_content_length_ = x;
  }

  // AnyPendingFetches is accurate only at the time of call; this is
  // used conservatively during shutdown.  It counts fetches that have been
  // requested by some thread, and can include fetches for which no action
  // has yet been taken (ie fetches that are not active).
  virtual bool AnyPendingFetches() { return !active_fetches_.empty(); }

  // ApproximateNumActiveFetches can under- or over-count and is used only for
  // error reporting.
  int ApproximateNumActiveFetches() { return active_fetches_.size(); }

  void CancelActiveFetches();

  // These must be accessed with mutex_ held.
  bool shutdown() const { return shutdown_; }
  void set_shutdown(bool s) { shutdown_ = s; }

 protected:
  typedef Pool<EnvoyFetch> EnvoyFetchPool;

 private:
  void FetchWithEnvoy();

  static void TimeoutHandler();
  static bool ParseUrl();
  friend class EnvoyFetch;

  EnvoyFetchPool active_fetches_;

  std::unique_ptr<EnvoyClusterManager> cluster_manager_ptr_;
  std::unique_ptr<PagespeedLogSink> envoy_log_sink_;
  EnvoyFetchPool pending_fetches_;
  EnvoyFetchPool completed_fetches_;
  char* proxy_;

  int fetchers_count_;
  bool shutdown_;
  bool track_original_content_length_;
  int64 byte_count_;
  ThreadSystem* thread_system_;
  MessageHandler* message_handler_;
  // Protect the member variable in this class
  // active_fetches, pending_fetches
  ThreadSystem::CondvarCapableMutex* mutex_;

  int64 resolver_timeout_;
  int64 fetch_timeout_;

  DISALLOW_COPY_AND_ASSIGN(EnvoyUrlAsyncFetcher);
};

}  // namespace net_instaweb