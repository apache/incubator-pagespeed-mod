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


#include "pagespeed/kernel/cache/cache_stats.h"

#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/delegating_cache_callback.h"

namespace {

const char kGetCountHistogram[] = "_get_count";
const char kHitLatencyHistogram[] = "_hit_latency_us";
const char kInsertLatencyHistogram[] = "_insert_latency_us";
const char kInsertSizeHistogram[] = "_insert_size_bytes";
const char kLookupSizeHistogram[] = "_lookup_size_bytes";

const char kDeletes[] = "_deletes";
const char kHits[] = "_hits";
const char kInserts[] = "_inserts";
const char kMisses[] = "_misses";

// TODO(jmarantz): tie this to CacheBatcher::kDefaultMaxQueueSize,
// but for now I want to get discrete counts in each bucket.
const int kGetCountHistogramMaxValue = 500;
const int kSizeHistogramMaxValue = 5*1000*1000;
const int kLatencyHistogramMaxValueUs = 1*1000*1000;

}  // namespace

namespace net_instaweb {

CacheStats::CacheStats(StringPiece prefix,
                       CacheInterface* cache,
                       Timer* timer,
                       Statistics* statistics)
    : cache_(cache),
      timer_(timer),
      get_count_histogram_(statistics->GetHistogram(
          StrCat(prefix, kGetCountHistogram))),
      hit_latency_us_histogram_(statistics->GetHistogram(
          StrCat(prefix, kHitLatencyHistogram))),
      insert_latency_us_histogram_(statistics->GetHistogram(
          StrCat(prefix, kInsertLatencyHistogram))),
      insert_size_bytes_histogram_(statistics->GetHistogram(
          StrCat(prefix, kInsertSizeHistogram))),
      lookup_size_bytes_histogram_(statistics->GetHistogram(
          StrCat(prefix, kLookupSizeHistogram))),
      deletes_(statistics->GetVariable(StrCat(prefix, kDeletes))),
      hits_(statistics->GetVariable(StrCat(prefix, kHits))),
      inserts_(statistics->GetVariable(StrCat(prefix, kInserts))),
      misses_(statistics->GetVariable(StrCat(prefix, kMisses))),
      prefix_(prefix.data(), prefix.size()) {
  get_count_histogram_->SetMaxValue(kGetCountHistogramMaxValue);
  insert_size_bytes_histogram_->SetMaxValue(kSizeHistogramMaxValue);
  lookup_size_bytes_histogram_->SetMaxValue(kSizeHistogramMaxValue);
  hit_latency_us_histogram_->SetMaxValue(kLatencyHistogramMaxValueUs);
  insert_latency_us_histogram_->SetMaxValue(kLatencyHistogramMaxValueUs);
}

CacheStats::~CacheStats() {
}

GoogleString CacheStats::FormatName(StringPiece prefix, StringPiece cache) {
  return StrCat("Stats(prefix=", prefix, ",cache=", cache, ")");
}

void CacheStats::InitStats(StringPiece prefix, Statistics* statistics) {
  Histogram* get_count_histogram =
      statistics->AddHistogram(StrCat(prefix, kGetCountHistogram));
  get_count_histogram->SetMaxValue(kGetCountHistogramMaxValue);
  statistics->AddHistogram(StrCat(prefix, kHitLatencyHistogram));
  statistics->AddHistogram(StrCat(prefix, kInsertLatencyHistogram));
  Histogram* insert_size_bytes_histogram =
      statistics->AddHistogram(StrCat(prefix, kInsertSizeHistogram));
  insert_size_bytes_histogram->SetMaxValue(kSizeHistogramMaxValue);
  Histogram* lookup_size_bytes_histogram =
      statistics->AddHistogram(StrCat(prefix, kLookupSizeHistogram));
  lookup_size_bytes_histogram->SetMaxValue(kSizeHistogramMaxValue);
  statistics->AddVariable(StrCat(prefix, kDeletes));
  statistics->AddVariable(StrCat(prefix, kHits));
  statistics->AddVariable(StrCat(prefix, kInserts));
  statistics->AddVariable(StrCat(prefix, kMisses));
}

class CacheStats::StatsCallback : public DelegatingCacheCallback {
 public:
  StatsCallback(CacheStats* stats,
                Timer* timer,
                CacheInterface::Callback* callback)
      : DelegatingCacheCallback(callback),
        stats_(stats),
        timer_(timer) {
    start_time_us_ = timer->NowUs();
  }

  virtual ~StatsCallback() {
  }

  virtual void Done(CacheInterface::KeyState state) {
    if (state == CacheInterface::kAvailable) {
      int64 end_time_us = timer_->NowUs();
      stats_->hits_->Add(1);
      stats_->lookup_size_bytes_histogram_->Add(value().size());
      stats_->hit_latency_us_histogram_->Add(end_time_us - start_time_us_);
    } else {
      stats_->misses_->Add(1);
    }
    DelegatingCacheCallback::Done(state);
  }

 private:
  CacheStats* stats_;
  Timer* timer_;
  int64 start_time_us_;

  DISALLOW_COPY_AND_ASSIGN(StatsCallback);
};

void CacheStats::Get(const GoogleString& key, Callback* callback) {
  if (shutdown_.value()) {
    ValidateAndReportResult(key, CacheInterface::kNotFound, callback);
  } else {
    StatsCallback* cb = new StatsCallback(this, timer_, callback);
    cache_->Get(key, cb);
    get_count_histogram_->Add(1);
  }
}

void CacheStats::MultiGet(MultiGetRequest* request) {
  if (shutdown_.value()) {
    ReportMultiGetNotFound(request);
  } else {
    get_count_histogram_->Add(request->size());
    for (int i = 0, n = request->size(); i < n; ++i) {
      KeyCallback* key_callback = &(*request)[i];
      key_callback->callback = new StatsCallback(this, timer_,
                                                 key_callback->callback);
    }
    cache_->MultiGet(request);
  }
}

void CacheStats::Put(const GoogleString& key, const SharedString& value) {
  if (!shutdown_.value()) {
    int64 start_time_us = timer_->NowUs();
    inserts_->Add(1);
    insert_size_bytes_histogram_->Add(value.size());
    cache_->Put(key, value);
    insert_latency_us_histogram_->Add(timer_->NowUs() - start_time_us);
  }
}

void CacheStats::Delete(const GoogleString& key) {
  if (!shutdown_.value()) {
    deletes_->Add(1);
    cache_->Delete(key);
  }
}

}  // namespace net_instaweb
