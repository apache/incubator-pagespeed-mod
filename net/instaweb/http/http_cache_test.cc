/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

// Unit-test the lru cache

#include "net/instaweb/http/public/http_cache.h"

#include <cstddef>                     // for size_t

#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/google_message_handler.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/mock_hasher.h"
#include "net/instaweb/util/public/mock_timer.h"
#include "net/instaweb/util/public/platform.h"
#include "net/instaweb/util/public/scoped_ptr.h"
#include "net/instaweb/util/public/simple_stats.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"
#include "net/instaweb/util/public/thread_system.h"
#include "net/instaweb/util/public/timer.h"

namespace {
// Set the cache size large enough so nothing gets evicted during this test.
const int kMaxSize = 10000;
const char kStartDate[] = "Sun, 16 Dec 1979 02:27:45 GMT";
const char kUrl[] = "http://www.test.com/";
const char kUrl2[] = "http://www.test.com/2";
const char kUrl3[] = "http://www.test.com/3";
const char kHttpsUrl[] = "https://www.test.com/";
const char kFragment[] = "www.test.com";
const char kFragment2[] = "www.other.com";
}  // namespace

namespace net_instaweb {

class HTTPCacheTest : public testing::Test {
 protected:
  // Helper class for calling Get and Query methods on cache implementations
  // that are blocking in nature (e.g. in-memory LRU or blocking file-system).
  class Callback : public HTTPCache::Callback {
   public:
    explicit Callback(const RequestContextPtr& ctx) : HTTPCache::Callback(ctx) {
      called_ = false;
      result_ = HTTPCache::kNotFound;
      cache_valid_ = true;
      fresh_ = true;
      override_cache_ttl_ms_= -1;
      http_value()->Clear();
      fallback_http_value()->Clear();
    }
    virtual void Done(HTTPCache::FindResult result) {
      called_ = true;
      result_ = result;
    }
    virtual bool IsCacheValid(const GoogleString& key,
                              const ResponseHeaders& headers) {
      // For unit testing, we are simply stubbing IsCacheValid.
      return cache_valid_;
    }
    virtual bool IsFresh(const ResponseHeaders& headers) {
      // For unit testing, we are simply stubbing IsFresh.
      return fresh_;
    }
    virtual int64 OverrideCacheTtlMs(const GoogleString& key) {
      return override_cache_ttl_ms_;
    }

    // Detailed Vary handling is tested in ResponseHeadersTest.
    virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
      return ResponseHeaders::kRespectVaryOnResources;
    }

    bool called_;
    HTTPCache::FindResult result_;
    bool cache_valid_;
    bool fresh_;
    int64 override_cache_ttl_ms_;
  };

  static int64 ParseDate(const char* start_date) {
    int64 time_ms;
    ResponseHeaders::ParseTime(start_date, &time_ms);
    return time_ms;
  }

  HTTPCacheTest()
      : thread_system_(Platform::CreateThreadSystem()),
        simple_stats_(thread_system_.get()),
        mock_timer_(thread_system_->NewMutex(), ParseDate(kStartDate)),
        lru_cache_(kMaxSize) {
    HTTPCache::InitStats(&simple_stats_);
    http_cache_.reset(new HTTPCache(&lru_cache_, &mock_timer_, &mock_hasher_,
                                    &simple_stats_));
  }

  void InitHeaders(ResponseHeaders* headers, const char* cache_control) {
    headers->Add("name", "value");
    headers->Add("Date", kStartDate);
    if (cache_control != NULL) {
      headers->Add("Cache-control", cache_control);
    }
    headers->SetStatusAndReason(HttpStatus::kOK);
    headers->ComputeCaching();
  }

  int GetStat(const char* name) { return simple_stats_.LookupValue(name); }

  HTTPCache::FindResult FindWithCallback(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers, MessageHandler* handler, Callback* callback) {
    http_cache_->Find(key, fragment, handler, callback);
    EXPECT_TRUE(callback->called_);
    if (callback->result_ == HTTPCache::kFound) {
      value->Link(callback->http_value());
    }
    headers->CopyFrom(*callback->response_headers());
    return callback->result_;
  }

