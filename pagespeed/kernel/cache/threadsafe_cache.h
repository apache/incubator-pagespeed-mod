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

#ifndef PAGESPEED_KERNEL_CACHE_THREADSAFE_CACHE_H_
#define PAGESPEED_KERNEL_CACHE_THREADSAFE_CACHE_H_

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

// Composes a cache with a Mutex to form a threadsafe cache.  Note
// that cache callbacks will be run in a thread that is dependent
// on the cache implementation.  This wrapper class just guarantees
// the thread safety of the cache itself, not the callbacks.
//
// WARNING: THIS IMPLEMENTATION HOLDS A LOCK ACROSS THE VALIDATOR.
// TODO(jmarantz): redesign the interface between this class and
// the cache it protects.
class ThreadsafeCache : public CacheInterface {
 public:
  // Does not takes ownership of cache.  Takes ownership of mutex.
  ThreadsafeCache(CacheInterface* cache, AbstractMutex* mutex)
      : cache_(cache), mutex_(mutex) {}
  ~ThreadsafeCache() override;

  void Get(const GoogleString& key, Callback* callback) override;
  void Put(const GoogleString& key, const SharedString& value)
      LOCKS_EXCLUDED(mutex_) override;
  void Delete(const GoogleString& key) LOCKS_EXCLUDED(mutex_) override;
  CacheInterface* Backend() override { return cache_; }
  bool IsBlocking() const override { return cache_->IsBlocking(); }
  bool IsHealthy() const LOCKS_EXCLUDED(mutex_) override;
  void ShutDown() LOCKS_EXCLUDED(mutex_) override;

  static GoogleString FormatName(StringPiece cache);
  GoogleString Name() const override { return FormatName(cache_->Name()); }

 private:
  CacheInterface* cache_;
  std::unique_ptr<AbstractMutex> mutex_;

  DISALLOW_COPY_AND_ASSIGN(ThreadsafeCache);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_CACHE_THREADSAFE_CACHE_H_
