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

#ifndef PAGESPEED_ENVOY_FETCH_H_
#define PAGESPEED_ENVOY_FETCH_H_

#pragma once

#include "envoy_url_async_fetcher.h"
#include <vector>
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/pool.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"
#include "pagespeed/kernel/thread/pthread_mutex.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed_remote_data_fetcher.h"

namespace net_instaweb {

class EnvoyUrlAsyncFetcher;

class EnvoyFetch : public PoolElement<EnvoyFetch> {
 public:
  EnvoyFetch(const GoogleString& url,
           AsyncFetch* async_fetch,
           MessageHandler* message_handler,
           EnvoyClusterManager* cluster_manager);
  ~EnvoyFetch();

 void FetchWithEnvoy();

  // Start the fetch.
  void Start();

  // This fetch task is done. Call Done() on the async_fetch. It will copy the
  // buffer to cache.
  void CallbackDone(bool success);

  MessageHandler* message_handler();

  int get_status_code() {
    return 0;
  }

 private:
  // Do the initialized work and start the resolver work.
  bool Init();

  // Prepare the request and write it to remote server.
  int InitRequest();
  // Create the connection with remote server.
  int Connect();

  // Add the pagespeed User-Agent.
  void FixUserAgent();
  void FixHost();

  const GoogleString str_url_;
  EnvoyUrlAsyncFetcher* fetcher_;
  AsyncFetch* async_fetch_;
  ResponseHeadersParser parser_;
  MessageHandler* message_handler_;
  EnvoyClusterManager* cluster_manager_;
  bool done_;
  int64 content_length_;
  bool content_length_known_;

  struct sockaddr_in sin_;

  DISALLOW_COPY_AND_ASSIGN(EnvoyFetch);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_ENVOY_FETCH_H_