  HTTPCache::FindResult Find(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers, MessageHandler* handler) {
    scoped_ptr<Callback> callback(NewCallback());
    return FindWithCallback(
        key, fragment, value, headers, handler, callback.get());
  }

  HTTPCache::FindResult Find(
      const GoogleString& key, const GoogleString& fragment, HTTPValue* value,
      ResponseHeaders* headers, MessageHandler* handler, bool cache_valid) {
    scoped_ptr<Callback> callback(NewCallback());
    callback->cache_valid_ = cache_valid;
    return FindWithCallback(
        key, fragment, value, headers, handler, callback.get());
  }

  Callback* NewCallback() {
    return new Callback(RequestContext::NewTestRequestContext(
        thread_system_.get()));
  }

  void Put(const GoogleString& key, const GoogleString& fragment,
           ResponseHeaders* headers, const StringPiece& content,
           MessageHandler* handler) {
    http_cache_->Put(key, fragment, RequestHeaders::Properties(),
                     ResponseHeaders::kRespectVaryOnResources,
                     headers, content, handler);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  SimpleStats simple_stats_;
  MockTimer mock_timer_;
  MockHasher mock_hasher_;
  LRUCache lru_cache_;
  scoped_ptr<HTTPCache> http_cache_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HTTPCacheTest);
};

// Simple flow of putting in an item, getting it.
TEST_F(HTTPCacheTest, PutGet) {
  simple_stats_.Clear();

  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));

  simple_stats_.Clear();
  scoped_ptr<Callback> callback(NewCallback());
  // Now advance time 301 seconds and the we should no longer
  // be able to fetch this resource out of the cache.
  mock_timer_.AdvanceMs(301 * 1000);
  found = FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                           &message_handler_, callback.get());
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheBackendHits));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheBackendMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheMisses));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheExpirations));

  // However, the fallback value should be filled in.
  HTTPValue* fallback_value = callback->fallback_http_value();
  meta_data_out.Clear();
  contents.clear();
  EXPECT_FALSE(fallback_value->Empty());
  ASSERT_TRUE(fallback_value->ExtractHeaders(&meta_data_out,
                                             &message_handler_));
  ASSERT_TRUE(meta_data_out.headers_complete());
  ASSERT_TRUE(fallback_value->ExtractContents(&contents));
  ASSERT_STREQ("value", meta_data_out.Lookup1("name"));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));

  // Try again but with the cache invalidated.
  simple_stats_.Clear();
  scoped_ptr<Callback> callback2(NewCallback());
  callback2->cache_valid_ = false;
  found = FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                           &message_handler_, callback2.get());
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheBackendHits));
  // The fallback is empty since the entry has been invalidated.
  fallback_value = callback2->fallback_http_value();
  ASSERT_TRUE(fallback_value->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
}

TEST_F(HTTPCacheTest, PutGetForInvalidUrl) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeHtml.mime_type());
  meta_data_in.ComputeCaching();
  // The response for the invalid url does not get cached.
  Put("blah", kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      "blah", kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
}

TEST_F(HTTPCacheTest, PutGetForHttps) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeHtml.mime_type());
  meta_data_in.ComputeCaching();
  // Disable caching of html on https.
  http_cache_->set_disable_html_caching_on_https(true);
  // The html response does not get cached.
  Put(kHttpsUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kHttpsUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);

  // However a css file is cached.
  meta_data_in.Replace(HttpAttributes::kContentType,
                       kContentTypeCss.mime_type());
  meta_data_in.ComputeCaching();
  Put(kHttpsUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  found = Find(kHttpsUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));
}

TEST_F(HTTPCacheTest, EtagsAddedIfAbsent) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));

  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());

  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_STREQ(HTTPCache::FormatEtag("0"),
               meta_data_out.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

TEST_F(HTTPCacheTest, EtagsNotAddedIfPresent) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  meta_data_in.Add(HttpAttributes::kEtag, "Etag!");
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));

  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());

  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_STREQ("Etag!", meta_data_out.Lookup1(HttpAttributes::kEtag));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

