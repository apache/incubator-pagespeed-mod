/*
 * Copyright 2016 Google Inc.
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
// Author: oschaaf@we-amp.com (Otto van der Schaaf)

#include "net/instaweb/http/public/tracing_url_async_fetcher.h"
#include <iostream>
#include <utility>

#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/scheduler.h"

namespace net_instaweb {

TracingUrlAsyncFetcher::TracingUrlAsyncFetcher(UrlAsyncFetcher* fetcher) :
    base_fetcher_(fetcher) {
}

TracingUrlAsyncFetcher::~TracingUrlAsyncFetcher() {
}

void TracingUrlAsyncFetcher::FetchImpl(const GoogleString& url,
                                  MessageHandler* message_handler,
                                  AsyncFetch* fetch) {
  //std::cerr << "pre trace fetch: " << url << " " << fetch->request_headers()->ToString() << std::endl;
  base_fetcher_->Fetch(url, message_handler, fetch);
}

bool TracingUrlAsyncFetcher::SupportsHttps() const { 
    return base_fetcher_->SupportsHttps(); 
}

int64 TracingUrlAsyncFetcher::timeout_ms() {
  return base_fetcher_->timeout_ms();
}

void TracingUrlAsyncFetcher::ShutDown() {
  base_fetcher_->ShutDown();
}

}  // namespace net_instaweb
