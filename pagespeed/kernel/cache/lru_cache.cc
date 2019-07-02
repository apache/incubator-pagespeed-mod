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


#include "pagespeed/kernel/cache/lru_cache.h"

#include <cstddef>
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

LRUCache::LRUCache(size_t max_size)
    : base_(max_size, &value_helper_),
      is_healthy_(true) {
  ClearStats();
}

LRUCache::~LRUCache() {
  Clear();
}

void LRUCache::Get(const GoogleString& key, Callback* callback) {
  if (!is_healthy_) {
    ValidateAndReportResult(key, kNotFound, callback);
    return;
  }
  KeyState key_state = kNotFound;
  SharedString* value = base_.GetFreshen(key);
  if (value != NULL) {
    key_state = kAvailable;
    callback->set_value(*value);
  }
  ValidateAndReportResult(key, key_state, callback);
}

void LRUCache::Put(const GoogleString& key, const SharedString& new_value) {
  if (!is_healthy_) {
    return;
  }

  base_.Put(key, new_value);
}

void LRUCache::Delete(const GoogleString& key) {
  if (!is_healthy_) {
    return;
  }

  base_.Delete(key);
}

void LRUCache::DeleteWithPrefixForTesting(StringPiece prefix) {
  if (!is_healthy_) {
    return;
  }

  base_.DeleteWithPrefixForTesting(prefix);
}

}  // namespace net_instaweb
