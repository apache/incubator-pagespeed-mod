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
// Contains MockTimeCache, which lets one inject Scheduler-simulated
// delays before callback invocations of a cache object.
//
// Note: DelayCache also supports delayed callbacks, but each key's
// delivery is controlled by API.
//
// TODO(jmarantz): consider refactoring this as a subclass of DelayCache.

#ifndef PAGESPEED_KERNEL_CACHE_MOCK_TIME_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_MOCK_TIME_CACHE_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

class Scheduler;

// See file comment
class MockTimeCache : public CacheInterface {
 public:
  // Note: takes ownership of nothing.
  MockTimeCache(Scheduler* scheduler, CacheInterface* cache);
  ~MockTimeCache() override;

  // Reimplementations of CacheInterface methods.
  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, const SharedString& value) override;
  void Delete(const GoogleString& key) override;

  // Sets the delay the cache will inject before invoking the callbacks.
  // Note that this only affects the 'Done' callback; 'ValidateCandidate'
  // happens immediately.
  void set_delay_us(int64 delay_us) { delay_us_ = delay_us; }
  int64 delay_us() const { return delay_us_; }

  Scheduler* scheduler() { return scheduler_; }

  GoogleString Name() const override { return FormatName(cache_->Name()); }
  static GoogleString FormatName(StringPiece name);
  bool IsBlocking() const override { return cache_->IsBlocking(); }
  bool IsHealthy() const override { return cache_->IsHealthy(); }
  void ShutDown() override { cache_->ShutDown(); }

 private:
  class DelayCallback;

  Scheduler* scheduler_;
  CacheInterface* cache_;
  int64 delay_us_;
  GoogleString name_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_MOCK_TIME_CACHE_H_
