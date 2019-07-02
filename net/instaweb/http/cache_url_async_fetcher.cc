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


#include "net/instaweb/http/public/cache_url_async_fetcher.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/async_fetch_with_lock.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/thread/sequence.h"
#include "pagespeed/opt/logging/log_record.h"  // for AbstractLogRecord


namespace net_instaweb {

// HTTP 501 Not Implemented: The server either does not recognize the
// request method, or it lacks the ability to fulfill the request.
const int CacheUrlAsyncFetcher::kNotInCacheStatus = HttpStatus::kNotImplemented;

namespace {

class CachePutFetch : public SharedAsyncFetch {
 public:
  CachePutFetch(const GoogleString& url, const GoogleString& fragment,
                AsyncFetch* base_fetch,
                ResponseHeaders::VaryOption respect_vary,
                bool default_cache_html, HTTPCache* cache,
                Histogram* backend_first_byte_latency, MessageHandler* handler)
      : SharedAsyncFetch(base_fetch),
        url_(url),
        fragment_(fragment),
        http_options_(request_context()->options()),
        respect_vary_(respect_vary),
        default_cache_html_(default_cache_html),
        cache_(cache),
        backend_first_byte_latency_(backend_first_byte_latency),
        handler_(handler),
        cacheable_(false),
        cache_value_writer_(&cache_value_, cache_),
        saved_headers_(http_options_),
        req_properties_(base_fetch->request_headers()->GetProperties()) {
    if (backend_first_byte_latency_ != NULL) {
      start_time_ms_ = cache_->timer()->NowMs();
    }
  }

  virtual ~CachePutFetch() {}

  virtual void HandleHeadersComplete() {
    // We compute the latency here as it's the spot where we're doing an
    // actual backend fetch and not potentially using the cache.
    int64 now_ms = cache_->timer()->NowMs();
    if (backend_first_byte_latency_ != NULL) {
      backend_first_byte_latency_->Add(now_ms - start_time_ms_);
    }
    ResponseHeaders* headers = response_headers();
    headers->FixDateHeaders(now_ms);
    bool is_html = headers->IsHtmlLike();
    const char* cache_control = headers->Lookup1(HttpAttributes::kCacheControl);
    if (default_cache_html_ && is_html &&
        // TODO(sligocki): Use some sort of computed
        // headers->HasExplicitCachingTtl() instead
        // of just checking for the existence of 2 headers.
        (cache_control == NULL || cache_control == StringPiece("public")) &&
        !headers->Has(HttpAttributes::kExpires)) {
      headers->Add(HttpAttributes::kCacheControl,
          "max-age=" + Integer64ToString(headers->implicit_cache_ttl_ms()));
    }
    headers->ComputeCaching();
    cacheable_ = headers->IsProxyCacheable(req_properties_, respect_vary_,
                                           ResponseHeaders::kHasValidator);
    if (cacheable_) {
      // Make a copy of the headers which we will send to the
      // cache_value_writer_ later.
      saved_headers_.CopyFrom(*headers);
    }

    SharedAsyncFetch::HandleHeadersComplete();
  }

  virtual bool HandleWrite(const StringPiece& content,
                           MessageHandler* handler) {
    bool ret = true;
    ret &= SharedAsyncFetch::HandleWrite(content, handler);
    if (cacheable_) {
      ret &= cache_value_writer_.Write(content, handler);
    }
    return ret;
  }

  virtual bool HandleFlush(MessageHandler* handler) {
    // Note cache_value_.Flush doesn't do anything.
    return SharedAsyncFetch::HandleFlush(handler);
  }

