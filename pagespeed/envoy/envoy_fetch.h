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

#pragma once

#include <vector>

#include "envoy_url_async_fetcher.h"
#include "envoy/config/core/v3/http_uri.pb.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"
#include "pagespeed_remote_data_fetcher.h"

namespace net_instaweb {

class EnvoyUrlAsyncFetcher;
class EnvoyFetch;

class PagespeedDataFetcherCallback : public PagespeedRemoteDataFetcherCallback {
 public:
  PagespeedDataFetcherCallback(EnvoyFetch* fetch);

  /**
   * This function will be called when data is fetched successfully from remote.
   * @param response remote data
   */
  void onSuccess(Envoy::Http::ResponseMessagePtr& response) override;

  /**
   * This function will be called when data fetched is failed.
   * @param reason cause of failure
   */
  void onFailure(FailureReason reason) override;

 private:
  EnvoyFetch* fetch_;
};

class EnvoyFetch : public PoolElement<EnvoyFetch> {
 public:
  EnvoyFetch(const GoogleString& url, AsyncFetch* async_fetch,
             MessageHandler* message_handler,
             EnvoyClusterManager& cluster_manager);

  /**
   * This function starts fetching url by posting an event to dispatcher
   * url is passed during EnvoyFetch creation
   */
  void Start();

  /**
   * This function sets the response header and body using fetched data
   * @param headers Response header of fetched url
   * @param response_body Response body of fetched url
   */
  void setResponse(Envoy::Http::HeaderMap& headers,
                   Envoy::Buffer::Instance& response_body);

 private:
  // Do the initialized work and start the resolver work.
  bool Init();

  // Prepare the request and write it to remote server.
  int InitRequest();

  // Create the connection with remote server.
  int Connect();
  void FetchWithEnvoy();
  MessageHandler* message_handler();

  // Add the pagespeed User-Agent.
  void FixUserAgent();
  void FixHost();

  const GoogleString str_url_;
  EnvoyUrlAsyncFetcher* fetcher_;
  std::unique_ptr<PagespeedDataFetcherCallback> cb_ptr_;
  AsyncFetch* async_fetch_;
  MessageHandler* message_handler_;
  EnvoyClusterManager& cluster_manager_;
  bool done_;
  int64 content_length_;
  bool content_length_known_;

  struct sockaddr_in sin_;

  DISALLOW_COPY_AND_ASSIGN(EnvoyFetch);
};

}  // namespace net_instaweb
