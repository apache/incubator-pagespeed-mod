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


#include "net/instaweb/http/public/url_async_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

const int64 UrlAsyncFetcher::kUnspecifiedTimeout = 0;

// TODO(oschaaf): we have counters for this, needs a mutex
int64 UrlAsyncFetcher::fetchId = 0;

UrlAsyncFetcher::~UrlAsyncFetcher() {
}

void UrlAsyncFetcher::ShutDown() {
}

AsyncFetch* UrlAsyncFetcher::EnableInflation(AsyncFetch* fetch) const {
  InflatingFetch* inflating_fetch = new InflatingFetch(fetch);
  if (fetch_with_gzip_) {
    inflating_fetch->EnableGzipFromBackend();
  }
  return inflating_fetch;
}

class TracingFetch : public SharedAsyncFetch {
 public:
  TracingFetch(UrlAsyncFetcher* sender, MessageHandler* message_handler, const GoogleString& url, AsyncFetch* base_fetch)
      : SharedAsyncFetch(base_fetch), url_(url), base_fetch_(base_fetch), original_fetcher_name_(typeid(*sender).name()), message_handler_(message_handler) {
    RequestHeaders* request_headers = base_fetch->request_headers();
    const char* id = request_headers->Lookup1("X-PageSpeed-UrlAsyncFetcher-Id");
    const char* nested = request_headers->Lookup1("X-PageSpeed-UrlAsyncFetcher-Nested");
    if (id == nullptr) {
      request_headers->Add("X-PageSpeed-UrlAsyncFetcher-Id", Integer64ToString(++UrlAsyncFetcher::fetchId));
    }
    int64 iNested = 0;
    if (nested == nullptr) {
      request_headers->Add("X-PageSpeed-UrlAsyncFetcher-Nested", "0");
    } else {
      StringToInt64(nested, &iNested);
      request_headers->Replace("X-PageSpeed-UrlAsyncFetcher-Nested", Integer64ToString(++iNested));
    }
    id = request_headers->Lookup1("X-PageSpeed-UrlAsyncFetcher-Id");
    id_ = GoogleString(id);
    nested_ = iNested;

    AsyncFetch* f = base_fetch_;
    while (dynamic_cast<TracingFetch*>(f) != nullptr) {
      f = dynamic_cast<TracingFetch*>(f)->base_fetch_;
    }
    
    base_fetch_name_ = GoogleString(typeid(*f).name());
    message_handler_->Message(kInfo, "(f:%s-%ld) %s by %s for %s", 
        id,
        nested_,
        url_.c_str(),
        original_fetcher_name_.c_str(),
        base_fetch_name_.c_str());
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    return SharedAsyncFetch::HandleWrite(content, handler);
  }
  
  virtual void HandleHeadersComplete() {
    traced_request_headers_.CopyFrom(*base_fetch_->request_headers());
    traced_response_headers_.CopyFrom(*base_fetch_->response_headers());
    SharedAsyncFetch::HandleHeadersComplete();
  }

  virtual void HandleDone(bool success) {
    const char* id = id_.c_str();

    message_handler_->Message(kInfo, "(d:%s-%ld) %s from %s (%d) on behalf of %s", 
        id,
        nested_,
        url_.c_str(),
        base_fetch_name_.c_str(),
        success ? 1 : 0,
        original_fetcher_name_.c_str());

    if (!nested_) {
      message_handler_->Message(kInfo, "           (RQ) %s", 
          traced_request_headers_.ToString().c_str());
      message_handler_->Message(kInfo, "           (RS) %s", 
          traced_response_headers_.ToString().c_str());      
    }
    SharedAsyncFetch::HandleDone(success);
    delete this;
  }

 private:
  GoogleString url_;
  AsyncFetch* base_fetch_;
  GoogleString original_fetcher_name_;
  GoogleString base_fetch_name_;
  int64 nested_;
  RequestHeaders traced_request_headers_;
  ResponseHeaders traced_response_headers_;
  MessageHandler* message_handler_;
  GoogleString id_;
  
  DISALLOW_COPY_AND_ASSIGN(TracingFetch);
};

void UrlAsyncFetcher::Fetch(const GoogleString& url,
            MessageHandler* message_handler,
            AsyncFetch* fetch) {
  FetchImpl(url, message_handler, new TracingFetch(this, message_handler, url, fetch));
}

}  // namespace net_instaweb
