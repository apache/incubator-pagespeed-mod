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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_ORIGIN_MAPPING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_ORIGIN_MAPPING_URL_ASYNC_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

class AbstractMutex;
class AsyncFetch;
class MessageHandler;
class Scheduler;
class ThreadSystem;
class Timer;

class OriginMappingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  OriginMappingUrlAsyncFetcher(UrlAsyncFetcher* fetcher);

  virtual ~OriginMappingUrlAsyncFetcher();

  virtual bool SupportsHttps() const;

  virtual int64 timeout_ms();

  virtual void ShutDown();

 protected:
  virtual void FetchImpl(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);
 
 private:
  UrlAsyncFetcher* base_fetcher_;
  DISALLOW_COPY_AND_ASSIGN(OriginMappingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_ORIGIN_MAPPING_URL_ASYNC_FETCHER_H_
