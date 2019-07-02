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


#include "pagespeed/system/system_server_context.h"

#include "base/logging.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/http/public/url_async_fetcher_stats.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/split_statistics.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/system/add_headers_fetcher.h"
#include "pagespeed/system/loopback_route_fetcher.h"
#include "pagespeed/system/system_cache_path.h"
#include "pagespeed/system/system_caches.h"
#include "pagespeed/system/system_request_context.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace net_instaweb {

class PurgeSet;

namespace {

const char kHtmlRewriteTimeUsHistogram[] = "Html Time us Histogram";
const char kLocalFetcherStatsPrefix[] = "http";
const char kCacheFlushCount[] = "cache_flush_count";
const char kCacheFlushTimestampMs[] = "cache_flush_timestamp_ms";
const char kStatistics404Count[] = "statistics_404_count";

}  // namespace

SystemServerContext::SystemServerContext(
    RewriteDriverFactory* factory, StringPiece hostname, int port)
    : ServerContext(factory),
      initialized_(false),
      use_per_vhost_statistics_(false),
      cache_flush_mutex_(thread_system()->NewMutex()),
      last_cache_flush_check_sec_(0),
      cache_flush_count_(NULL),         // Lazy-initialized under mutex.
      cache_flush_timestamp_ms_(NULL),  // Lazy-initialized under mutex.
      html_rewrite_time_us_histogram_(NULL),
      local_statistics_(NULL),
      hostname_identifier_(StrCat(hostname, ":", IntegerToString(port))),
      system_caches_(NULL),
      cache_path_(NULL) {
  global_system_rewrite_options()->set_description(hostname_identifier_);
}

SystemServerContext::~SystemServerContext() {
  if (cache_path_ != NULL) {
    cache_path_->RemoveServerContext(this);
  }
}

void SystemServerContext::SetCachePath(SystemCachePath* cache_path) {
  DCHECK(cache_path_ == NULL);
  cache_path_ = cache_path;
  cache_path->AddServerContext(this);
}

// If we haven't checked the timestamp of $FILE_PREFIX/cache.flush in the past
// cache_flush_poll_interval_sec_ seconds do so, and if the timestamp has
// expired then update the cache_invalidation_timestamp in global_options,
// thus flushing the cache.
void SystemServerContext::FlushCacheIfNecessary() {
  if (global_system_rewrite_options()->enable_cache_purge()) {
    cache_path_->FlushCacheIfNecessary();
  } else {
    CheckLegacyGlobalCacheFlushFile();
  }
}