  virtual void HandleDone(bool success) {
    DCHECK_EQ(request_headers()->method(), RequestHeaders::kGet);
    // We do not cache empty 200 responses. (Empty 404, 500 are fine.)
    // https://github.com/apache/incubator-pagespeed-mod/issues/1050
    const bool empty_200 =
        (response_headers()->status_code() == HttpStatus::kOK &&
         cache_value_.contents_size() == 0);
    const bool insert_into_cache = (success &&
                                    cacheable_ &&
                                    !empty_200 &&
                                    cache_value_writer_.has_buffered());

    if (insert_into_cache) {
      // The X-Original-Content-Length header will have been added after
      // HandleHeadersComplete(), so extract its value and add it to the
      // saved headers.
      const char* orig_content_length = extra_response_headers()->Lookup1(
          HttpAttributes::kXOriginalContentLength);
      int64 ocl;
      if (orig_content_length != NULL &&
          StringToInt64(orig_content_length, &ocl)) {
        saved_headers_.SetOriginalContentLength(ocl);
      }
      // Finalize the headers.
      cache_value_writer_.SetHeaders(&saved_headers_);
    } else {
      // Set is_original_resource_cacheable.
      log_record()->SetIsOriginalResourceCacheable(false);
    }

    // Finish fetch.
    SharedAsyncFetch::HandleDone(success);
    // Note: SharedAsyncFetch::base_fetch_ and other things that refer to that,
    // like request_context() cannot be accessed or used any more.

    // Add result to cache.
    if (insert_into_cache) {
      cache_->Put(url_, fragment_, req_properties_, http_options_,
                  &cache_value_, handler_);
    }
    // Note: We explicitly do not remember fetch failure, uncacheable nor
    // empty resources here since we still want to proxy those through every
    // time they are requested.
    // TODO(sligocki): Maybe we should be remembering failures.
    delete this;
  }

 private:
  const GoogleString url_;
  const GoogleString fragment_;
  const HttpOptions http_options_;
  // TODO(sligocki): Remove and use http_options_.respect_vary instead.
  ResponseHeaders::VaryOption respect_vary_;
  bool default_cache_html_;
  HTTPCache* cache_;
  Histogram* backend_first_byte_latency_;
  MessageHandler* handler_;

  bool cacheable_;
  HTTPValue cache_value_;
  HTTPValueWriter cache_value_writer_;
  int64 start_time_ms_;  // only used if backend_first_byte_latency_ != NULL
  ResponseHeaders saved_headers_;
  RequestHeaders::Properties req_properties_;

  DISALLOW_COPY_AND_ASSIGN(CachePutFetch);
};

class CacheFindCallback : public HTTPCache::Callback {
 public:
  class BackgroundFreshenFetch : public AsyncFetchWithLock {
   public:
    explicit BackgroundFreshenFetch(
        const Hasher* lock_hasher,
        const RequestContextPtr& request_context,
        const GoogleString& url,
        NamedLockManager* lock_manager,
        MessageHandler* message_handler,
        CacheFindCallback* callback,
        CacheUrlAsyncFetcher::AsyncOpHooks* async_op_hooks)
        : AsyncFetchWithLock(
              lock_hasher, request_context, url, url /* cache_key*/,
              lock_manager, message_handler),
          callback_(callback),
          async_op_hooks_(async_op_hooks) {
      async_op_hooks_->StartAsyncOp();
    }

    virtual ~BackgroundFreshenFetch() {
      async_op_hooks_->FinishAsyncOp();
    }

    virtual void StartFetch(
        UrlAsyncFetcher* fetcher, MessageHandler* handler) {
      AsyncFetch* fetch = callback_->WrapCachePutFetchAndConditionalFetch(this);
      fetcher->Fetch(url(), handler, fetch);
    }

    virtual bool ShouldYieldToRedundantFetchInProgress() {
      return true;
    }

    virtual bool IsBackgroundFetch() const { return true; }

   private:
    CacheFindCallback* callback_;
    CacheUrlAsyncFetcher::AsyncOpHooks* async_op_hooks_;

    DISALLOW_COPY_AND_ASSIGN(BackgroundFreshenFetch);
  };

