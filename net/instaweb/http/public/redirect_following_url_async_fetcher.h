/*
 * Copyright 2015 Google Inc.
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

#ifndef NET_INSTAWEB_HTTP_PUBLIC_REDIRECT_FOLLOWING_URL_ASYNC_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_REDIRECT_FOLLOWING_URL_ASYNC_FETCHER_H_

#include <set>

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class Statistics;
class ThreadSystem;


typedef std::set<GoogleString> GoogleUrlSet;

class RedirectFollowingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // Does not take ownership of 'fetcher'.
  RedirectFollowingUrlAsyncFetcher(UrlAsyncFetcher* fetcher,
                                 ThreadSystem* thread_system,
                                 Statistics* statistics,
                                 int max_redirects_);

  virtual ~RedirectFollowingUrlAsyncFetcher();

  virtual bool SupportsHttps() const {
    return base_fetcher_->SupportsHttps();
  }

  void FollowRedirect(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch, int count,
                     GoogleUrlSet* redirects_followed_earlier);

  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  virtual void ShutDown();

  class RedirectFollowingFetch;
  friend class RedirectFollowingFetch;
  int max_redirects() { return max_redirects_; }
 private:
  UrlAsyncFetcher* base_fetcher_;
  int max_redirects_;

  DISALLOW_COPY_AND_ASSIGN(RedirectFollowingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REDIRECT_FOLLOWING_URL_ASYNC_FETCHER_H_
