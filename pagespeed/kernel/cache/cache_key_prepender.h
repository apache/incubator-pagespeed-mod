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


#ifndef PAGESPEED_KERNEL_CACHE_CACHE_KEY_PREPENDER_H_
#define PAGESPEED_KERNEL_CACHE_CACHE_KEY_PREPENDER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

// Implements a cache adapter that prepends a fixed string to all keys that are
// used in the cache. Can be used for isolating unit tests of external caches
// (e.g. memcached).
class CacheKeyPrepender : public CacheInterface {
 public:
  // Does not takes ownership of the cache
  CacheKeyPrepender(StringPiece prefix, CacheInterface* cache);
  ~CacheKeyPrepender() override {}

  // Implementation of CacheInterface
  void Get(const GoogleString& key, Callback* callback) override;
  void MultiGet(MultiGetRequest* request) override;
  void Put(const GoogleString& key, const SharedString& value) override;
  void Delete(const GoogleString& key) override;
  CacheInterface* Backend() override { return cache_; }
  bool IsBlocking() const override { return cache_->IsBlocking(); }

  // Implementation of CacheInterface
  bool IsHealthy() const override { return cache_->IsHealthy(); }
  void ShutDown() override { cache_->ShutDown(); }
  GoogleString Name() const override {
    return FormatName(prefix_.Value(), cache_->Name());
  }

  static GoogleString FormatName(StringPiece prefix, StringPiece cache);

 private:
  class KeyPrependerCallback;

  CacheInterface* cache_;
  SharedString prefix_;  // copy of prefix to prepend, shared with callbacks

  GoogleString AddPrefix(const GoogleString &key);

  DISALLOW_COPY_AND_ASSIGN(CacheKeyPrepender);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_CACHE_KEY_PREPENDER_H_
