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


#include "net/instaweb/http/public/http_dump_url_async_writer.h"

#include "net/instaweb/http/public/counting_url_async_fetcher.h"
#include "net/instaweb/http/public/fetcher_test.h"
#include "net/instaweb/http/public/wait_url_async_fetcher.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

// TODO(sligocki): Merge with CacheUrlAsyncFetcherTest and refactor into
// FetcherTestBase.
class HttpDumpUrlAsyncWriterTest : public FetcherTest {
 protected:
  HttpDumpUrlAsyncWriterTest()
      : root_dir_(GTestTempDir() + "/http_dump_url_async_writer_test/"),
        mock_timer_(new NullMutex(), 0),
        file_system_(thread_system_.get(), &mock_timer_),
        dump_fetcher_(root_dir_, counting_fetcher(), &file_system_,
                      &mock_timer_) {
  }

  UrlAsyncFetcher* async_fetcher() { return &dump_fetcher_; }

  GoogleString root_dir_;
  MockTimer mock_timer_;
  MemFileSystem file_system_;
  HttpDumpUrlAsyncWriter dump_fetcher_;
};

TEST_F(HttpDumpUrlAsyncWriterTest, TestCacheable) {
  // With the async cached fetching interface, we will expect even the
  // initial request to succeed, once the callbacks are run.
  bool callback_called1, callback_called2, callback_called3;
  EXPECT_EQ(1, CountFetchesAsync(kGoodUrl, true, &callback_called1));
  EXPECT_FALSE(callback_called1);

  EXPECT_EQ(1, CountFetchesAsync(kGoodUrl, true, &callback_called2));
  EXPECT_FALSE(callback_called1);
  EXPECT_FALSE(callback_called2);

  wait_fetcher()->CallCallbacks();
  EXPECT_TRUE(callback_called1);
  EXPECT_TRUE(callback_called2);

  EXPECT_EQ(0, CountFetchesAsync(kGoodUrl, true, &callback_called3));
  // No async fetcher callbacks were queued because the content
  // was cached, so no need to call CallCallbacks() again here.
  EXPECT_TRUE(callback_called3);
}

TEST_F(HttpDumpUrlAsyncWriterTest, TestNotCacheable) {
  // With the async cached fetching interface, we will expect even the
  // initial request to succeed, once the callbacks are run.
  bool callback_called1, callback_called2, callback_called3;
  EXPECT_EQ(1, CountFetchesAsync(kNotCachedUrl, true, &callback_called1));
  EXPECT_FALSE(callback_called1);

  EXPECT_EQ(1, CountFetchesAsync(kNotCachedUrl, true, &callback_called2));
  EXPECT_FALSE(callback_called1);
  EXPECT_FALSE(callback_called2);

  wait_fetcher()->CallCallbacks();
  EXPECT_TRUE(callback_called1);
  EXPECT_TRUE(callback_called2);

  // This is not a proper cache and does not distinguish between cacheable
  // or non-cacheable URLs.
  EXPECT_EQ(0, CountFetchesAsync(kNotCachedUrl, true, &callback_called3));
  EXPECT_TRUE(callback_called3);
}

TEST_F(HttpDumpUrlAsyncWriterTest, TestCacheWithASyncFetcherFail) {
  bool callback_called1, callback_called2, callback_called3;

  EXPECT_EQ(1, CountFetchesAsync(kBadUrl, false, &callback_called1));
  EXPECT_FALSE(callback_called1);

  EXPECT_EQ(1, CountFetchesAsync(kBadUrl, false, &callback_called2));
  EXPECT_FALSE(callback_called1);
  EXPECT_FALSE(callback_called2);

  wait_fetcher()->CallCallbacks();
  EXPECT_TRUE(callback_called1);
  EXPECT_TRUE(callback_called2);

  EXPECT_EQ(1, CountFetchesAsync(kBadUrl, false, &callback_called3));
  EXPECT_FALSE(callback_called3);
  wait_fetcher()->CallCallbacks();  // Otherwise memory will be leaked
  EXPECT_TRUE(callback_called3);
}

}  // namespace net_instaweb
