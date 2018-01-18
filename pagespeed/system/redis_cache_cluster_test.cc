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


// Unit-test the redis interface in conjunction with Redis Cluster

#include "pagespeed/system/redis_cache.h"

#include <cstddef>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/cache_interface.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_test_base.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/util/simple_stats.h"
#include "pagespeed/system/redis_cache_cluster_setup.h"
#include "pagespeed/system/tcp_connection_for_testing.h"

namespace net_instaweb {

namespace {
  static const int kReconnectionDelayMs = 10;
  static const int kTimeoutUs = 100 * Timer::kMsUs;
  static const int kSlaveNodesFlushingTimeoutMs = 1000;
  static const int kDatabaseIndex = 0;

  // One can check following constants with CLUSTER KEYSLOT command.
  // For testing purposes, both KEY and {}KEY should be in the same slot range.
  // Implementation may or may not prepend {} to all keys processed to avoid
  // keys distribution due to hash tags. We want tests to work in both
  // situations. See http://redis.io/topics/cluster-spec#keys-hash-tags.
  //
  // TODO(yeputons): add static assertion that these keys really belong to
  // corresponding slots.
  static const char kKeyOnNode1[] = "Foobar";        // Slots 0-5499
  static const char kKeyOnNode1b[] = "Coolkey";      // Slots 0-5499
  static const char kKeyOnNode2[] = "SomeOtherKey";  // Slots 5500-10999
  static const char kKeyOnNode3[] = "Key";           // Slots 11000-16383
  static const char kValue1[] = "Value1";
  static const char kValue2[] = "Value2";
  static const char kValue3[] = "Value3";
  static const char kValue4[] = "Value4";
}  // namespace

typedef std::vector<std::unique_ptr<TcpConnectionForTesting>> ConnectionList;

class RedisCacheClusterTest : public CacheTestBase {
 protected:
  RedisCacheClusterTest()
      : thread_system_(Platform::CreateThreadSystem()),
        statistics_(thread_system_.get()),
        timer_(new NullMutex, 0) {
    RedisCache::InitStats(&statistics_);
  }

  // run_program_with_redis_cluster.sh should take care of this for us, but
  // leaving this here to make the test as hermetic as possible.
  static void SetUpTestCase() {
    StringVector node_ids;
    std::vector<int> ports;
    ConnectionList connections;

    if (RedisCluster::LoadConfiguration(&node_ids, &ports, &connections)) {
      RedisCluster::ResetConfiguration(&node_ids, &ports, &connections);
    }
  }

  void TearDown() override {
    RedisCluster::FlushAll(&connections_);
  }

  bool InitRedisClusterOrSkip() {
    if (!RedisCluster::LoadConfiguration(&node_ids_, &ports_, &connections_)) {
      return false;  // Already logged an error.
    }

    // Setting up cache.
    cache_.reset(new RedisCache("localhost", ports_[0], thread_system_.get(),
                                &handler_, &timer_, kReconnectionDelayMs,
                                kTimeoutUs, &statistics_, kDatabaseIndex));
    cache_->StartUp();
    return true;
  }

  CacheInterface* Cache() override { return cache_.get(); }

  scoped_ptr<RedisCache> cache_;
  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats statistics_;
  MockTimer timer_;
  GoogleMessageHandler handler_;

  StringVector node_ids_;
  std::vector<int> ports_;
  ConnectionList connections_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RedisCacheClusterTest);
};

TEST_F(RedisCacheClusterTest, HashSlot) {
  // Expected crc16 hashes taken from running RedisClusterCRC16.crc16 from
  // https://github.com/antirez/redis-rb-cluster/blob/master/crc16.rb

  EXPECT_EQ(15332, RedisCache::HashSlot("hello world"));

  // If there's curly brace section, only that section is considered for the
  // key.
  EXPECT_EQ(7855, RedisCache::HashSlot("curly"));
  EXPECT_EQ(7855, RedisCache::HashSlot("hello {curly} world"));
  // Only take the first such section.
  EXPECT_EQ(7855, RedisCache::HashSlot("hello {curly} world {ignored}"));
  // Any other junk doesn't matter.
  EXPECT_EQ(7855, RedisCache::HashSlot(
      "hello {curly} world {nothing here matters"));
  EXPECT_EQ(7855, RedisCache::HashSlot(
      "}}} hello {curly} world {nothing else matters"));
  // Incomplete curlies are ignored.
  EXPECT_EQ(8673, RedisCache::HashSlot("hello {curly world"));
  EXPECT_EQ(950, RedisCache::HashSlot("hello }curly{ world"));
  EXPECT_EQ(3940, RedisCache::HashSlot("hello curly world{"));
  // Empty string is fine.
  EXPECT_EQ(0, RedisCache::HashSlot(""));
  // While {a} means to only consider a, {} means consider the whole message
  // when hashing.  (Otherwise this would return 0, the hash of "".)
  EXPECT_EQ(13934, RedisCache::HashSlot("hello {} world"));
  // After an empty curly, all other curlies are still ignored.  (Otherwise
  // this would return 7855.)
  EXPECT_EQ(2795, RedisCache::HashSlot("{}hello {curly} world"));
}