void SystemServerContext::CheckLegacyGlobalCacheFlushFile() {
  int64 cache_flush_poll_interval_sec =
      global_system_rewrite_options()->cache_flush_poll_interval_sec();
  if (cache_flush_poll_interval_sec > 0) {
    int64 now_sec = timer()->NowMs() / Timer::kSecondMs;
    bool check_cache_file = false;
    {
      ScopedMutex lock(cache_flush_mutex_.get());
      if (now_sec >= (last_cache_flush_check_sec_ +
                      cache_flush_poll_interval_sec)) {
        last_cache_flush_check_sec_ = now_sec;
        check_cache_file = true;
      }
      if (cache_flush_count_ == NULL) {
        cache_flush_count_ = statistics()->GetVariable(kCacheFlushCount);
      }
      if (cache_flush_timestamp_ms_ == NULL) {
        cache_flush_timestamp_ms_ = statistics()->GetUpDownCounter(
            kCacheFlushTimestampMs);
      }
    }

    if (check_cache_file) {
      GoogleString cache_flush_filename =
          global_system_rewrite_options()->cache_flush_filename();
      if (cache_flush_filename.empty()) {
        cache_flush_filename = "cache.flush";
      }
      if (cache_flush_filename[0] != '/') {
        // Implementations must ensure the file cache path is an absolute path.
        // mod_pagespeed checks in mod_instaweb.cc:pagespeed_post_config while
        // ngx_pagespeed checks in ngx_pagespeed.cc:ps_merge_srv_conf.
        DCHECK_EQ('/', global_system_rewrite_options()->file_cache_path()[0]);
        cache_flush_filename = StrCat(
            global_system_rewrite_options()->file_cache_path(), "/",
            cache_flush_filename);
      }
      int64 cache_flush_timestamp_sec;
      NullMessageHandler null_handler;
      if (file_system()->Mtime(cache_flush_filename,
                               &cache_flush_timestamp_sec,
                               &null_handler)) {
        int64 timestamp_ms = cache_flush_timestamp_sec * Timer::kSecondMs;

        bool flushed = UpdateCacheFlushTimestampMs(timestamp_ms);

        // The multiple child processes each must independently
        // discover a fresh cache.flush and update the options. However,
        // as shown in
        //     http://github.com/apache/incubator-pagespeed-mod/issues/568
        // we should only bump the flush-count and print a warning to
        // the log once per new timestamp.
        if (flushed &&
            (timestamp_ms !=
             cache_flush_timestamp_ms_->SetReturningPreviousValue(
                 timestamp_ms))) {
          int count = cache_flush_count_->Add(1);
          message_handler()->Message(kWarning, "Cache Flush %d", count);
        }
      }
    } else {
      // Check on every request whether another child process has updated the
      // statistic.
      int64 timestamp_ms = cache_flush_timestamp_ms_->Get();

      // Do the difference-check first because that involves only a
      // reader-lock, so we have zero contention risk when the cache is not
      // being flushed.
      if ((timestamp_ms > 0) &&
          global_options()->has_cache_invalidation_timestamp_ms() &&
          (global_options()->cache_invalidation_timestamp() < timestamp_ms)) {
        UpdateCacheFlushTimestampMs(timestamp_ms);
      }
    }
  }
}

void SystemServerContext::UpdateCachePurgeSet(
    const CopyOnWrite<PurgeSet>& purge_set) {
  global_options()->UpdateCachePurgeSet(purge_set);
  if (cache_flush_count_ == NULL) {
    cache_flush_count_ = statistics()->GetVariable(kCacheFlushCount);
  }
  cache_flush_count_->Add(1);
}

bool SystemServerContext::UpdateCacheFlushTimestampMs(int64 timestamp_ms) {
  return global_options()->UpdateCacheInvalidationTimestampMs(timestamp_ms);
}

void SystemServerContext::AddHtmlRewriteTimeUs(int64 rewrite_time_us) {
  if (html_rewrite_time_us_histogram_ != NULL) {
    html_rewrite_time_us_histogram_->Add(rewrite_time_us);
  }
}

SystemRewriteOptions* SystemServerContext::global_system_rewrite_options() {
  SystemRewriteOptions* out =
      dynamic_cast<SystemRewriteOptions*>(global_options());
  CHECK(out != NULL);
  return out;
}

void SystemServerContext::PostInitHook() {
  ServerContext::PostInitHook();
  admin_site_.reset(new AdminSite(static_asset_manager(), timer(),
                                  message_handler()));
}

void SystemServerContext::CreateLocalStatistics(
    Statistics* global_statistics,
    SystemRewriteDriverFactory* factory) {
  local_statistics_ =
      factory->AllocateAndInitSharedMemStatistics(
          true /* local */, hostname_identifier(),
          *global_system_rewrite_options());
  split_statistics_.reset(new SplitStatistics(
      factory->thread_system(), local_statistics_, global_statistics));
  // local_statistics_ was ::InitStat'd by AllocateAndInitSharedMemStatistics,
  // but we need to take care of split_statistics_.
  factory->NonStaticInitStats(split_statistics_.get());
}

