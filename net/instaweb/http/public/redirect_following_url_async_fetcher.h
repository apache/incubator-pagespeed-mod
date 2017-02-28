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

#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

class AsyncFetch;
class MessageHandler;
class RewriteOptions;
class Statistics;
class ThreadSystem;

class RedirectFollowingUrlAsyncFetcher : public UrlAsyncFetcher {

 public:
  // Does not take ownership of 'fetcher'.
  // The context_url is needed for verifying that the url we are about to
  // redirect to is authorized.
  RedirectFollowingUrlAsyncFetcher(UrlAsyncFetcher* fetcher,
                                   const GoogleString& context_url,
                                   ThreadSystem* thread_system,
                                   Statistics* statistics, int max_redirects,
                                   bool follow_temp_redirects,
                                   RewriteOptions* rewrite_options,
                                   RewriteOptionsManager* rewrite_options_manager);

  virtual ~RedirectFollowingUrlAsyncFetcher();

  bool SupportsHttps() const override { return base_fetcher_->SupportsHttps(); }

  void Fetch(const GoogleString& url, MessageHandler* message_handler,
             AsyncFetch* fetch) override;

  // Returns the maximum number of redirects that will be followed.
  int max_redirects() { return max_redirects_; }
  // If set will follow temporary redirects (302 status code) when they are
  // marked as publically cacheable.
  bool follow_temp_redirects() { return follow_temp_redirects_; }
  const RewriteOptions* rewrite_options() { return rewrite_options_; }
  RewriteOptionsManager* rewrite_options_manager() { return rewrite_options_manager_; }
 private:
  static const int64 kUnset;

  class RedirectFollowingFetch;

  // Initiates a fetch of a pre-validated url originating from a Location
  // header of a response.
  void FollowRedirect(const GoogleString& valid_redirect_url,
                      MessageHandler* message_handler, AsyncFetch* fetch,
                      StringSet* redirects_followed_earlier, int64 max_age);
  UrlAsyncFetcher* base_fetcher_;
  // base url as stored on the request context.
  GoogleString context_url_;
  int max_redirects_;
  bool follow_temp_redirects_;
  const RewriteOptions* rewrite_options_;
  RewriteOptionsManager* rewrite_options_manager_;

  DISALLOW_COPY_AND_ASSIGN(RedirectFollowingUrlAsyncFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTTP_PUBLIC_REDIRECT_FOLLOWING_URL_ASYNC_FETCHER_H_
