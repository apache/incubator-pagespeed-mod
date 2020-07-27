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

#ifndef PAGESPEED_KERNEL_CACHE_CACHE_STATS_H_
#define PAGESPEED_KERNEL_CACHE_CACHE_STATS_H_

#include "pagespeed/kernel/base/atomic_bool.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

class Histogram;
class Statistics;
class Timer;
class Variable;

// Wrapper around a CacheInterface that adds statistics and histograms
// for hit-rate, latency, etc.  As there can be multiple caches in a
// system (l1, l2, etc), the constructor takes a string prefix so they
// can be measured independently.
class CacheStats : public CacheInterface {
 public:
  // Doees not takes ownership of the cache, timer, or statistics.
  CacheStats(StringPiece prefix, CacheInterface* cache, Timer* timer,
             Statistics* statistics);
  ~CacheStats() override;

  // This must be called once for every unique cache prefix.
  static void InitStats(StringPiece prefix, Statistics* statistics);

  void Get(const GoogleString& key, Callback* callback) override;
  void MultiGet(MultiGetRequest* request) override;
  void Put(const GoogleString& key, const SharedString& value) override;
  void Delete(const GoogleString& key) override;
  CacheInterface* Backend() override { return cache_; }
  bool IsBlocking() const override { return cache_->IsBlocking(); }

  bool IsHealthy() const override {
    return !shutdown_.value() && cache_->IsHealthy();
  }

  void ShutDown() override {
    shutdown_.set_value(true);
    cache_->ShutDown();
  }

  GoogleString Name() const override {
    return FormatName(prefix_, cache_->Name());
  }
  static GoogleString FormatName(StringPiece prefix, StringPiece cache);

 private:
  class StatsCallback;
  friend class StatsCallback;

  CacheInterface* cache_;
  Timer* timer_;
  Histogram* get_count_histogram_;
  Histogram* hit_latency_us_histogram_;
  Histogram* insert_latency_us_histogram_;
  Histogram* insert_size_bytes_histogram_;
  Histogram* lookup_size_bytes_histogram_;
  Variable* deletes_;
  Variable* hits_;
  Variable* inserts_;
  Variable* misses_;
  GoogleString prefix_;
  AtomicBool shutdown_;

  DISALLOW_COPY_AND_ASSIGN(CacheStats);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_CACHE_STATS_H_