void SystemServerContext::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheFlushCount);
  statistics->AddUpDownCounter(kCacheFlushTimestampMs);
  statistics->AddVariable(kStatistics404Count);
  Histogram* html_rewrite_time_us_histogram =
      statistics->AddHistogram(kHtmlRewriteTimeUsHistogram);
  // We set the boundary at 2 seconds which is about 2 orders of magnitude worse
  // than anything we have reasonably seen, to make sure we don't cut off actual
  // samples.
  html_rewrite_time_us_histogram->SetMaxValue(2 * Timer::kSecondUs);
  UrlAsyncFetcherStats::InitStats(kLocalFetcherStatsPrefix, statistics);
}

Variable* SystemServerContext::statistics_404_count() {
  return statistics()->GetVariable(kStatistics404Count);
}

void SystemServerContext::ChildInit(SystemRewriteDriverFactory* factory) {
  DCHECK(!initialized_);
  use_per_vhost_statistics_ = factory->use_per_vhost_statistics();
  if (!initialized_ && !global_options()->unplugged()) {
    initialized_ = true;
    system_caches_ = factory->caches();
    set_lock_manager(factory->caches()->GetLockManager(
        global_system_rewrite_options()));
    UrlAsyncFetcher* fetcher =
        factory->GetFetcher(global_system_rewrite_options());
    set_default_system_fetcher(fetcher);

    if (split_statistics_.get() != NULL) {
      // Readjust the SHM stuff for the new process
      local_statistics_->Init(false, message_handler());

      // Create local stats for the ServerContext, and fill in its
      // statistics() and rewrite_stats() using them; if we didn't do this here
      // they would get set to the factory's by the InitServerContext call
      // below.
      set_statistics(split_statistics_.get());
      local_rewrite_stats_.reset(new RewriteStats(
          factory->HasWaveforms(), split_statistics_.get(),
          factory->thread_system(), factory->timer()));
      set_rewrite_stats(local_rewrite_stats_.get());

      // In case of gzip fetching, we will have the UrlAsyncFetcherStats take
      // care of decompression rather than the original fetcher, so we get
      // correct numbers for bytes fetched.
      bool fetch_with_gzip = global_system_rewrite_options()->fetch_with_gzip();
      if (fetch_with_gzip) {
        fetcher->set_fetch_with_gzip(false);
      }
      stats_fetcher_.reset(new UrlAsyncFetcherStats(
          kLocalFetcherStatsPrefix, fetcher,
          factory->timer(), split_statistics_.get()));
      if (fetch_with_gzip) {
        stats_fetcher_->set_fetch_with_gzip(true);
      }
      set_default_system_fetcher(stats_fetcher_.get());
    }

    // To allow Flush to come in while multiple threads might be
    // referencing the signature, we must be able to mutate the
    // timestamp and signature atomically.  RewriteOptions supports
    // an optional read/writer lock for this purpose.
    global_options()->set_cache_invalidation_timestamp_mutex(
        thread_system()->NewRWLock());
    factory->InitServerContext(this);

    html_rewrite_time_us_histogram_ = statistics()->GetHistogram(
        kHtmlRewriteTimeUsHistogram);
    html_rewrite_time_us_histogram_->SetMaxValue(2 * Timer::kSecondUs);
  }
}


void SystemServerContext::ApplySessionFetchers(
    const RequestContextPtr& request, RewriteDriver* driver) {
  const SystemRewriteOptions* conf =
      SystemRewriteOptions::DynamicCast(driver->options());
  CHECK(conf != NULL);
  SystemRequestContext* system_request = SystemRequestContext::DynamicCast(
      request.get());
  if (system_request == NULL) {
    return;  // decoding_driver has a null RequestContext.
  }

  // Note that these fetchers are applied in the opposite order of how they are
  // added: the last one added here is the first one applied and vice versa.
  //
  // Currently, we want AddHeadersFetcher running first, then
  // LoopbackRouteFetcher (and then Serf).
  SystemRewriteOptions* options = global_system_rewrite_options();
  if (!options->disable_loopback_routing() &&
      !options->slurping_enabled() &&
      !options->test_proxy()) {
    // Note the port here is our port, not from the request, since
    // LoopbackRouteFetcher may decide we should be talking to ourselves.
    driver->SetSessionFetcher(new LoopbackRouteFetcher(
        driver->options(), system_request->local_ip(),
        system_request->local_port(), driver->async_fetcher()));
  }

  if (driver->options()->num_custom_fetch_headers() > 0) {
    driver->SetSessionFetcher(new AddHeadersFetcher(driver->options(),
                                                    driver->async_fetcher()));
  }
}