TEST_F(RedisCacheClusterTest, FirstNodePutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode1, kValue1);
  CheckGet(kKeyOnNode1, kValue1);

  CheckDelete(kKeyOnNode1);
  CheckNotFound(kKeyOnNode1);

  // All requests are for node1, which is the main node, so we should never be
  // redirected or have to fetch slots.
  EXPECT_EQ(0, cache_->Redirections());
  EXPECT_EQ(0, cache_->ClusterSlotsFetches());
}

TEST_F(RedisCacheClusterTest, OtherNodesPutGetDelete) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  CheckPut(kKeyOnNode2, kValue1);
  // This should have redirected us from node1 to node2, and prompted us to
  // update our cluster map.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode3, kValue2);

  CheckGet(kKeyOnNode2, kValue1);
  CheckGet(kKeyOnNode3, kValue2);

  CheckDelete(kKeyOnNode2);
  CheckDelete(kKeyOnNode3);

  CheckNotFound(kKeyOnNode2);
  CheckNotFound(kKeyOnNode3);

  // No more redirections or slots fetches triggered after the first one above.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());
}

TEST_F(RedisCacheClusterTest, SlotBoundaries) {
  // These are designed to exercise the slot lookup code at slot boundaries.
  // 0 and 16384 are min/max slot. Slot 10999 is on node 2 and 11000 is on node
  // 3.
  const char kHashesTo0[] = "";
  const char kHashesTo10999[] = "AFKb";
  const char kHashesTo11000[] = "PNP";
  const char kHashesTo16383[] = "C0p";

  if (!InitRedisClusterOrSkip()) {
    return;
  }

  ASSERT_EQ(0, RedisCache::HashSlot(kHashesTo0));
  ASSERT_EQ(10999, RedisCache::HashSlot(kHashesTo10999));
  ASSERT_EQ(11000, RedisCache::HashSlot(kHashesTo11000));
  ASSERT_EQ(16383, RedisCache::HashSlot(kHashesTo16383));

  // Do one lookup with a redirection, to prime the table.
  CheckPut(kKeyOnNode2, kValue1);
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  for (const GoogleString& key :
       {kHashesTo0, kHashesTo10999, kHashesTo11000, kHashesTo16383}) {
    CheckPut(key, key);
    CheckGet(key, key);

    // If our cluster lookup code is correct, there shouldn't be any
    // redirections.
    EXPECT_EQ(1, cache_->Redirections()) << " for key " << key;
    EXPECT_EQ(1, cache_->ClusterSlotsFetches()) << " for key " << key;
  }
}

int CountSubstring(const GoogleString& haystack, const GoogleString& needle) {
  size_t pos = -1;
  int count = 0;
  while (true) {
    pos = haystack.find(needle, pos+1);
    if (pos == haystack.npos) {
      return count;
    }
    count++;
  }
}

TEST_F(RedisCacheClusterTest, GetStatus) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  // We're only connected to the main node right now.
  GoogleString status;
  cache_->GetStatus(&status);
  EXPECT_EQ(1, CountSubstring(status, "redis_version:"));
  EXPECT_EQ(1, CountSubstring(status, "connected_clients:"));

  CheckPut(kKeyOnNode1, kValue1);

  // Still only on the main node.
  status.clear();
  cache_->GetStatus(&status);
  EXPECT_EQ(1, CountSubstring(status, "redis_version:"));
  EXPECT_EQ(1, CountSubstring(status, "connected_clients:"));

  CheckPut(kKeyOnNode2, kValue2);
  CheckPut(kKeyOnNode3, kValue1);

  // Now we're connected to all the nodes.
  status.clear();
  cache_->GetStatus(&status);
  LOG(INFO) << status;
  // Either three or four is ok here, because the connections map isn't fully
  // deduplicated.  Specifically, when we originally connect to redis we do it
  // by some name (host:port) and then when we learn about other nodes they
  // have other names (ip1:port1, ip2:port2, ...)  We can often learn about the
  // original node by whatever IP redis uses for it instead of the hostname or
  // IP we originally used for it, in which case we'll get a single duplicate
  // connection.  It would be possible to fix this by paying attention to node
  // ids, which newer versions of redis cluster give you, but it would be kind
  // of a pain just to reduce our connection count by 1.
  EXPECT_LE(3, CountSubstring(status, "redis_version:"));
  EXPECT_GE(4, CountSubstring(status, "redis_version:"));
  EXPECT_LE(3, CountSubstring(status, "connected_clients:"));
  EXPECT_GE(4, CountSubstring(status, "connected_clients:"));
}