  CacheFindCallback(const Hasher* lock_hasher,
                    NamedLockManager* lock_manager,
                    const GoogleString& url,
                    const GoogleString& fragment,
                    AsyncFetch* base_fetch,
                    CacheUrlAsyncFetcher* owner,
                    CacheUrlAsyncFetcher::AsyncOpHooks* async_op_hooks,
                    MessageHandler* handler)
      : HTTPCache::Callback(base_fetch->request_context(),
                            base_fetch->request_headers()->GetProperties()),
        lock_hasher_(lock_hasher),
        lock_manager_(lock_manager),
        url_(url),
        base_fetch_(base_fetch),
        cache_(owner->http_cache()),
        fragment_(fragment),
        async_op_hooks_(async_op_hooks),
        fetcher_(owner->fetcher()),
        backend_first_byte_latency_(
            owner->backend_first_byte_latency_histogram()),
        fallback_responses_served_(owner->fallback_responses_served()),
        fallback_responses_served_while_revalidate_(
            owner->fallback_responses_served_while_revalidate()),
        num_conditional_refreshes_(owner->num_conditional_refreshes()),
        num_proactively_freshen_user_facing_request_(
            owner->num_proactively_freshen_user_facing_request()),
        handler_(handler),
        http_options_(base_fetch->request_context()->options()),
        respect_vary_(ResponseHeaders::GetVaryOption(owner->respect_vary())),
        ignore_recent_fetch_failed_(owner->ignore_recent_fetch_failed()),
        serve_stale_if_fetch_error_(owner->serve_stale_if_fetch_error()),
        default_cache_html_(owner->default_cache_html()),
        proactively_freshen_user_facing_request_(
            owner->proactively_freshen_user_facing_request()),
        serve_stale_while_revalidate_threshold_sec_(
            owner->serve_stale_while_revalidate_threshold_sec()) {
    // Note that this is a cache lookup: there are no request-headers.  At
    // this level, we have already made a policy decision that any Vary
    // headers present will be ignored.  See
    // https://modpagespeed.com/doc/configuration#respectvary
    set_response_headers(base_fetch->response_headers());
  }
  virtual ~CacheFindCallback() {}

  virtual void Done(HTTPCache::FindResult find_result) {
    if (response_sequence_ == NULL) {
      Finish(find_result);
    } else {
      response_sequence_->Add(MakeFunction(
          this, &CacheFindCallback::Finish, find_result));
    }
  }