void SystemServerContext::CollapseConfigOverlaysAndComputeSignatures() {
  ComputeSignature(global_system_rewrite_options());
}

// Handler which serves PSOL console.
void SystemServerContext::ConsoleHandler(
    const SystemRewriteOptions& options,
    AdminSite::AdminSource source,
    const QueryParams& query_params, AsyncFetch* fetch) {
  admin_site_->ConsoleHandler(*global_system_rewrite_options(), options,
                              source, query_params, fetch, statistics());
}

void SystemServerContext::StatisticsHandler(
    const RewriteOptions& options,
    bool is_global_request,
    AdminSite::AdminSource source,
    AsyncFetch* fetch) {
  if (!use_per_vhost_statistics_) {
    is_global_request = true;
  }
  Statistics* stats = is_global_request ? factory()->statistics()
      : statistics();
  admin_site_->StatisticsHandler(options, source, fetch, stats);
}

void SystemServerContext::ConsoleJsonHandler(
    const QueryParams& params, AsyncFetch* fetch) {
  admin_site_->ConsoleJsonHandler(params, fetch, statistics());
}

void SystemServerContext::PrintHistograms(
    bool is_global_request,
    AdminSite::AdminSource source,
    AsyncFetch* fetch) {
  Statistics* stats = is_global_request ? factory()->statistics()
      : statistics();
  admin_site_->PrintHistograms(source, fetch, stats);
}

void SystemServerContext::PrintCaches(bool is_global,
                                      AdminSite::AdminSource source,
                                      const GoogleUrl& stripped_gurl,
                                      const QueryParams& query_params,
                                      const RewriteOptions* options,
                                      AsyncFetch* fetch) {
  admin_site_->PrintCaches(is_global, source, stripped_gurl, query_params,
                           options, cache_path(), fetch, system_caches_,
                           filesystem_metadata_cache(), http_cache(),
                           metadata_cache(), page_property_cache(), this);
}

void SystemServerContext::PrintConfig(
    AdminSite::AdminSource source, AsyncFetch* fetch) {
  admin_site_->PrintConfig(source, fetch, global_system_rewrite_options());
}

void SystemServerContext::MessageHistoryHandler(
    const RewriteOptions& options, AdminSite::AdminSource source,
    AsyncFetch* fetch) {
  admin_site_->MessageHistoryHandler(options, source, fetch);
}

void SystemServerContext::AdminPage(
    bool is_global, const GoogleUrl& stripped_gurl,
    const QueryParams& query_params,
    const RewriteOptions* options,
    AsyncFetch* fetch) {
  Statistics* stats = is_global ? factory()->statistics()
      : statistics();
  admin_site_->AdminPage(is_global, stripped_gurl, query_params, options,
                         cache_path(), fetch, system_caches_,
                         filesystem_metadata_cache(), http_cache(),
                         metadata_cache(), page_property_cache(), this,
                         statistics(), stats,  global_system_rewrite_options());
}

void SystemServerContext::StatisticsPage(bool is_global,
                                         const QueryParams& query_params,
                                         const RewriteOptions* options,
                                         AsyncFetch* fetch) {
  Statistics* stats = is_global ? factory()->statistics()
      : statistics();
  admin_site_->StatisticsPage(
      is_global, query_params, options, fetch,
      system_caches_, filesystem_metadata_cache(), http_cache(),
      metadata_cache(), page_property_cache(), this, statistics(), stats,
      global_system_rewrite_options());
}

}  // namespace net_instaweb