class RedisCacheClusterTestWithReconfiguration : public RedisCacheClusterTest {
 protected:
  void TearDown() override {
    if (!connections_.empty()) {
      RedisCluster::ResetConfiguration(&node_ids_, &ports_, &connections_);
    }
  }
};

TEST_F(RedisCacheClusterTestWithReconfiguration, HandlesMigrations) {
  if (!InitRedisClusterOrSkip()) {
    return;
  }

  LOG(INFO) << "Putting value on the first node";
  CheckPut(kKeyOnNode1, kValue1);
  CheckPut(kKeyOnNode1b, kValue2);
  CheckGet(kKeyOnNode1, kValue1);
  CheckGet(kKeyOnNode1b, kValue2);

  // No redirections or slot fetches needed.
  EXPECT_EQ(0, cache_->Redirections());
  EXPECT_EQ(0, cache_->ClusterSlotsFetches());

  // Now trigger a redirection and slot fetch.
  CheckPut(kKeyOnNode3, kValue3);
  CheckGet(kKeyOnNode3, kValue3);
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  LOG(INFO) << "Starting migration of the first node";
  for (int i = 0; i < 5000; i++) {
    connections_[1]->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i),
                                 " IMPORTING ", node_ids_[0], "\r\n"));
  }
  for (int i = 0; i < 5000; i++) {
    CHECK_EQ("+OK\r\n", connections_[1]->ReadLineCrLf());
  }
  for (int i = 0; i < 5000; i++) {
    connections_[0]->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i),
                                 " MIGRATING ", node_ids_[1], "\r\n"));
  }
  for (int i = 0; i < 5000; i++) {
    CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());
  }

  LOG(INFO) << "Checking availability before actually moving the key";
  // The key should still be available on the first node, where it was.
  CheckGet(kKeyOnNode1, kValue1);
  CheckPut(kKeyOnNode1, kValue2);
  CheckGet(kKeyOnNode1, kValue2);

  // No additional redirects or slot fetches.
  EXPECT_EQ(1, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  connections_[0]->Send(StrCat("MIGRATE 127.0.0.1 ", IntegerToString(ports_[1]),
                               " ", kKeyOnNode1, " 0 5000\r\n"));
  CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());

  LOG(INFO) << "Checking availability after actually moving the key";
  // This is ugly: because we moved the key and now it's not where it should be
  // for the slot it's in, we see redirections with ASK on every
  // interaction. They're ASKs, though, so they're just temporary and we
  // shouldn't reload mappings.
  CheckGet(kKeyOnNode1, kValue2);
  EXPECT_EQ(2, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode1, kValue3);
  EXPECT_EQ(3, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  CheckGet(kKeyOnNode1, kValue3);
  EXPECT_EQ(4, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  // But not for the second key, which is still on the first node.
  CheckGet(kKeyOnNode1b, kValue2);
  CheckPut(kKeyOnNode1b, kValue3);
  CheckGet(kKeyOnNode1b, kValue3);
  EXPECT_EQ(4, cache_->Redirections());
  EXPECT_EQ(1, cache_->ClusterSlotsFetches());

  LOG(INFO) << "Moving the second key as well";
  connections_[0]->Send(StrCat("MIGRATE 127.0.0.1 ", IntegerToString(ports_[1]),
                               " ", kKeyOnNode1b, " 0 5000\r\n"));
  CHECK_EQ("+OK\r\n", connections_[0]->ReadLineCrLf());

  LOG(INFO) << "Ending migration";
  for (int c = 0; c < 3; c++) {
    auto &conn = connections_[c];
    for (int i = 0; i < 5000; i++) {
      conn->Send(StrCat("CLUSTER SETSLOT ", IntegerToString(i), " NODE ",
                        node_ids_[1], "\r\n"));
    }
    for (int i = 0; i < 5000; i++) {
      CHECK_EQ("+OK\r\n", conn->ReadLineCrLf());
    }
  }

  LOG(INFO) << "Checking availability after migration";
  CheckGet(kKeyOnNode1, kValue3);
  // Now that the migration is complete and we've called SETSLOT we'll get a
  // MOVED instead of an ASK, so we'll fetch slots.
  EXPECT_EQ(5, cache_->Redirections());
  EXPECT_EQ(2, cache_->ClusterSlotsFetches());

  CheckPut(kKeyOnNode1, kValue4);
  CheckGet(kKeyOnNode1, kValue4);

  CheckGet(kKeyOnNode1b, kValue3);
  CheckPut(kKeyOnNode1b, kValue4);
  CheckGet(kKeyOnNode1b, kValue4);

  // No more redirections or slots fetches.
  EXPECT_EQ(5, cache_->Redirections());
  EXPECT_EQ(2, cache_->ClusterSlotsFetches());
}

}  // namespace net_instaweb
