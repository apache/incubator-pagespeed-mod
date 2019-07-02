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


#include "net/instaweb/http/public/mock_url_fetcher.h"

#include <map>
#include <utility>                      // for pair

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class MessageHandler;

MockUrlFetcher::MockUrlFetcher()
    : enabled_(true),
      fail_on_unexpected_(true),
      update_date_headers_(false),
      omit_empty_writes_(false),
      fail_after_headers_(false),
      verify_host_header_(false),
      verify_pagespeed_header_off_(false),
      split_writes_(false),
      supports_https_(false),
      strip_query_params_(false),
      timer_(NULL),
      thread_system_(Platform::CreateThreadSystem()),
      // TODO(hujie): We should pass in the mutex at all call-sites instead of
      //     creating a new mutex here.
      mutex_(thread_system_->NewMutex()) {
}

MockUrlFetcher::~MockUrlFetcher() {
  Clear();
}

void MockUrlFetcher::SetResponse(const StringPiece& url,
                                 const ResponseHeaders& response_header,
                                 const StringPiece& response_body) {
  // Note: This is a little kludgey, but if you set a normal response and
  // always perform normal GETs you won't even notice that we've set the
  // last_modified_time internally.
  DCHECK(response_header.headers_complete());
  SetConditionalResponse(url, 0, "" , response_header, response_body);
}

void MockUrlFetcher::AddToResponse(const StringPiece& url,
                                   const StringPiece& name,
                                   const StringPiece& value) {
  ResponseMap::iterator iter = response_map_.find(url.as_string());
  CHECK(iter != response_map_.end());
  HttpResponse* http_response = iter->second;
  ResponseHeaders* response = http_response->mutable_header();
  response->Add(name, value);
  response->ComputeCaching();
}

void MockUrlFetcher::SetResponseFailure(const StringPiece& url) {
  ResponseMap::iterator iter = response_map_.find(url.as_string());
  CHECK(iter != response_map_.end());
  HttpResponse* http_response = iter->second;
  http_response->set_success(false);
}

void MockUrlFetcher::SetConditionalResponse(
    const StringPiece& url, int64 last_modified_time, const GoogleString& etag,
    const ResponseHeaders& response_header, const StringPiece& response_body) {
  GoogleString url_string = url.as_string();
  // Remove any old response.
  RemoveResponse(url);

  // Add new response.
  HttpResponse* response = new HttpResponse(last_modified_time, etag,
                                            response_header, response_body);
  response_map_.insert(ResponseMap::value_type(url_string, response));
}

void MockUrlFetcher::Clear() {
  STLDeleteContainerPairSecondPointers(response_map_.begin(),
                                       response_map_.end());
  response_map_.clear();
  // We don't have to protect response_map_ here, since only single
  // setup/teardown would be called at a time.
  {
    ScopedMutex lock(mutex_.get());
    last_referer_.clear();
  }
}

void MockUrlFetcher::RemoveResponse(const StringPiece& url) {
  GoogleString url_string = url.as_string();
  ResponseMap::iterator iter = response_map_.find(url_string);
  if (iter != response_map_.end()) {
    delete iter->second;
    response_map_.erase(iter);
  }
}