  void Finish(HTTPCache::FindResult find_result) {
    switch (find_result.status) {
      case HTTPCache::kFound: {
        VLOG(1) << "Found in cache: " << url_ << " (" << fragment_ << ")";
        http_value()->ExtractHeaders(response_headers(), handler_);

        bool is_imminently_expiring = false;

        // Respond with a 304 if the If-Modified-Since / If-None-Match values
        // are equal to those in the request.
        if (ShouldReturn304()) {
          response_headers()->Clear();
          response_headers()->SetStatusAndReason(HttpStatus::kNotModified);
          response_headers()->ComputeCaching();
          is_imminently_expiring = IsImminentlyExpiring(*response_headers());
          base_fetch_->HeadersComplete();
        } else if (base_fetch_->request_headers()->method() !=
                   RequestHeaders::kHead) {
          DCHECK_EQ(base_fetch_->request_headers()->method(),
                    RequestHeaders::kGet);

          // Before calling HeadersComplete, record the content-length so that
          // http server gaskets have an opportunity to examine
          // content_length_known() in HandleHeadersComplete and thereby serve
          // non-chunked responses.
          StringPiece contents;
          http_value()->ExtractContents(&contents);
          base_fetch_->set_content_length(contents.size());
          response_headers()->ComputeCaching();
          is_imminently_expiring = IsImminentlyExpiring(*response_headers());
          base_fetch_->HeadersComplete();

          // TODO(sligocki): We are writing all the content in one shot, this
          // fact might be useful to the HtmlParser if this is HTML. Perhaps
          // we should add an API for conveying that information, which can
          // be detected via AsyncFetch::content_length_known().
          base_fetch_->Write(contents, handler_);
        } else {
          response_headers()->ComputeCaching();
          is_imminently_expiring = IsImminentlyExpiring(*response_headers());
        }

        if (fetcher_ != NULL &&
            proactively_freshen_user_facing_request_ &&
            async_op_hooks_ != NULL &&
            is_imminently_expiring) {
          // Triggers the background fetch to freshen the value in cache if
          // resource is about to expire.
          if (num_proactively_freshen_user_facing_request_ != NULL) {
            num_proactively_freshen_user_facing_request_->Add(1);
          }
          TriggerBackgroundFreshenFetch();
        }

        base_fetch_->Done(true);
        break;
      }
      // Note: currently no resources fetched through CacheUrlAsyncFetcher
      // will be marked RememberFetchFailedOrNotCacheable.
      // TODO(sligocki): Should we mark resources as such in this class?
      case HTTPCache::kRecentFailure:
        VLOG(1) << "RecentFetchFailed, NotCacheable or Empty: "
                << url_ << " (" << fragment_ << ")";
        if (!ignore_recent_fetch_failed_) {
          base_fetch_->Done(false);
          break;
        } else {
          // If we are ignoring advice of kRecentFetchFailedOrNotCacheable,
          // we will refetch the resource as we would for kNotFound.
          //
          // For example, we should do this for fetches that are being proxied.
          FALLTHROUGH_INTENDED;
        }
      case HTTPCache::kNotFound: {
        VLOG(1) << "Did not find in cache: "
                << url_ << " (" << fragment_ << ")";
        if (fetcher_ == NULL) {
          // Set status code to indicate reason we failed Fetch.
          DCHECK(!base_fetch_->headers_complete());
          base_fetch_->response_headers()->set_status_code(
              CacheUrlAsyncFetcher::kNotInCacheStatus);
          base_fetch_->Done(false);
        } else {
          AsyncFetch* base_fetch = base_fetch_;
          if (request_headers()->method() == RequestHeaders::kGet) {
            // Only cache GET results as they can be used for HEAD requests,
            // but not vice versa.
            // TODO(gee): It is possible to cache HEAD results as well, but we
            // must add code to ensure we do not serve GET requests using HEAD
            // responses.
            if (ServedStaleContentWhileRevalidate(base_fetch)) {
              // Serve stale content while revalidate in the background.
              break;
            }
            if (serve_stale_if_fetch_error_) {
              // If fallback_http_value() is populated, use it in case the
              // fetch fails. Note that this is only populated if the
              // response in cache is stale.
              FallbackSharedAsyncFetch* fallback_fetch =
                  new FallbackSharedAsyncFetch(
                      base_fetch_, fallback_http_value(), handler_);
              fallback_fetch->set_fallback_responses_served(
                  fallback_responses_served_);
              base_fetch = fallback_fetch;
            }

            base_fetch = WrapCachePutFetchAndConditionalFetch(base_fetch);
          }

          fetcher_->Fetch(url_, handler_, base_fetch);
        }
        break;
      }
    }

    delete this;
  }

  virtual bool IsCacheValid(const GoogleString& key,
                            const ResponseHeaders& headers) {
    // base_fetch_ already has the key (URL + fragment).
    return base_fetch_->IsCachedResultValid(headers);
  }

  virtual ResponseHeaders::VaryOption RespectVaryOnResources() const {
    return respect_vary_;
  }

  void set_response_sequence(Sequence* sequence) {
    response_sequence_ = sequence;
  }

 private:
  bool ServedStaleContentWhileRevalidate(AsyncFetch* base_fetch) {
    if (serve_stale_while_revalidate_threshold_sec_ == 0 ||
        fallback_http_value() == NULL ||
        fallback_http_value()->Empty()) {
      return false;
    }
    ResponseHeaders* response_headers = base_fetch->response_headers();
    if (!fallback_http_value()->ExtractHeaders(response_headers, handler_)) {
      // Returns false if it fails to extract headers.
      response_headers->Clear();
      return false;
    }
    response_headers->ComputeCaching();
    const int64 expiry_ms = response_headers->CacheExpirationTimeMs();
    const int64 now_ms = cache_->timer()->NowMs();
    const int64 serve_stale_threshold_ms =
        serve_stale_while_revalidate_threshold_sec_ * 1000;
    if (now_ms > expiry_ms +  serve_stale_threshold_ms ||
        response_headers->IsHtmlLike()) {
      // Serve non-html request with fallback http value if resource
      // was expired within serve_stale_while_revalidate_threshold_ms_.
      response_headers->Clear();
      return false;
    }
    if (fallback_responses_served_while_revalidate_ != NULL) {
      fallback_responses_served_while_revalidate_->Add(1);
    }
    // CacheControl header is changed to private, max-age=0 to avoid caching
    // of the resource either by browser or intermediate proxy as stale
    // content should be served only for this request, any future requests
    // should be served with fresh content.
    response_headers->Replace(HttpAttributes::kCacheControl,
                              "private, max-age=0");
    response_headers->RemoveAll(HttpAttributes::kExpires);
    response_headers->ComputeCaching();
    base_fetch_->HeadersComplete();
    StringPiece contents;
    fallback_http_value()->ExtractContents(&contents);
    base_fetch_->Write(contents, handler_);

    // Issue a background fetch to update the cache with a fresh value so
    // that future request will be responded with fresh content.
    TriggerBackgroundFreshenFetch();
    base_fetch_->Done(true);
    return true;
  }