TEST_F(HTTPCacheTest, CookiesNotCached) {
  simple_stats_.Clear();
  ResponseHeaders meta_data_in, meta_data_out;
  meta_data_in.Add(HttpAttributes::kSetCookie, "cookies!");
  meta_data_in.Add(HttpAttributes::kSetCookie2, "more cookies!");
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheInserts));
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheHits));
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kFound, found);
  ASSERT_TRUE(meta_data_out.headers_complete());
  StringPiece contents;
  ASSERT_TRUE(value.ExtractContents(&contents));
  ConstStringStarVector values;
  ASSERT_TRUE(meta_data_out.Lookup("name", &values));
  ASSERT_EQ(static_cast<size_t>(1), values.size());
  EXPECT_EQ(GoogleString("value"), *(values[0]));
  EXPECT_FALSE(meta_data_out.Lookup(HttpAttributes::kSetCookie, &values));
  EXPECT_FALSE(meta_data_out.Lookup(HttpAttributes::kSetCookie2, &values));
  EXPECT_EQ("content", contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheHits));  // The "query" counts as a hit.
}

// Verifies that the cache will 'remember' that a fetch failed for
// remember_fetch_failed_ttl_seconds_.
TEST_F(HTTPCacheTest, RememberFetchFailed) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFetchFailed(kUrl, kFragment, &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  http_cache_->set_remember_fetch_failed_ttl_seconds(600);
  http_cache_->RememberFetchFailed(kUrl, kFragment, &message_handler_);
  // Now advance time 301 seconds; the cache should remember that the fetch
  // failed previously.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

// Verifies that the cache will 'remember' 'non-cacheable' for
// remember_not_cacheable_ttl_seconds_.
TEST_F(HTTPCacheTest, RememberNotCacheableNot200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberNotCacheable(kUrl, kFragment, false, &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  http_cache_->set_remember_not_cacheable_ttl_seconds(600);
  http_cache_->RememberNotCacheable(kUrl, kFragment, false, &message_handler_);
  // Now advance time 301 seconds; the cache should remember that the fetch
  // failed previously.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

// Verifies that the cache will 'remember' 'non-cacheable' for
// remember_not_cacheable_ttl_seconds_.
TEST_F(HTTPCacheTest, RememberNotCacheable200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberNotCacheable(kUrl, kFragment, true, &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  http_cache_->set_remember_not_cacheable_ttl_seconds(600);
  http_cache_->RememberNotCacheable(kUrl, kFragment, true, &message_handler_);
  // Now advance time 301 seconds; the cache should remember that the fetch
  // failed previously.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

// Verifies that the cache will 'remember' 'dropped' for
// remember_dropped_ttl_seconds_.
TEST_F(HTTPCacheTest, RememberDropped) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFetchDropped(kUrl, kFragment, &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // Advance by 5 seconds: must still be here.
  mock_timer_.AdvanceMs(5 * Timer::kSecondMs);
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // After 6 more => 11 seconds later the cache should now let us retry
  // again.
  mock_timer_.AdvanceMs(6 * Timer::kSecondMs);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  http_cache_->set_remember_fetch_dropped_ttl_seconds(60);
  http_cache_->RememberFetchDropped(kUrl, kFragment, &message_handler_);
  // Now should remember after 11 seconds.
  mock_timer_.AdvanceMs(11 * Timer::kSecondMs);
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
  // ... but not after 61.
  mock_timer_.AdvanceMs(50 * Timer::kSecondMs);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

// Remember empty resources.
TEST_F(HTTPCacheTest, RememberEmpty) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberEmpty(kUrl, kFragment, &message_handler_);
  HTTPValue value;
  EXPECT_EQ(HTTPCache::kRecentFetchEmpty,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  // Now advance time 301 seconds; the cache should allow us to try fetching
  // again.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  http_cache_->set_remember_empty_ttl_seconds(600);
  http_cache_->RememberEmpty(kUrl, kFragment, &message_handler_);
  // Now advance time 301 seconds; the cache should remember that the resource
  // is empty.
  mock_timer_.AdvanceMs(301 * 1000);
  EXPECT_EQ(HTTPCache::kRecentFetchEmpty,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

// Make sure we don't remember 'non-cacheable' once we've put it into
// non-recording of failures mode (but do before that), and that we
// remember successful results even when in SetIgnoreFailurePuts() mode.
TEST_F(HTTPCacheTest, IgnoreFailurePuts) {
  http_cache_->RememberNotCacheable(kUrl, kFragment, false, &message_handler_);
  http_cache_->SetIgnoreFailurePuts();
  http_cache_->RememberNotCacheable(kUrl2, kFragment, false, &message_handler_);

  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl3, kFragment, &meta_data_in, "content", &message_handler_);

  HTTPValue value_out;
  EXPECT_EQ(
      HTTPCache::kRecentFetchNotCacheable,
      Find(kUrl, kFragment, &value_out, &meta_data_out, &message_handler_));
  EXPECT_EQ(
      HTTPCache::kNotFound,
      Find(kUrl2, kFragment, &value_out, &meta_data_out, &message_handler_));
  EXPECT_EQ(
      HTTPCache::kFound,
      Find(kUrl3, kFragment, &value_out, &meta_data_out, &message_handler_));
}

TEST_F(HTTPCacheTest, Uncacheable) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, NULL);
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

TEST_F(HTTPCacheTest, UncacheablePrivate) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "private, max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  HTTPValue value;
  HTTPCache::FindResult found = Find(
      kUrl, kFragment, &value, &meta_data_out, &message_handler_);
  ASSERT_EQ(HTTPCache::kNotFound, found);
  ASSERT_FALSE(meta_data_out.headers_complete());
}

// Unit testing cache invalidation.
TEST_F(HTTPCacheTest, CacheInvalidation) {
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  HTTPValue value;
  // Check with cache valid.
  EXPECT_EQ(
      HTTPCache::kFound,
      Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_, true));
  // Check with cache invalidated.
  EXPECT_EQ(
      HTTPCache::kNotFound,
      Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_, false));
}

TEST_F(HTTPCacheTest, IsFresh) {
  const char kDataIn[] = "content";
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, kDataIn, &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  callback->fresh_ = true;
  // Check with IsFresh set to true.
  EXPECT_EQ(HTTPCache::kFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  StringPiece contents;
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));

  callback.reset(NewCallback());
  value.Clear();
  callback->fresh_ = false;
  // Check with IsFresh set to false.
  EXPECT_EQ(HTTPCache::kNotFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  EXPECT_TRUE(value.Empty());
  EXPECT_TRUE(callback->fallback_http_value()->ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMs) {
  simple_stats_.Clear();

  // First test overriding works for a publicly cacheable response if the
  // override TTL is larger than the original one.
  const char kDataIn[] = "content";
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, kDataIn, &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(HTTPCache::kFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  StringPiece contents;
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now, test that overriding has no effect if the override TTL is less than
  // the original one.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::kFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=300",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now, test that overriding works for Cache-Control: private responses.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  InitHeaders(&meta_data_in, "private");
  Put(kUrl, kFragment, &meta_data_in, kDataIn, &message_handler_);
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(HTTPCache::kFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));

  // Now advance the time by 310 seconds and set override cache TTL to 300
  // seconds. The lookup fails.
  simple_stats_.Clear();
  mock_timer_.AdvanceMs(310 * 1000);
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  callback->override_cache_ttl_ms_ = 300 * 1000;
  EXPECT_EQ(HTTPCache::kNotFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  EXPECT_EQ(1, GetStat(HTTPCache::kCacheFallbacks));

  // Set the override cache TTL to 400 seconds. The lookup succeeds and the
  // Cache-Control header is updated.
  simple_stats_.Clear();
  callback.reset(NewCallback());
  value.Clear();
  meta_data_in.Clear();
  callback->override_cache_ttl_ms_ = 400 * 1000;
  EXPECT_EQ(HTTPCache::kFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
  EXPECT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ(kDataIn, contents);
  EXPECT_TRUE(callback->fallback_http_value()->Empty());
  EXPECT_EQ(0, GetStat(HTTPCache::kCacheFallbacks));
  EXPECT_STREQ("max-age=400",
               meta_data_out.Lookup1(HttpAttributes::kCacheControl));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyNotCacheable200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberNotCacheable(kUrl, kFragment, true, &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup returns
  // kNotFound now.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::kNotFound,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyNotCacheableNon200) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberNotCacheable(kUrl, kFragment, false, &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup returns
  // kNotFound now.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::kRecentFetchNotCacheable,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
}

TEST_F(HTTPCacheTest, OverrideCacheTtlMsForOriginallyFetchFailed) {
  ResponseHeaders meta_data_out;
  http_cache_->RememberFetchFailed(kUrl, kFragment, &message_handler_);
  HTTPValue value;
  scoped_ptr<Callback> callback(NewCallback());
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));

  // Now change the value of override_cache_ttl_ms_. The lookup continues to
  // return kRecentFetchFailed.
  callback.reset(NewCallback());
  value.Clear();
  callback->override_cache_ttl_ms_ = 200 * 1000;
  EXPECT_EQ(HTTPCache::kRecentFetchFailed,
            FindWithCallback(kUrl, kFragment, &value, &meta_data_out,
                             &message_handler_, callback.get()));
}

TEST_F(HTTPCacheTest, FragmentsIndependent) {
  HTTPValue value;
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  Put(kUrl, kFragment, &meta_data_in, "content", &message_handler_);
  ASSERT_EQ(HTTPCache::kFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
  ASSERT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment2, &value, &meta_data_out, &message_handler_));
  Put(kUrl, kFragment2, &meta_data_in, "content", &message_handler_);
  ASSERT_EQ(HTTPCache::kFound,
            Find(kUrl, kFragment2, &value, &meta_data_out, &message_handler_));
}

TEST_F(HTTPCacheTest, UpdateVersion) {
  HTTPValue value;
  ResponseHeaders meta_data_in, meta_data_out;
  InitHeaders(&meta_data_in, "max-age=300");
  StringPiece contents;

  // Equivalent to pre-versioned caching.
  http_cache_->set_version_prefix("");
  Put(kUrl, "", &meta_data_in, "v1: No fragment", &message_handler_);
  Put(kUrl, kFragment, &meta_data_in, "v1: Fragment", &message_handler_);

  EXPECT_EQ(HTTPCache::kFound,
            Find(kUrl, "", &value, &meta_data_out, &message_handler_));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v1: No fragment", contents);
  EXPECT_EQ(HTTPCache::kFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v1: Fragment", contents);

  // Setting version invalidates old data.
  http_cache_->SetVersion(2);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, "", &value, &meta_data_out, &message_handler_));
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));

  Put(kUrl, "", &meta_data_in, "v2: No fragment", &message_handler_);
  Put(kUrl, kFragment, &meta_data_in, "v2: Fragment", &message_handler_);

  EXPECT_EQ(HTTPCache::kFound,
            Find(kUrl, "", &value, &meta_data_out, &message_handler_));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v2: No fragment", contents);
  EXPECT_EQ(HTTPCache::kFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
  ASSERT_TRUE(value.ExtractContents(&contents));
  EXPECT_STREQ("v2: Fragment", contents);

  // Updating version invalidates old data.
  http_cache_->SetVersion(3);
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, "", &value, &meta_data_out, &message_handler_));
  EXPECT_EQ(HTTPCache::kNotFound,
            Find(kUrl, kFragment, &value, &meta_data_out, &message_handler_));
}

}  // namespace net_instaweb
