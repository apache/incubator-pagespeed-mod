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

//
// Contains UserAgentSensitiveTestFetcher, which appends the UA string as a
// query param before delegating to another fetcher. Meant for use in
// unit tests.

#ifndef NET_INSTAWEB_HTTP_PUBLIC_UA_SENSITIVE_TEST_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_UA_SENSITIVE_TEST_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;

// A helper fetcher that adds in UA to the URL, so we can use
// MockUrlAsyncFetcher w/UA-sensitive things. It also enforces the
// domain whitelist.
class UserAgentSensitiveTestFetcher : public UrlAsyncFetcher {
 public:
  explicit UserAgentSensitiveTestFetcher(UrlAsyncFetcher* base_fetcher);
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  virtual bool SupportsHttps() const;

 private:
  UrlAsyncFetcher* base_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(UserAgentSensitiveTestFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_UA_SENSITIVE_TEST_FETCHER_H_
