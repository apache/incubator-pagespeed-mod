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

#include "envoy_fetch.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/inflating_fetch.h"
#include "pagespeed/envoy/header_utils.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/pool_element.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"
#include "pagespeed_remote_data_fetcher.h"

namespace net_instaweb {

// Default keepalive 60s.
// const int64 keepalive_timeout_ms = 60000;

PagespeedDataFetcherCallback::PagespeedDataFetcherCallback(EnvoyFetch* fetch) {
  fetch_ = fetch;
}

void PagespeedDataFetcherCallback::onSuccess(
    Envoy::Http::ResponseMessagePtr& response) {
  fetch_->setResponse(response->headers(), response->body());
}

void PagespeedDataFetcherCallback::onFailure(FailureReason reason) {
  // TODO : Handle fetch failure conditions
  // Possible failures can be
  // 1. host not reachable
  // 2. timeout error
}

EnvoyFetch::EnvoyFetch(const GoogleString& url, AsyncFetch* async_fetch,
                       MessageHandler* message_handler,
                       EnvoyClusterManager& cluster_manager)
    : str_url_(url),
      fetcher_(nullptr),
      async_fetch_(async_fetch),
      message_handler_(message_handler),
      cluster_manager_(cluster_manager),
      done_(false),
      content_length_(-1),
      content_length_known_(false) {}

void EnvoyFetch::FetchWithEnvoy() {
  envoy::config::core::v3::HttpUri http_uri;
  http_uri.set_uri(str_url_);
  http_uri.set_cluster(cluster_manager_.getClusterName());
  cb_ptr_ = std::make_unique<PagespeedDataFetcherCallback>(this);

  std::unique_ptr<PagespeedRemoteDataFetcher> pagespeed_remote_data_fetch_ptr =
      std::make_unique<PagespeedRemoteDataFetcher>(
          cluster_manager_.getClusterManager(str_url_), http_uri, *cb_ptr_);

  pagespeed_remote_data_fetch_ptr->fetch();
  cluster_manager_.getDispatcher()->run(
      Envoy::Event::Dispatcher::RunType::Block);
}

// This function is called by EnvoyUrlAsyncFetcher::StartFetch.
void EnvoyFetch::Start() {
  std::function<void()> fetch_fun_ptr =
      std::bind(&EnvoyFetch::FetchWithEnvoy, this);
  cluster_manager_.getDispatcher()->post(fetch_fun_ptr);
  cluster_manager_.getDispatcher()->run(
      Envoy::Event::Dispatcher::RunType::NonBlock);
}

bool EnvoyFetch::Init() { return true; }

void EnvoyFetch::setResponse(Envoy::Http::HeaderMap& headers,
                             Envoy::Buffer::Instance& response_body) {
  ResponseHeaders* res_header = async_fetch_->response_headers();
  std::unique_ptr<ResponseHeaders> response_headers_ptr =
      HeaderUtils::toPageSpeedResponseHeaders(headers);
  res_header->CopyFrom(*response_headers_ptr);

  async_fetch_->response_headers()->SetOriginalContentLength(
      response_body.length());
  if (async_fetch_->response_headers()->Has(
          HttpAttributes::kXOriginalContentLength)) {
    async_fetch_->extra_response_headers()->SetOriginalContentLength(
        response_body.length());
  }

  async_fetch_->Write(StringPiece(response_body.toString()), message_handler());

  async_fetch_->Done(true);
}

MessageHandler* EnvoyFetch::message_handler() { return message_handler_; }

void EnvoyFetch::FixUserAgent() {}

// Prepare the request data for this fetch, and hook the write event.
int EnvoyFetch::InitRequest() { return 0; }

int EnvoyFetch::Connect() { return 0; }

}  // namespace net_instaweb
