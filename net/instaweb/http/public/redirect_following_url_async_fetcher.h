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
class RewriteOptions;
class Statistics;
class ThreadSystem;

typedef std::set<GoogleString> GoogleUrlSet;

class RedirectFollowingUrlAsyncFetcher : public UrlAsyncFetcher {
 public:
  // Does not take ownership of 'fetcher'.
  RedirectFollowingUrlAsyncFetcher(UrlAsyncFetcher* fetcher,
                                   GoogleString context_url,
                                   ThreadSystem* thread_system,
                                   Statistics* statistics, int max_redirects,
                                   RewriteOptions* rewrite_options);

  virtual ~RedirectFollowingUrlAsyncFetcher();

  virtual bool SupportsHttps() const { return base_fetcher_->SupportsHttps(); }

  virtual void Fetch(const GoogleString& url, MessageHandler* message_handler,
                     AsyncFetch* fetch);

  class RedirectFollowingFetch;
  friend class RedirectFollowingFetch;

  // Returns the maximum number of redirects that will be followed.
  int max_redirects() { return max_redirects_; }
  const RewriteOptions* rewrite_options() { return rewrite_options_; }

 private:
  // Initiates a fetch of a pre-validated url originating from a Location
  // header of a response.
  void FollowRedirect(const GoogleString& valid_redirect_url,
                      MessageHandler* message_handler, AsyncFetch* fetch,
                      GoogleUrlSet* redirects_followed_earlier);
  UrlAsyncFetcher* base_fetcher_;
  // base url as stored on the request context.
  GoogleString context_url_;
  int max_redirects_;
  const RewriteOptions* rewrite_options_;

  DISALLOW_COPY_AND_ASSIGN(RedirectFollowingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REDIRECT_FOLLOWING_URL_ASYNC_FETCHER_H_
