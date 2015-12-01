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

#include "net/instaweb/http/public/redirect_following_url_async_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

#include <iostream>

namespace net_instaweb {

class RedirectFollowingUrlAsyncFetcher::RedirectFollowingFetch : public SharedAsyncFetch {
 public:
  RedirectFollowingFetch(RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher_, AsyncFetch* base_fetch,
    const GoogleString& url)
        : SharedAsyncFetch(base_fetch), redirect_following_fetcher_(redirect_following_fetcher_), recieved_redirect_status_code_(false),
          redirects_followed_earlier_count_(0), redirects_followed_earlier_(new GoogleUrlSet()), url_(url), gurl_(url), base_fetch_(base_fetch) {
    if (!gurl_.IsWebValid()) {
      Done(false);
      return;
    }            
    // TODO(oschaaf): do we need to validate the url here and below? 
    // TODO: we should normalize before inserting into the set here and alter on.
    GoogleString sanitized = GoogleUrl::Sanitize(url);
    redirects_followed_earlier_->insert(sanitized);
  }

  RedirectFollowingFetch(RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher_, AsyncFetch* base_fetch,
    const GoogleString& url, int redirect_count, GoogleUrlSet* redirects_followed_earlier)
        : SharedAsyncFetch(base_fetch), redirect_following_fetcher_(redirect_following_fetcher_), recieved_redirect_status_code_(false),
          redirects_followed_earlier_count_(redirect_count), redirects_followed_earlier_(redirects_followed_earlier), url_(url), gurl_(url), base_fetch_(base_fetch) {
  }

 protected:
  virtual bool HandleFlush(MessageHandler* message_handler) {
      if (!recieved_redirect_status_code_) {
        return SharedAsyncFetch::HandleFlush(message_handler);
      }
      return true;
  }

  virtual void HandleHeadersComplete() {
    {
      GoogleString h = response_headers()->ToString();
      //std::cout  << "Headers Complete( " << this << " " << url_ << ") ";// << ": " << h << std::endl;
      
      recieved_redirect_status_code_ = response_headers()->status_code() == HttpStatus::kMovedPermanently;

      if (!recieved_redirect_status_code_) {
        SharedAsyncFetch::HandleHeadersComplete();
      }
    }
  }


  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    if (!recieved_redirect_status_code_) {
      return SharedAsyncFetch::HandleWrite(content, handler);
    }
    return true;
  }

  virtual void HandleDone(bool success) {    
    // TODO(oschaaf): double check the assumption that url_ must be valid.
    DCHECK(gurl_.IsWebValid()) << "Should not have been constructed with an invalid url '" << url_ << "'";

    if (!recieved_redirect_status_code_) {
      SharedAsyncFetch::HandleDone(success);
      delete this;
      return;
    }

    bool my_success = success;
    int redirects_followed = redirects_followed_earlier_count_;
    const char* raw_redirect_url = response_headers()->Lookup1("Location");
    if (!raw_redirect_url) {
      std::cout  << "Failed looking up exactly one Location: header " << url_ << std::endl;
      my_success = false;
    }
    else if (!strlen(raw_redirect_url)) {
      std::cout  << "Empty Location: header value" << std::endl;
      my_success = false;
    }

    GoogleString redirect_url = GoogleUrl::Sanitize(raw_redirect_url);
    
    // TODO: check how this handles #fragments.
    // TODO: check https://tools.ietf.org/html/rfc3986#section-4.2
    // TODO: check https://tools.ietf.org/html/rfc3986#section-5
    scoped_ptr<GoogleUrl> redirect_gurl;
    if (my_success) {
      redirect_gurl.reset(new GoogleUrl(gurl_, redirect_url));
      // TODO(oschaaf): relative redirects, etc.
      if (!redirect_gurl->IsWebValid()) {
        std::cout  << "Invalid url in location header: " << redirect_url << std::endl;
        std::cout  << "Abs: " << redirect_gurl->spec_c_str() << std::endl;
        my_success = false;          
      } else {
        redirect_url = redirect_gurl->spec_c_str();
      }
    }    

    if (my_success) {
      if (redirects_followed >= redirect_following_fetcher_->max_redirects()) {
        std::cout  << "Max redirects " << redirects_followed_earlier_count_  << " / " << redirect_following_fetcher_->max_redirects() << " exceedeed for " << redirect_url << std::endl;
        my_success = false;
      } 
    }

    if (my_success) {
      std::pair<GoogleUrlSet::iterator, bool> ret = redirects_followed_earlier_->insert(redirect_url);
      if (!ret.second) {
        std::cout  << "Already followed " << redirect_url << std::endl;
        my_success = false;
      }
    }

    if (my_success) {        
      redirects_followed++;
      std::cout  << "Following redirect #" << redirects_followed << ": " << url_ << "->" << redirect_url << std::endl;
      redirect_following_fetcher_->FollowRedirect(redirect_url, NULL, 
          base_fetch_, redirects_followed, redirects_followed_earlier_.release());
    }
    
    if (redirects_followed_earlier_count_ == redirects_followed) {
      SharedAsyncFetch::HandleDone(my_success);
      delete this;
    }
  }

 private:
  RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher_;
  bool recieved_redirect_status_code_;
  int redirects_followed_earlier_count_;
  scoped_ptr<GoogleUrlSet> redirects_followed_earlier_;
  const GoogleString url_;
  const GoogleUrl gurl_;
  AsyncFetch* base_fetch_;

  //DISALLOW_COPY_AND_ASSIGN(RedirectFollowingFetch);
};

RedirectFollowingUrlAsyncFetcher::RedirectFollowingUrlAsyncFetcher(
    UrlAsyncFetcher* fetcher,
    ThreadSystem* thread_system,
    Statistics* statistics,
    int max_redirects)
    : base_fetcher_(fetcher), max_redirects_(max_redirects) {
}

RedirectFollowingUrlAsyncFetcher::~RedirectFollowingUrlAsyncFetcher() {
}

void RedirectFollowingUrlAsyncFetcher::FollowRedirect(const GoogleString& url,
                                           MessageHandler* message_handler,
                                           AsyncFetch* fetch, int count,
                                           GoogleUrlSet* redirects_followed_earlier) {
  RedirectFollowingFetch* redirect_following_fetch = new RedirectFollowingFetch(this, fetch, url, count, redirects_followed_earlier);
  base_fetcher_->Fetch(url, message_handler, redirect_following_fetch);
}

void RedirectFollowingUrlAsyncFetcher::Fetch(const GoogleString& url,
                                           MessageHandler* message_handler,
                                           AsyncFetch* fetch) {
  RedirectFollowingFetch* redirect_following_fetch = new RedirectFollowingFetch(this, fetch, url);
  base_fetcher_->Fetch(url, message_handler, redirect_following_fetch);
}

void RedirectFollowingUrlAsyncFetcher::ShutDown() {
  // TODO(oschaaf): remove?
}

}  // namespace net_instaweb
