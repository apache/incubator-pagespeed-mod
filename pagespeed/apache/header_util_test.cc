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


#include "pagespeed/apache/header_util.h"

#include "pagespeed/apache/mock_apache.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"

#include "http_request.h"                                            // NOLINT

namespace net_instaweb {

class HeaderUtilTest : public testing::Test {
 public:
  void PredicateMatchingA(StringPiece name, bool* ok) {
    *ok = (name == "a");
  }

 protected:
  virtual void SetUp() {
    MockApache::Initialize();
    MockApache::PrepareRequest(&request_);
  }

  virtual void TearDown() {
    MockApache::CleanupRequest(&request_);
    MockApache::Terminate();
  }

  void SetLastModified(const char* last_modified) {
    apr_table_set(request_.headers_out, HttpAttributes::kLastModified,
                  last_modified);
  }

  const char* GetLastModified() {
    return apr_table_get(request_.headers_out, HttpAttributes::kLastModified);
  }

  void SetCacheControl(const char* cache_control) {
    apr_table_set(request_.headers_out, HttpAttributes::kCacheControl,
                  cache_control);
  }

  const char* GetCacheControl() {
    return apr_table_get(request_.headers_out, HttpAttributes::kCacheControl);
  }

  request_rec request_;
};

TEST_F(HeaderUtilTest, DisableEmpty) {
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableCaching) {
  SetCacheControl("max-age=60");
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisablePrivateCaching) {
  SetCacheControl("private, max-age=60");
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisablePublicCaching) {
  SetCacheControl("public, max-age=60");
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(HttpAttributes::kNoCacheMaxAge0, GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableNostore) {
  SetCacheControl("must-revalidate, private, no-store");
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               GetCacheControl());
}

TEST_F(HeaderUtilTest, DisableNostoreRetainNoCache) {
  SetCacheControl("no-cache, must-revalidate, private, no-store");
  SetLastModified("some random string");
  DisableCacheControlHeader(&request_);
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               GetCacheControl());
  EXPECT_STREQ("some random string", GetLastModified());
}

TEST_F(HeaderUtilTest, DisableCachingRelatedHeaders) {
  SetCacheControl("no-cache, must-revalidate, private, no-store");
  SetLastModified("some random string");
  DisableCachingRelatedHeaders(&request_);
  DisableCacheControlHeader(&request_);
  EXPECT_EQ(NULL, GetLastModified());
  EXPECT_STREQ(StrCat(HttpAttributes::kNoCacheMaxAge0,
                      ", must-revalidate, ",
                      HttpAttributes::kNoStore),
               GetCacheControl());
}

TEST_F(HeaderUtilTest, SelectiveRequestHeaders) {
  apr_table_set(request_.headers_in, "a", "b");
  apr_table_set(request_.headers_in, "c", "d");
  RequestHeaders all, selective;
  ApacheRequestToRequestHeaders(request_, &all);
  EXPECT_STREQ("b", all.Lookup1("a"));
  EXPECT_STREQ("d", all.Lookup1("c"));
  EXPECT_EQ(2, all.NumAttributes());
  HeaderUtilTest* test = this;
  scoped_ptr<HeaderPredicateFn> predicate(NewPermanentCallback(
      test, &HeaderUtilTest::PredicateMatchingA));
  ApacheRequestToRequestHeaders(request_, &selective, predicate.get());
  EXPECT_STREQ("b", selective.Lookup1("a"));
  EXPECT_TRUE(selective.Lookup1("c") == NULL);
  EXPECT_EQ(1, selective.NumAttributes());
}

}  // namespace net_instaweb
