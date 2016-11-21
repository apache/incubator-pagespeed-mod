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

#include "net/instaweb/http/public/redirect_following_url_async_fetcher.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

// TODO(oschaaf): inlining & intent should  be persisted across redirects.
namespace net_instaweb {

class RedirectFollowingUrlAsyncFetcher::RedirectFollowingFetch
    : public SharedAsyncFetch {
 public:
  RedirectFollowingFetch(
      RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher,
      AsyncFetch* base_fetch, const GoogleString& url,
      const GoogleString& context_url, MessageHandler* message_handler)
      : SharedAsyncFetch(base_fetch),
        redirect_following_fetcher_(redirect_following_fetcher),
        recieved_redirect_status_code_(false),
        urls_seen_(new GoogleUrlSet()),
        url_(url),
        gurl_(url),
        base_fetch_(base_fetch),
        context_url_(context_url),
        message_handler_(message_handler) {
    GoogleString sanitized = GoogleUrl::Sanitize(url);
    urls_seen_->insert(sanitized);
  }

  RedirectFollowingFetch(
      RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher,
      AsyncFetch* base_fetch, const GoogleString& url,
      const GoogleString& context_url, GoogleUrlSet* redirects_followed_earlier,
      MessageHandler* message_handler)
      : SharedAsyncFetch(base_fetch),
        redirect_following_fetcher_(redirect_following_fetcher),
        recieved_redirect_status_code_(false),
        urls_seen_(redirects_followed_earlier),
        url_(url),
        gurl_(url),
        base_fetch_(base_fetch),
        context_url_(context_url),
        message_handler_(message_handler) {}

  bool Validate() {
    if (!gurl_.IsWebValid()) {
      response_headers()->set_status_code(HttpStatus::kBadRequest);
      return false;
    }
    return true;
  }

 protected:
  virtual bool HandleFlush(MessageHandler* message_handler) {
    if (!recieved_redirect_status_code_) {
      return SharedAsyncFetch::HandleFlush(message_handler);
    }
    return true;
  }

  virtual void HandleHeadersComplete() {
    GoogleString h = response_headers()->ToString();
    // Currently we support permanent and temporary redirects.
    recieved_redirect_status_code_ =
        response_headers()->status_code() == HttpStatus::kMovedPermanently ||
        response_headers()->status_code() == HttpStatus::kFound;

    if (!recieved_redirect_status_code_) {
      SharedAsyncFetch::HandleHeadersComplete();
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
    DCHECK(gurl_.IsWebValid() || !success);
    const RewriteOptions* options =
        redirect_following_fetcher_->rewrite_options();

    if (!recieved_redirect_status_code_) {
      SharedAsyncFetch::HandleDone(success);
      delete this;
      return;
    }

    bool my_success = success;
    int redirects_followed = urls_seen_->size();

    if (my_success) {
      // TODO(oschaaf): does this have the correct arguments?
      bool cacheable = response_headers()->IsProxyCacheable(
          base_fetch_->request_headers()->GetProperties(),
          ResponseHeaders::GetVaryOption(options->respect_vary()),
          ResponseHeaders::kHasValidator);
      if (!cacheable) {
        LOG(WARNING) << "Not following uncacheable redirect: " << url_;
        my_success = false;
      }
    }

    const char* raw_redirect_url =
        response_headers()->Lookup1(HttpAttributes::kLocation);
    GoogleString redirect_url;

    if (!raw_redirect_url) {
      LOG(WARNING) << "Failed looking up exactly one Location: header " << url_;
      my_success = false;
    } else if (!strlen(raw_redirect_url)) {
      LOG(WARNING) << "Empty Location: header value";
      my_success = false;
    } else {
      redirect_url = GoogleUrl::Sanitize(raw_redirect_url);
      raw_redirect_url = redirect_url.c_str();
    }

    if (my_success && FindIgnoreCase(redirect_url, "#") != StringPiece::npos) {
      LOG(WARNING) << "Decline redirect to " << redirect_url
                   << " fragments are not supported.";
      my_success = false;
    }

    scoped_ptr<GoogleUrl> redirect_gurl;
    if (my_success) {
      redirect_gurl.reset(new GoogleUrl(gurl_, redirect_url));
      if (!redirect_gurl->IsWebValid()) {
        LOG(WARNING) << "Invalid or unsupported url in location header: "
                     << redirect_url;
        my_success = false;
      } else {
        redirect_url = redirect_gurl->spec_c_str();
      }
    }

    if (my_success) {
      if (redirects_followed > redirect_following_fetcher_->max_redirects()) {
        LOG(WARNING) << "Max redirects " << redirects_followed << " / "
                     << redirect_following_fetcher_->max_redirects()
                     << " exceedeed for " << redirect_url;
        my_success = false;
      }
    }

    if (my_success) {
      std::pair<GoogleUrlSet::iterator, bool> ret =
          urls_seen_->insert(redirect_url);
      if (!ret.second) {
        LOG(WARNING) << "Already followed " << redirect_url;
        my_success = false;
      }
    }

    const DomainLawyer* domain_lawyer = options->domain_lawyer();

    if (my_success &&
        !domain_lawyer->IsDomainAuthorized(GoogleUrl(context_url_),
                                           *redirect_gurl)) {
      LOG(WARNING) << "Unauthorized url: " << context_url_ << " -> "
                   << redirect_gurl->Spec();
      my_success = false;
    }
    if (my_success && !options->IsAllowed(redirect_gurl->Spec())) {
      LOG(WARNING) << "Rewriting disallowed for " << redirect_gurl->Spec();
      my_success = false;
    }

    if (my_success) {
      GoogleString mapped_domain_name;
      GoogleString host_header;
      bool is_proxy;
      bool mapped = domain_lawyer->MapOriginUrl(
          *redirect_gurl, &mapped_domain_name, &host_header, &is_proxy);

      if (mapped) {
        redirect_gurl->Reset(mapped_domain_name);
        redirect_url.assign(mapped_domain_name);
        if (redirect_gurl->SchemeIs("https") &&
            !redirect_following_fetcher_->SupportsHttps()) {
          LOG(WARNING) << "Can't follow redirect to https because https is not "
                          "supported: ' "
                       << redirect_url;
          my_success = false;
        }
        if (!is_proxy) {
          request_headers()->Replace(HttpAttributes::kHost, host_header);
        }
      } else {
        // Shouldn't happen
        DCHECK(false);
        LOG(WARNING) << "Invalid mapped url: " << redirect_gurl->Spec();
        my_success = false;
      }
    }

    // Wipe out the 3XX response. We'll either fail/404 or return 200/OK
    response_headers()->Clear();

    if (my_success) {
      redirect_following_fetcher_->FollowRedirect(
          redirect_url, message_handler_, base_fetch_, urls_seen_.release());
    } else {
      response_headers()->set_status_code(HttpStatus::kNotFound);
      SharedAsyncFetch::HandleDone(false);
    }

    delete this;
  }

 private:
  RedirectFollowingUrlAsyncFetcher* redirect_following_fetcher_;
  bool recieved_redirect_status_code_;
  scoped_ptr<GoogleUrlSet> urls_seen_;
  GoogleString url_;
  GoogleUrl gurl_;
  AsyncFetch* base_fetch_;
  const GoogleString context_url_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(RedirectFollowingFetch);
};

RedirectFollowingUrlAsyncFetcher::RedirectFollowingUrlAsyncFetcher(
    UrlAsyncFetcher* fetcher, GoogleString context_url,
    ThreadSystem* thread_system, Statistics* statistics, int max_redirects,
    RewriteOptions* rewrite_options)
    : base_fetcher_(fetcher),
      context_url_(context_url),
      max_redirects_(max_redirects),
      rewrite_options_(rewrite_options) {
  CHECK(rewrite_options);
}

RedirectFollowingUrlAsyncFetcher::~RedirectFollowingUrlAsyncFetcher() {}

void RedirectFollowingUrlAsyncFetcher::FollowRedirect(
    const GoogleString& url, MessageHandler* message_handler, AsyncFetch* fetch,
    GoogleUrlSet* redirects_followed_earlier) {
  RedirectFollowingFetch* redirect_following_fetch =
      new RedirectFollowingFetch(this, fetch, url, context_url_,
                                 redirects_followed_earlier, message_handler);

  if (redirect_following_fetch->Validate()) {
    base_fetcher_->Fetch(url, message_handler, redirect_following_fetch);

  } else {
    message_handler->Message(
        kWarning, "Decline following of bad redirect url: %s", url.c_str());
    LOG(WARNING) << "Decline following of bad redirect url: " << url;
    redirect_following_fetch->Done(false);
  }
}

void RedirectFollowingUrlAsyncFetcher::Fetch(const GoogleString& url,
                                             MessageHandler* message_handler,
                                             AsyncFetch* fetch) {
  RedirectFollowingFetch* redirect_following_fetch = new RedirectFollowingFetch(
      this, fetch, url, context_url_, message_handler);

  if (redirect_following_fetch->Validate()) {
    base_fetcher_->Fetch(url, message_handler, redirect_following_fetch);
  } else {
    LOG(WARNING) << "Decline fetching of bad url: " << url << std::endl;
    redirect_following_fetch->Done(false);
  }
}

}  // namespace net_instaweb