  void TriggerBackgroundFreshenFetch() {
    AsyncFetchWithLock* fetch = new BackgroundFreshenFetch(
        lock_hasher_,
        base_fetch_->request_context(),
        url_,
        lock_manager_,
        handler_,
        this,
        async_op_hooks_);
    RequestHeaders* request_headers = fetch->request_headers();
    request_headers->CopyFrom(*base_fetch_->request_headers());
    DCHECK(request_headers->method() == RequestHeaders::kGet ||
           request_headers->method() == RequestHeaders::kHead);
    // It's possible for us to trigger a background freshen on a HEAD.
    // If so, actually send the GET request, since we don't want to be
    // trying to cache a HEAD response.
    request_headers->set_method(RequestHeaders::kGet);
    fetch->Start(fetcher_);
  }

  bool ShouldReturn304() const {
    if (ConditionalHeadersMatch(HttpAttributes::kIfNoneMatch,
                                HttpAttributes::kEtag)) {
      // If the Etag matches, return a 304.
      return true;
    }
    // Otherwise, return a 304 only if there was no If-None-Match header in the
    // request and the last modified timestamp matches.
    // (from http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html)
    return request_headers()->Lookup1(HttpAttributes::kIfNoneMatch) == NULL &&
        ConditionalHeadersMatch(HttpAttributes::kIfModifiedSince,
                                HttpAttributes::kLastModified);
  }

  bool ConditionalHeadersMatch(const StringPiece& request_header,
                               const StringPiece& response_header) const {
    const char* request_header_value =
        request_headers()->Lookup1(request_header);
    const char* response_header_value =
        response_headers()->Lookup1(response_header);
    return request_header_value != NULL && response_header_value != NULL &&
        strcmp(request_header_value, response_header_value) == 0;
  }

  const RequestHeaders* request_headers() const {
    return base_fetch_->request_headers();
  }
  RequestHeaders* request_headers() { return base_fetch_->request_headers(); }

  bool IsImminentlyExpiring(const ResponseHeaders& headers) const {
    return ResponseHeaders::IsImminentlyExpiring(
        headers.date_ms(),
        headers.CacheExpirationTimeMs(),
        cache_->timer()->NowMs(),
        headers.http_options());
  }

  AsyncFetch* WrapCachePutFetchAndConditionalFetch(AsyncFetch* base_fetch) {
    CachePutFetch* put_fetch = new CachePutFetch(
        url_, fragment_, base_fetch, respect_vary_, default_cache_html_, cache_,
        backend_first_byte_latency_, handler_);
    DCHECK_EQ(response_headers(), base_fetch_->response_headers());

    // Remove any Etags added by us before sending the request out. This is the
    // etags generated by server and upstream original code would not
    // understand them.
    const char* etag = request_headers()->Lookup1(
        HttpAttributes::kIfNoneMatch);
    if (etag != NULL &&
        StringCaseStartsWith(etag, HTTPCache::kEtagPrefix)) {
      put_fetch->request_headers()->RemoveAll(
          HttpAttributes::kIfNoneMatch);
    }

    ConditionalSharedAsyncFetch* conditional_fetch =
        new ConditionalSharedAsyncFetch(
            put_fetch, fallback_http_value(), handler_);
    conditional_fetch->set_num_conditional_refreshes(
        num_conditional_refreshes_);
    return conditional_fetch;
  }

