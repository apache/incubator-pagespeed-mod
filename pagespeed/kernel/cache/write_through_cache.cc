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


#include "pagespeed/kernel/cache/write_through_cache.h"

#include <cstddef>

#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

const size_t WriteThroughCache::kUnlimited = static_cast<size_t>(-1);

WriteThroughCache::~WriteThroughCache() {
}

void WriteThroughCache::PutInCache1(const GoogleString& key,
                                    const SharedString& value) {
  if ((cache1_size_limit_ == kUnlimited) ||
      (key.size() + value.size() < cache1_size_limit_)) {
    cache1_->Put(key, value);
  }
}

class WriteThroughCallback : public CacheInterface::Callback {
 public:
  WriteThroughCallback(WriteThroughCache* wtc,
                       const GoogleString& key,
                       CacheInterface::Callback* callback)
      : write_through_cache_(wtc),
        key_(key),
        callback_(callback),
        trying_cache2_(false) {
  }

  virtual bool ValidateCandidate(const GoogleString& key,
                                 CacheInterface::KeyState state) {
    callback_->set_value(value());
    return callback_->DelegatedValidateCandidate(key, state);
  }

  virtual void Done(CacheInterface::KeyState state) {
    if (state == CacheInterface::kAvailable) {
      if (trying_cache2_) {
        write_through_cache_->PutInCache1(key_, value());
      }
      callback_->DelegatedDone(state);
      delete this;
    } else if (trying_cache2_) {
      callback_->DelegatedDone(state);
      delete this;
    } else {
      trying_cache2_ = true;
      write_through_cache_->cache2()->Get(key_, this);
    }
  }


  WriteThroughCache* write_through_cache_;
  GoogleString key_;
  CacheInterface::Callback* callback_;
  bool trying_cache2_;
};

GoogleString WriteThroughCache::FormatName(StringPiece cache1,
                                           StringPiece cache2) {
  return StrCat("WriteThroughCache(l1=", cache1, ",l2=", cache2, ")");
}

void WriteThroughCache::Get(const GoogleString& key, Callback* callback) {
  cache1_->Get(key, new WriteThroughCallback(this, key, callback));
}

void WriteThroughCache::Put(const GoogleString& key,
                            const SharedString& value) {
  PutInCache1(key, value);
  cache2_->Put(key, value);
}

void WriteThroughCache::Delete(const GoogleString& key) {
  cache1_->Delete(key);
  cache2_->Delete(key);
}

}  // namespace net_instaweb
