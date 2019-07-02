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


// Unit-test the write-through cache

#include "pagespeed/kernel/cache/write_through_cache.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/cache/lru_cache.h"

namespace net_instaweb {

class WriteThroughCacheTest : public CacheTestBase {
 protected:
  WriteThroughCacheTest()
      : small_cache_(15),
        big_cache_(1000),
        write_through_cache_(&small_cache_, &big_cache_) {
  }

  LRUCache small_cache_;
  LRUCache big_cache_;
  WriteThroughCache write_through_cache_;

  virtual CacheInterface* Cache() { return &write_through_cache_; }
  virtual void PostOpCleanup() {
    small_cache_.SanityCheck();
    big_cache_.SanityCheck();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WriteThroughCacheTest);
};

// Simple flow of putting in an item, getting it, deleting it.
TEST_F(WriteThroughCacheTest, PutGetDelete) {
  // First, put some small data into the write-through.  It should
  // be available in both caches.
  CheckPut("Name", "Value");
  CheckGet(&write_through_cache_, "Name", "Value");
  CheckGet(&small_cache_, "Name", "Value");
  CheckGet(&big_cache_, "Name", "Value");

  CheckNotFound(&write_through_cache_, "Another Name");

  // next, put another value in.  This will evict the first item
  // out of the small cache,
  CheckPut("Name2", "NewValue");
  CheckGet(&write_through_cache_, "Name2", "NewValue");
  CheckGet(&small_cache_, "Name2", "NewValue");
  CheckGet(&big_cache_, "Name2", "NewValue");

  // The first item will still be available in the write-through,
  // and in the big-cache, but will have been evicted from the
  // small cache.
  CheckNotFound(&small_cache_, "Name");
  CheckGet(&big_cache_, "Name", "Value");
  CheckNotFound(&small_cache_, "Name");

  CheckGet(&write_through_cache_, "Name", "Value");

  // But now, once we've gotten in out of the write-through cache,
  // the small cache will have the value "freshened."
  CheckGet(&small_cache_, "Name", "Value");

  write_through_cache_.Delete("Name2");
  CheckNotFound(&write_through_cache_, "Name2");
  CheckNotFound(&small_cache_, "Name2");
  CheckNotFound(&big_cache_, "Name2");
}

// Check size-limits for the small cache
TEST_F(WriteThroughCacheTest, SizeLimit) {
  write_through_cache_.set_cache1_limit(10);

  // This one will fit.
  CheckPut("Name", "Value");
  CheckGet(&write_through_cache_, "Name", "Value");
  CheckGet(&small_cache_, "Name", "Value");
  CheckGet(&big_cache_, "Name", "Value");

  // This one will not.
  CheckPut("Name2", "TooBig");
  CheckGet(&write_through_cache_, "Name2", "TooBig");
  CheckNotFound(&small_cache_, "Name2");
  CheckGet(&big_cache_, "Name2", "TooBig");

  // However "Name" is still in both caches.
  CheckGet(&small_cache_, "Name", "Value");
  CheckGet(&write_through_cache_, "Name", "Value");
  CheckGet(&big_cache_, "Name", "Value");
}

TEST_F(WriteThroughCacheTest, FindShadowedValid) {
  // Make sure we find a valid value in L2 if it's shadowed by an invalid one
  // in L1.
  CheckPut(&small_cache_, "Name", "invalid");
  CheckPut(&big_cache_, "Name", "valid");
  set_invalid_value("invalid");
  CheckNotFound(&small_cache_, "Name");
  CheckGet(&big_cache_, "Name", "valid");
  CheckGet(&write_through_cache_, "Name", "valid");
  // Make sure we fixed up the small_cache_
  CheckGet(&small_cache_, "Name", "valid");
}

}  // namespace net_instaweb