  const Hasher* lock_hasher_;
  NamedLockManager* lock_manager_;
  const GoogleString url_;
  RequestHeaders request_headers_;
  AsyncFetch* base_fetch_;
  HTTPCache* cache_;
  GoogleString fragment_;
  CacheUrlAsyncFetcher::AsyncOpHooks* async_op_hooks_;
  UrlAsyncFetcher* fetcher_;
  Histogram* backend_first_byte_latency_;
  Variable* fallback_responses_served_;
  Variable* fallback_responses_served_while_revalidate_;
  Variable* num_conditional_refreshes_;
  Variable* num_proactively_freshen_user_facing_request_;
  MessageHandler* handler_;

  const HttpOptions http_options_;
  // TODO(sligocki): Remove and use http_options_.respect_vary instead.
  ResponseHeaders::VaryOption respect_vary_;
  bool ignore_recent_fetch_failed_;
  bool serve_stale_if_fetch_error_;
  bool default_cache_html_;
  bool proactively_freshen_user_facing_request_;
  int64 serve_stale_while_revalidate_threshold_sec_;
  Sequence* response_sequence_;

  DISALLOW_COPY_AND_ASSIGN(CacheFindCallback);
};

}  // namespace

CacheUrlAsyncFetcher::CacheUrlAsyncFetcher(const Hasher* lock_hasher,
                                           NamedLockManager* lock_manager,
                                           HTTPCache* cache,
                                           const GoogleString& fragment,
                                           AsyncOpHooks* async_op_hooks,
                                           UrlAsyncFetcher* fetcher)
    : lock_hasher_(lock_hasher),
      lock_manager_(lock_manager),
      http_cache_(cache),
      fragment_(fragment),
      fetcher_(fetcher),
      async_op_hooks_(async_op_hooks),
      backend_first_byte_latency_(NULL),
      fallback_responses_served_(NULL),
      fallback_responses_served_while_revalidate_(NULL),
      num_conditional_refreshes_(NULL),
      num_proactively_freshen_user_facing_request_(NULL),
      respect_vary_(false),
      ignore_recent_fetch_failed_(false),
      serve_stale_if_fetch_error_(false),
      default_cache_html_(false),
      proactively_freshen_user_facing_request_(false),
      own_fetcher_(false),
      serve_stale_while_revalidate_threshold_sec_(0),
      response_sequence_(NULL) {
}

CacheUrlAsyncFetcher::~CacheUrlAsyncFetcher() {
  if (own_fetcher_) {
    delete fetcher_;
    fetcher_ = NULL;
  }
}

void CacheUrlAsyncFetcher::Fetch(
    const GoogleString& url, MessageHandler* handler, AsyncFetch* base_fetch) {
  switch (base_fetch->request_headers()->method()) {
    case RequestHeaders::kHead:
      // HEAD is identical to GET, with the body trimmed.  Even though we are
      // able to respond to HEAD requests with a cached value from a GET
      // response, at this point we do not allow caching of HEAD responses from
      // the origin, so mark the "original" resource as uncacheable.
      base_fetch->log_record()->SetIsOriginalResourceCacheable(false);
      FALLTHROUGH_INTENDED;
    case RequestHeaders::kGet:
      {
        CacheFindCallback* find_callback =
            new CacheFindCallback(
                lock_hasher_,
                lock_manager_,
                url,
                fragment_,
                base_fetch,
                this,
                async_op_hooks_,
                handler);
        find_callback->set_response_sequence(response_sequence_);
        http_cache_->Find(url, fragment_, handler, find_callback);
      }
      return;

    default:
      // POST may not be idempotent and thus we must not serve a cached value
      // from a prior request.
      // TODO(gee): What about the other methods?
      break;
  }

  // Original resource not cacheable.
  base_fetch->log_record()->SetIsOriginalResourceCacheable(false);
  if (fetcher_ != NULL) {
    fetcher_->Fetch(url, handler, base_fetch);
  } else {
    // Set status code to indicate reason we failed Fetch.
    DCHECK(!base_fetch->headers_complete());
    base_fetch->response_headers()->set_status_code(kNotInCacheStatus);
    base_fetch->Done(false);
  }
}

CacheUrlAsyncFetcher::AsyncOpHooks::~AsyncOpHooks() {
}

}  // namespace net_instaweb
