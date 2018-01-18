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


// Unit-test the http dump fetcher, using a mock fetcher.  Note that
// the HTTP Dump Fetcher is, in essence, a caching fetcher except that:
//    1. It ignores caching headers completely
//    2. It uses file-based storage with no expectation of ever evicting
//       anything.
//
// TODO(jmarantz): consider making this class a special case of the
// combination of HTTPCache, FileCache, and HttpDumpUrlFetcher.

#include "net/instaweb/http/public/http_dump_url_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/request_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

class HttpDumpUrlFetcherTest : public testing::Test {
 public:
  HttpDumpUrlFetcherTest()
      : thread_system_(Platform::CreateThreadSystem()),
        mock_timer_(thread_system_->NewMutex(), 0),
        http_dump_fetcher_(
            GTestSrcDir() + "/net/instaweb/http/testdata",
            &file_system_,
            &mock_timer_) {
  }

 protected:
  scoped_ptr<ThreadSystem> thread_system_;
  MockTimer mock_timer_;
  StdioFileSystem file_system_;
  GoogleString content_;
  HttpDumpUrlFetcher http_dump_fetcher_;
  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpDumpUrlFetcherTest);
};

TEST_F(HttpDumpUrlFetcherTest, TestReadWithGzip) {
  ResponseHeaders response;
  RequestHeaders request;
  request.Add(HttpAttributes::kAcceptEncoding, HttpAttributes::kGzip);
  StringAsyncFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), &content_);
  fetch.set_response_headers(&response);
  fetch.set_request_headers(&request);

  http_dump_fetcher_.Fetch(
      "http://www.google.com", &message_handler_, &fetch);
  ASSERT_TRUE(fetch.done());
  ASSERT_TRUE(fetch.success());
  ConstStringStarVector v;
  ASSERT_TRUE(response.Lookup(HttpAttributes::kContentEncoding, &v));
  ASSERT_EQ(1, v.size());
  EXPECT_EQ(GoogleString(HttpAttributes::kGzip), *(v[0]));
  EXPECT_EQ(5513, content_.size());
  int64 content_length = 0;
  EXPECT_TRUE(response.FindContentLength(&content_length));
  EXPECT_EQ(content_length, 5513);
}

TEST_F(HttpDumpUrlFetcherTest, TestReadUncompressedFromGzippedDump) {
  ResponseHeaders response;
  StringAsyncFetch fetch(
      RequestContext::NewTestRequestContext(thread_system_.get()), &content_);
  fetch.set_response_headers(&response);

  http_dump_fetcher_.Fetch(
      "http://www.google.com", &message_handler_, &fetch);
  ASSERT_TRUE(fetch.done());
  ASSERT_TRUE(fetch.success());
  ConstStringStarVector v;
  if (response.Lookup(HttpAttributes::kContentEncoding, &v)) {
    ASSERT_EQ(1, v.size());
    EXPECT_NE(GoogleString(HttpAttributes::kGzip), *(v[0]));
  }
  EXPECT_EQ(14450, content_.size());
  int64 content_length = 0;
  EXPECT_TRUE(response.FindContentLength(&content_length));
  EXPECT_EQ(content_length, 14450);
}

// Helper that checks the Date: field as it starts writing.
class CheckDateHeaderFetch : public StringAsyncFetch {
 public:
  CheckDateHeaderFetch(const MockTimer* timer, ThreadSystem* threads)
      : StringAsyncFetch(RequestContext::NewTestRequestContext(threads)),
        headers_complete_called_(false), timer_(timer) {}
  virtual ~CheckDateHeaderFetch() {}

  virtual void HandleHeadersComplete() {
    headers_complete_called_ = true;
    response_headers()->ComputeCaching();
    EXPECT_EQ(timer_->NowMs(), response_headers()->date_ms());
  }

  bool headers_complete_called() const { return headers_complete_called_; }

 private:
  bool headers_complete_called_;
  const MockTimer* timer_;
  DISALLOW_COPY_AND_ASSIGN(CheckDateHeaderFetch);
};


TEST_F(HttpDumpUrlFetcherTest, TestDateAdjustment) {
  // Set a time in 2030s, which should be bigger than the time of the slurp,
  // which is a prerequisite for date adjustment
  mock_timer_.SetTimeUs(60 * Timer::kYearMs * Timer::kMsUs);

  // Make sure that date fixing up works in time for first write ---
  // which is needed for adapting it into an async fetcher.
  CheckDateHeaderFetch check_date(&mock_timer_, thread_system_.get());

  http_dump_fetcher_.Fetch("http://www.google.com", &message_handler_,
                           &check_date);
  EXPECT_TRUE(check_date.done());
  EXPECT_TRUE(check_date.success());
  EXPECT_TRUE(check_date.headers_complete_called());
}

}  // namespace

}  // namespace net_instaweb