void MockUrlFetcher::Fetch(
    const GoogleString& url_in, MessageHandler* message_handler,
    AsyncFetch* fetch) {
  const RequestHeaders& request_headers = *fetch->request_headers();
  ResponseHeaders* response_headers = fetch->response_headers();
  bool ret = false;

  bool enabled;
  bool verify_host_header;
  bool verify_pagespeed_header_off;
  bool fail_after_headers;
  bool update_date_headers;
  bool omit_empty_writes;
  bool fail_on_unexpected;
  bool split_writes;
  bool strip_query_params;
  GoogleString error_message;
  Timer* timer;
  {
    ScopedMutex lock(mutex_.get());
    enabled = enabled_;
    verify_host_header = verify_host_header_;
    verify_pagespeed_header_off = verify_pagespeed_header_off_;
    timer = timer_;
    fail_after_headers = fail_after_headers_;
    update_date_headers = update_date_headers_;
    omit_empty_writes = omit_empty_writes_;
    fail_on_unexpected = fail_on_unexpected_;
    error_message = error_message_;
    split_writes = split_writes_;
    strip_query_params = strip_query_params_;
  }
  if (enabled) {
    GoogleString url = url_in;  // editable version
    GoogleUrl gurl(url);
    EXPECT_TRUE(gurl.IsAnyValid());

    if (strip_query_params) {
      url = gurl.AllExceptQuery().as_string();
      gurl.Reset(url);
    }
    // Verify that the url and Host: header match.
    if (verify_host_header) {
      const char* host_header = request_headers.Lookup1(HttpAttributes::kHost);
      EXPECT_STREQ(gurl.HostAndPort(), host_header);
    }
    if (verify_pagespeed_header_off) {
      EXPECT_TRUE(request_headers.HasValue("PageSpeed", "off"));
    }
    const char* referer = request_headers.Lookup1(HttpAttributes::kReferer);
    {
      ScopedMutex lock(mutex_.get());
      if (referer == NULL) {
        last_referer_.clear();
      } else {
        last_referer_ = referer;
      }
    }
    ResponseMap::iterator iter = response_map_.find(url);
    if (iter != response_map_.end()) {
      const HttpResponse* response = iter->second;
      ret = response->success();

      // Check if we should return 304 Not Modified or full response.
      ConstStringStarVector values;
      int64 if_modified_since_time;
      if (request_headers.Lookup(HttpAttributes::kIfModifiedSince, &values) &&
          values.size() == 1 &&
          ConvertStringToTime(*values[0], &if_modified_since_time) &&
          if_modified_since_time > 0 &&
          if_modified_since_time >= response->last_modified_time()) {
        // We recieved an If-Modified-Since header with a date that was
        // parsable and at least as new our new resource.
        //
        // So, just serve 304 Not Modified.
        response_headers->SetStatusAndReason(HttpStatus::kNotModified);
        // TODO(sligocki): Perhaps allow other headers to be set.
        // Date is technically required to be set.
      } else if (!response->etag().empty() &&
          request_headers.Lookup(HttpAttributes::kIfNoneMatch, &values) &&
          values.size() == 1 && *values[0] == response->etag()) {
        // We received an If-None-Match header whose etag matches that of the
        // stored response. serve a 304 Not Modified.
        response_headers->SetStatusAndReason(HttpStatus::kNotModified);
      } else {
        // Otherwise serve a normal 200 OK response.
        int64 implicit_cache_ttl_ms_ =
             response_headers->implicit_cache_ttl_ms();
        response_headers->CopyFrom(response->header());
        // implicit_cache_ttl_ms is set to default value from the origin
        // fetch. The explicit values set in the test case take precedence over
        // the default values set in origin fetch.
        response_headers->set_implicit_cache_ttl_ms(implicit_cache_ttl_ms_);
        if (fail_after_headers) {
          fetch->Done(false);
          return;
        }
        if (update_date_headers) {
          CHECK(timer != NULL);
          // Update Date headers.
          response_headers->SetDate(timer_->NowMs());
        }
        response_headers->ComputeCaching();

        if (!(response->body().empty() && omit_empty_writes)) {
          if (!split_writes) {
            // Normal case.
            fetch->Write(response->body(), message_handler);
          } else {
            // This is used to test Ajax's RecordingFetch's cache recovery.
            int mid = response->body().size() / 2;
            StringPiece body = response->body();
            StringPiece head = body.substr(0, mid);
            StringPiece tail = body.substr(mid, StringPiece::npos);
            if (!(head.empty() && omit_empty_writes)) {
              fetch->Write(head, message_handler);
            }
            if (!(tail.empty() && omit_empty_writes)) {
              fetch->Write(tail, message_handler);
            }
          }
        }
      }
    } else {
      // This is used in tests and we do not expect the test to request a
      // resource that we don't have. So fail if we do.
      //
      // If you want a 404 response, you must explicitly use SetResponse.
      if (fail_on_unexpected) {
        EXPECT_TRUE(false) << "Requested unset url " << url;
      }
    }
  }

  if (!ret && !error_message.empty()) {
    if (!response_headers->headers_complete()) {
      response_headers->SetStatusAndReason(HttpStatus::kInternalServerError);
    }
    fetch->Write(error_message, message_handler);
  }

  fetch->Done(ret);
}

}  // namespace net_instaweb
