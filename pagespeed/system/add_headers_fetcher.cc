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


#include "pagespeed/system/add_headers_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

AddHeadersFetcher::AddHeadersFetcher(const RewriteOptions* options,
                                     UrlAsyncFetcher* backend_fetcher)
    : options_(options), backend_fetcher_(backend_fetcher) {
}

AddHeadersFetcher::~AddHeadersFetcher() {}

void AddHeadersFetcher::Fetch(const GoogleString& original_url,
                              MessageHandler* message_handler,
                              AsyncFetch* fetch) {
  RequestHeaders* request_headers = fetch->request_headers();
  for (int i = 0; i < options_->num_custom_fetch_headers(); ++i) {
    const RewriteOptions::NameValue* nv = options_->custom_fetch_header(i);
    request_headers->Replace(nv->name, nv->value);
  }
  backend_fetcher_->Fetch(original_url, message_handler, fetch);
}

}  // namespace net_instaweb

