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


#include "net/instaweb/rewriter/public/rewrite_driver.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <list>
#include <map>
#include <new>
#include <set>
#include <utility>  // for std::pair
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/add_head_filter.h"
#include "net/instaweb/rewriter/public/add_ids_filter.h"
#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"
#include "net/instaweb/rewriter/public/base_tag_filter.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/collect_dependencies_filter.h"
#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/critical_css_beacon_filter.h"
#include "net/instaweb/rewriter/public/critical_images_beacon_filter.h"
#include "net/instaweb/rewriter/public/critical_selector_filter.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/css_combine_filter.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"
#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"
#include "net/instaweb/rewriter/public/css_outline_filter.h"
#include "net/instaweb/rewriter/public/css_summarizer_base.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/debug_filter.h"
#include "net/instaweb/rewriter/public/decode_rewritten_urls_filter.h"
#include "net/instaweb/rewriter/public/dedup_inlined_images_filter.h"
#include "net/instaweb/rewriter/public/defer_iframe_filter.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/dependency_tracker.h"
#include "net/instaweb/rewriter/public/deterministic_js_filter.h"
#include "net/instaweb/rewriter/public/dom_stats_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/downstream_cache_purger.h"
#include "net/instaweb/rewriter/public/file_input_resource.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/fix_reflow_filter.h"
#include "net/instaweb/rewriter/public/flush_html_filter.h"
#include "net/instaweb/rewriter/public/google_analytics_filter.h"
#include "net/instaweb/rewriter/public/google_font_css_inline_filter.h"
#include "net/instaweb/rewriter/public/handle_noscript_redirect_filter.h"
#include "net/instaweb/rewriter/public/image_combine_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/in_place_rewrite_context.h"
#include "net/instaweb/rewriter/public/insert_amp_link_filter.h"
#include "net/instaweb/rewriter/public/insert_dns_prefetch_filter.h"
#include "net/instaweb/rewriter/public/insert_ga_filter.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/js_combine_filter.h"
#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"
#include "net/instaweb/rewriter/public/js_disable_filter.h"
#include "net/instaweb/rewriter/public/js_inline_filter.h"
#include "net/instaweb/rewriter/public/js_outline_filter.h"
#include "net/instaweb/rewriter/public/lazyload_images_filter.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/make_show_ads_async_filter.h"
#include "net/instaweb/rewriter/public/meta_tag_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/pedantic_filter.h"
#include "net/instaweb/rewriter/public/property_cache_util.h"
#include "net/instaweb/rewriter/public/push_preload_filter.h"
#include "net/instaweb/rewriter/public/redirect_on_size_limit_filter.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/responsive_image_filter.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewritten_content_scanning_filter.h"
#include "net/instaweb/rewriter/public/scan_filter.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/strip_scripts_filter.h"
#include "net/instaweb/rewriter/public/strip_subresource_hints_filter.h"
#include "net/instaweb/rewriter/public/support_noscript_filter.h"
#include "net/instaweb/rewriter/public/url_input_resource.h"
#include "net/instaweb/rewriter/public/url_left_trim_filter.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/request_trace.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/sha1_signature.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/html/amp_document_filter.h"
#include "pagespeed/kernel/html/collapse_whitespace_filter.h"
#include "pagespeed/kernel/html/elide_attributes_filter.h"
#include "pagespeed/kernel/html/html_attribute_quote_removal.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/html/remove_comments_filter.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/thread/scheduler_sequence.h"
#include "pagespeed/kernel/util/statistics_logger.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

class RewriteDriverPool;

namespace {

const int kTestTimeoutMs = 10000;
const char kDeadlineExceeded[] = "deadline_exceeded";

// Implementation of RemoveCommentsFilter::OptionsInterface that wraps
// a RewriteOptions instance.
class RemoveCommentsFilterOptions
    : public RemoveCommentsFilter::OptionsInterface {
 public:
  explicit RemoveCommentsFilterOptions(const RewriteOptions* options)
      : options_(options) {
  }

  virtual bool IsRetainedComment(const StringPiece& comment) const {
    return options_->IsRetainedComment(comment);
  }

 private:
  const RewriteOptions* options_;

  DISALLOW_COPY_AND_ASSIGN(RemoveCommentsFilterOptions);
};

// Provides hook to CacheUrlAsyncFetcher to protect the lifetime of the
// RewriteDriver which owns fetcher, otherwise, fetcher may be deleted
// by the time background fetch completes.
class RewriteDriverCacheUrlAsyncFetcherAsyncOpHooks
    : public CacheUrlAsyncFetcher::AsyncOpHooks {
 public:
  explicit RewriteDriverCacheUrlAsyncFetcherAsyncOpHooks(
      RewriteDriver* rewrite_driver) : rewrite_driver_(rewrite_driver) {
  }

  virtual ~RewriteDriverCacheUrlAsyncFetcherAsyncOpHooks() {
  }

  // TODO(pulkitg): Remove session fetchers, so that fetcher can live as long
  // server is alive and there is no need of
  // {Increment/Decrement}AsyncEventsCount().
  virtual void StartAsyncOp() {
    // Increment async_events_counts so that driver will be alive as long as
    // background fetch happens in CacheUrlAsyncFetcher.
    rewrite_driver_->IncrementAsyncEventsCount();
  }

  virtual void FinishAsyncOp() {
    rewrite_driver_->DecrementAsyncEventsCount();
  }

 private:
  RewriteDriver* rewrite_driver_;

  DISALLOW_COPY_AND_ASSIGN(RewriteDriverCacheUrlAsyncFetcherAsyncOpHooks);
};

}  // namespace

const char RewriteDriver::kDomCohort[] = "dom";
const char RewriteDriver::kBeaconCohort[] = "beacon_cohort";
const char RewriteDriver::kDependenciesCohort[] = "dependencies_cohort";
const char RewriteDriver::kSubresourcesPropertyName[] = "subresources";
const char RewriteDriver::kStatusCodePropertyName[] = "status_code";

const char RewriteDriver::kLastRequestTimestamp[] = "last_request_timestamp";
const char RewriteDriver::kParseSizeLimitExceeded[] =
    "parse_size_limit_exceeded";

int RewriteDriver::initialized_count_ = 0;

RewriteDriver::RewriteDriver(MessageHandler* message_handler,
                             FileSystem* file_system,
                             UrlAsyncFetcher* url_async_fetcher)
    : HtmlParse(message_handler),
      base_was_set_(false),
      refs_before_base_(false),
      other_base_problem_(false),
      filters_added_(false),
      externally_managed_(false),
      ref_counts_(this),
      release_driver_(false),
      waiting_(kNoWait),
      waiting_deadline_reached_(false),
      fully_rewrite_on_flush_(false),
      fast_blocking_rewrite_(true),
      flush_requested_(false),
      flush_occurred_(false),
      is_lazyload_script_flushed_(false),
      write_property_cache_dom_cohort_(false),
      should_skip_parsing_(kNotSet),
      response_headers_(NULL),
      status_code_(HttpStatus::kUnknownStatusCode),
      max_page_processing_delay_ms_(-1),
      num_initiated_rewrites_(0),
      num_detached_rewrites_(0),
      possibly_quick_rewrites_(0),
      file_system_(file_system),
      server_context_(NULL),
      scheduler_(NULL),
      default_url_async_fetcher_(url_async_fetcher),
      url_async_fetcher_(default_url_async_fetcher_),
      dom_stats_filter_(NULL),
      scan_filter_(this),
      controlling_pool_(NULL),
      cache_url_async_fetcher_async_op_hooks_(
          new RewriteDriverCacheUrlAsyncFetcherAsyncOpHooks(this)),
      html_worker_(NULL),
      rewrite_worker_(NULL),
      low_priority_rewrite_worker_(NULL),
      writer_(NULL),
      fallback_property_page_(NULL),
      owns_property_page_(false),
      device_type_(UserAgentMatcher::kDesktop),
      xhtml_mimetype_computed_(false),
      xhtml_status_(kXhtmlUnknown),
      num_inline_preview_images_(0),
      num_bytes_in_(0),
      debug_filter_(NULL),
      can_rewrite_resources_(true),
      is_nested_(false),
      request_context_(NULL),
      start_time_ms_(0),
      defer_instrumentation_script_(false),
      is_amp_(false),
      downstream_cache_purger_(this)
      // NOTE:  Be sure to clear per-request member variables in Clear()
{ // NOLINT  -- I want the initializer-list to end with that comment.
  // The Scan filter always goes first so it can find base-tags.
  early_pre_render_filters_.push_back(&scan_filter_);

  dependency_tracker_.reset(new DependencyTracker(this));
}

void RewriteDriver::PopulateRequestContext() {
  if ((request_context_.get() != NULL && (request_headers_ != NULL))) {
    request_context_->SetAcceptsWebp(
        request_properties_->SupportsWebpRewrittenUrls());
    request_context_->SetAcceptsGzip(request_properties_->AcceptsGzip());
    request_context_->Freeze();
  }
}

void RewriteDriver::SetRequestHeaders(const RequestHeaders& headers) {
  DCHECK(request_headers_.get() == NULL);
  RequestHeaders* new_request_headers = new RequestHeaders();
  new_request_headers->CopyFrom(headers);
  new_request_headers->PopulateLazyCaches();
  request_headers_.reset(new_request_headers);
  ClearRequestProperties();

  const char* user_agent = request_headers_->Lookup1(
      HttpAttributes::kUserAgent);
  if (user_agent != NULL) {
    user_agent_ = user_agent;
    request_properties_->SetUserAgent(user_agent_);
  }

  request_properties_->ParseRequestHeaders(*request_headers_);
  PopulateRequestContext();
}

void RewriteDriver::set_request_context(const RequestContextPtr& x) {
  // Ideally, we would have a CHECK(x.get() != NULL) here, since all "real"
  // RewriteDrivers should have a valid request context.
  //
  // However, one use-case currently prevent this --
  // ServerContext::InitWorkersAndDecodingDriver() creates a new driver
  // to decode options. This creation, via NewUnmanagedRewriteDriver(), invokes
  // this method with the provided request context, which really should be NULL
  // because it is not associated with a request.
  //
  // In lieu of the significant refactor required to move option decoding out
  // of RewriteDriver or synthesizing a context, we allow NULL here, and opt
  // to instead CHECK aggressively on code paths that really should have a
  // request context; i.e., those necessarily associated with page serving
  // rather than option decoding.
  request_context_.reset(x);
  if (request_context_.get() != NULL) {
    request_context_->log_record()->SetRewriterInfoMaxSize(
        options()->max_rewrite_info_log_size());
    request_context_->log_record()->SetAllowLoggingUrls(
        options()->allow_logging_urls_in_log_record());
    request_context_->log_record()->SetLogUrlIndices(
        options()->log_url_indices());
    PopulateRequestContext();
  }
}

AbstractLogRecord* RewriteDriver::log_record() {
  CHECK(request_context_.get() != NULL);
  return request_context_->log_record();
}

RewriteDriver::~RewriteDriver() {
  if (rewrite_worker_ != NULL) {
    scheduler_->UnregisterWorker(rewrite_worker_);
    server_context_->rewrite_workers()->FreeSequence(rewrite_worker_);
  }
  if (html_worker_ != NULL) {
    scheduler_->UnregisterWorker(html_worker_);
    server_context_->html_workers()->FreeSequence(html_worker_);
  }
  if (low_priority_rewrite_worker_ != NULL) {
    scheduler_->UnregisterWorker(low_priority_rewrite_worker_);
    server_context_->low_priority_rewrite_workers()->FreeSequence(
        low_priority_rewrite_worker_);
  }
  Clear();
  STLDeleteElements(&filters_to_delete_);
  STLDeleteElements(&resource_claimants_);
}

RewriteDriver* RewriteDriver::Clone() {
  RewriteDriver* result;
  RewriteDriverPool* pool = controlling_pool();
  if (pool == NULL) {
    // TODO(jmarantz): when used with SetParent, it should not be
    // necessary to clone the options here.  Once we set the child's
    // parent to this, the child will reference this->options() and
    // ignores its self_options_.  To exploit that, we'd need to
    // make a different entry-point for CloneAndSetParent.
    RewriteOptions* options_copy = options()->Clone();
    options_copy->ComputeSignature();
    result =
        server_context_->NewCustomRewriteDriver(options_copy, request_context_);
  } else {
    result = server_context_->NewRewriteDriverFromPool(pool, request_context_);
  }
  result->is_nested_ = true;

  // Remove any Via headers for the nested driver.  This is intended for
  // removing "Via:1.1 google", so that nested drivers don't wind up
  // adding cc:public into intermediate cached results.
  //
  // Note that we *do* want to propagate http/2 detection to nested drivers.
  // This is OK because it gets captured in the RequestContext, which is
  // shared, and is not reconstructed from request-headers.
  RequestHeaders headers;
  headers.CopyFrom(*request_headers_);
  headers.RemoveAll(HttpAttributes::kVia);
  result->SetRequestHeaders(headers);

  return result;
}

void RewriteDriver::Clear() NO_THREAD_SAFETY_ANALYSIS {
  if (scheduler_sequence_.get() != NULL) {
    CleanupRequestThread();
  }

  HtmlParse::Clear();

  // If this was a fetch, fetch_rewrites_ may still hold a reference to a
  // RewriteContext.
  STLDeleteElements(&fetch_rewrites_);

  DCHECK(!flush_requested_);
  release_driver_ = false;
  downstream_cache_purger_.Clear();
  write_property_cache_dom_cohort_ = false;
  base_url_.Clear();
  DCHECK(!base_url_.IsAnyValid());
  decoded_base_url_.Clear();
  fetch_url_.clear();

  if (!server_context_->shutting_down()) {
    if (!externally_managed_) {
      ref_counts_.DCheckAllCountsZero();
    }
    DCHECK(primary_rewrite_context_map_.empty());
    DCHECK(initiated_rewrites_.empty());
    DCHECK(detached_rewrites_.empty());
    DCHECK(rewrites_.empty());
    DCHECK_EQ(0, possibly_quick_rewrites_);
  }
  xhtml_mimetype_computed_ = false;
  xhtml_status_ = kXhtmlUnknown;

  should_skip_parsing_ = kNotSet;
  max_page_processing_delay_ms_ = -1;
  request_headers_.reset(NULL);
  response_headers_ = NULL;
  status_code_ = 0;
  flush_requested_ = false;
  flush_occurred_ = false;
  defer_instrumentation_script_ = false;
  is_amp_ = false;
  executing_rewrite_tasks_.set_value(false);
  is_lazyload_script_flushed_ = false;
  base_was_set_ = false;
  refs_before_base_ = false;
  other_base_problem_ = false;
  containing_charset_.clear();
  fully_rewrite_on_flush_ = false;
  fast_blocking_rewrite_ = true;
  num_inline_preview_images_ = 0;
  num_bytes_in_ = 0;
  flush_early_info_.reset(NULL);
  can_rewrite_resources_ = true;
  is_nested_ = false;
  num_initiated_rewrites_ = 0;
  num_detached_rewrites_ = 0;
  if (request_context_.get() != NULL) {
    request_context_->WriteBackgroundRewriteLog();
    request_context_.reset(NULL);
  }
  start_time_ms_ = 0;

  critical_images_info_.reset(NULL);
  critical_selector_info_.reset(NULL);

  if (owns_property_page_) {
    delete fallback_property_page_;
  }
  fallback_property_page_ = NULL;
  origin_property_page_.reset();
  owns_property_page_ = false;
  device_type_ = UserAgentMatcher::kDesktop;
  pagespeed_query_params_.clear();
  pagespeed_option_cookies_.clear();

  // Reset to the default fetcher from any session fetcher
  // (as the request is over).
  url_async_fetcher_ = default_url_async_fetcher_;
  STLDeleteElements(&owned_url_async_fetchers_);
  ClearRequestProperties();
  user_agent_.clear();

  csp_context_.Clear();
}

// Must be called with rewrite_mutex() held.
bool RewriteDriver::RewritesComplete() const {
  // 3 kinds of rewrites triggered from HTML:
  bool no_pending_rewrites =
      (ref_counts_.QueryCountMutexHeld(kRefPendingRewrites) == 0);
  bool no_deleting_rewrites =
      (ref_counts_.QueryCountMutexHeld(kRefDeletingRewrites) == 0);
  bool no_detached_rewrites = detached_rewrites_.empty();
  DCHECK_EQ(static_cast<int>(detached_rewrites_.size()),
            ref_counts_.QueryCountMutexHeld(kRefDetachedRewrites));

  // And also user-facing fetches. Note that background fetches are handled
  // by IsDone separately.
  bool no_user_facing_fetch =
      (ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing) == 0);

  return no_pending_rewrites && no_deleting_rewrites && no_detached_rewrites &&
         no_user_facing_fetch;
}

void RewriteDriver::WaitForCompletion() {
  BoundedWaitFor(kWaitForCompletion, -1);
}

void RewriteDriver::WaitForShutDown() {
  BoundedWaitFor(kWaitForShutDown, -1);
}

void RewriteDriver::BoundedWaitFor(WaitMode mode, int64 timeout_ms) {
  SchedulerBlockingFunction wait(scheduler_);
  {
    ScopedMutex lock(rewrite_mutex());
    ref_counts_.AddRefMutexHeld(kRefUser);
    CheckForCompletionAsync(mode, timeout_ms, &wait);
  }
  wait.Block();
#ifndef NDEBUG
  {
    ScopedMutex lock(rewrite_mutex());
    CHECK_EQ(waiting_, kNoWait);
  }
#endif
  DropReference(kRefUser);
}

void RewriteDriver::CheckForCompletionAsync(WaitMode wait_mode,
                                            int64 timeout_ms,
                                            Function* done) {
  scheduler_->DCheckLocked();
  DCHECK_NE(kNoWait, wait_mode);
  DCHECK_EQ(kNoWait, waiting_);
  waiting_ = wait_mode;
  waiting_deadline_reached_ = false;

  int64 end_time_ms;
  if (timeout_ms <= 0) {
    end_time_ms = -1;  // Encodes unlimited
  } else {
    end_time_ms = server_context()->timer()->NowMs() + timeout_ms;
  }

  TryCheckForCompletion(wait_mode, end_time_ms, done);
}

void RewriteDriver::TryCheckForCompletion(WaitMode wait_mode, int64 end_time_ms,
                                          Function* done)
    NO_THREAD_SAFETY_ANALYSIS {
  scheduler_->DCheckLocked();
  int64 now_ms = server_context_->timer()->NowMs();
  int64 sleep_ms;
  if (end_time_ms < 0) {
    waiting_deadline_reached_ = false;  // Unlimited wait..
    sleep_ms = kTestTimeoutMs;
  } else {
    waiting_deadline_reached_ = (now_ms >= end_time_ms);
    if (waiting_deadline_reached_) {
      // If deadline is already reached if we keep going we will want to use
      // long sleeps since we expect to be woken up based on conditions.
      sleep_ms = kTestTimeoutMs;
    } else {
      sleep_ms = end_time_ms - now_ms;
    }
  }

  // Note that we may end up going past the deadline in order to make sure
  // that at least the metadata cache lookups have a chance to come in.
  if (!IsDone(wait_mode, waiting_deadline_reached_)) {
    scheduler_->TimedWaitMs(
        sleep_ms,
        MakeFunction(this, &RewriteDriver::TryCheckForCompletion,
                     wait_mode, end_time_ms, done));
  } else {
    // Done. Note that we may get deleted by our callback, so we have to
    // make sure to save the mutex pointer. The thread annotation can't deal
    // with this aliasing, hence the need for NO_THREAD_SAFETY_ANALYSIS above.
    AbstractMutex* mutex = rewrite_mutex();
    waiting_ = kNoWait;
    mutex->Unlock();
    done->CallRun();
    mutex->Lock();
  }
}

bool RewriteDriver::IsDone(WaitMode wait_mode, bool deadline_reached) {
  int async_events = ref_counts_.QueryCountMutexHeld(kRefAsyncEvents);
  if (async_events > 0 && WaitForPendingAsyncEvents(wait_mode)) {
    return false;
  }

  int render_blocking_async_events =
      ref_counts_.QueryCountMutexHeld(kRefRenderBlockingAsyncEvents);
  if (render_blocking_async_events > 0) {
    return false;
  }

  // Before deadline, we're happy only if we're 100% done.
  if (!deadline_reached) {
    bool have_background_fetch =
        (ref_counts_.QueryCountMutexHeld(kRefFetchBackground) != 0);
    return RewritesComplete() &&
           !((wait_mode == kWaitForShutDown) && have_background_fetch);
  } else {
    // When we've reached the deadline, if we're Render()'ing
    // we also give the jobs we can serve from cache a chance to finish
    // (so they always render).
    // We do not have to worry about possibly_quick_rewrites_ not being
    // incremented yet as jobs are only initiated from the HTML parse thread.
    if (wait_mode == kWaitForCachedRender) {
      return (possibly_quick_rewrites_ == 0);
    } else {
      return true;
    }
  }
}

void RewriteDriver::ExecuteFlushIfRequested() {
  if (flush_requested_) {
    Flush();
  }
}

void RewriteDriver::ExecuteFlushIfRequestedAsync(Function* callback) {
  if (flush_requested_) {
    FlushAsync(callback);
  } else {
    callback->CallRun();
  }
}

void RewriteDriver::Flush() {
  SchedulerBlockingFunction wait(scheduler_);
  FlushAsync(&wait);
  wait.Block();
  flush_requested_ = false;
}

void RewriteDriver::FlushAsync(Function* callback) {
  DCHECK(request_context_.get() != NULL);
  TraceLiteral("RewriteDriver::FlushAsync()");
  if (debug_filter_ != NULL) {
    debug_filter_->StartRender();
  }
  flush_requested_ = false;

  // Figure out which filters should be enabled and whether any enabled filter
  // can modify urls.
  DetermineFiltersBehavior();

  for (FilterList::iterator it = early_pre_render_filters_.begin();
      it != early_pre_render_filters_.end(); ++it) {
    HtmlFilter* filter = *it;
    if (filter->is_enabled()) {
      ApplyFilter(filter);
    }
  }
  for (FilterList::iterator it = pre_render_filters_.begin();
      it != pre_render_filters_.end(); ++it) {
    HtmlFilter* filter = *it;
    if (filter->is_enabled()) {
      ApplyFilter(filter);
    }
  }

  int num_rewrites = rewrites_.size();

  // Copy all of the RewriteContext* into the initiated_rewrites_ set
  // *before* initiating them, as we are doing this before we lock.
  // The RewriteThread can start mutating the initiated_rewrites_
  // set as soon as one is initiated.
  {
    // If not locked, this WRITE to initiated_rewrites_ can race with
    // locked READs of initiated_rewrites_ in RewriteComplete which
    // runs in the Rewrite thread.  Note that the DCHECK above, of
    // initiated_rewrites_.empty(), is a READ and it's OK to have
    // concurrent READs.
    ScopedMutex lock(rewrite_mutex());

    // Note that no actual resource Rewriting can occur until this point
    // is reached, where we initiate all the RewriteContexts.
    DCHECK(initiated_rewrites_.empty());

    DCHECK_EQ(ref_counts_.QueryCountMutexHeld(kRefPendingRewrites),
              num_rewrites);
    initiated_rewrites_.insert(rewrites_.begin(), rewrites_.end());
    num_initiated_rewrites_ += num_rewrites;

    // We must also start tasks while holding the lock, as otherwise a
    // successor task may complete and delete itself before we see if we
    // are the ones to start it.
    for (int i = 0; i < num_rewrites; ++i) {
      RewriteContext* rewrite_context = rewrites_[i];
      if (!rewrite_context->chained()) {
        rewrite_context->Initiate();
      }
    }
  }
  rewrites_.clear();

  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchBackground));
    Function* flush_async_done =
        MakeFunction(this, &RewriteDriver::QueueFlushAsyncDone,
                     num_rewrites, callback);
    if (fully_rewrite_on_flush_) {
      CheckForCompletionAsync(kWaitForCompletion, -1, flush_async_done);
    } else {
      int64 deadline = ComputeCurrentFlushWindowRewriteDelayMs();
      CheckForCompletionAsync(kWaitForCachedRender, deadline, flush_async_done);
    }
  }
}

int64 RewriteDriver::ComputeCurrentFlushWindowRewriteDelayMs() {
  int64 deadline = rewrite_deadline_ms();
  // If we've configured a max processing delay for the entire page, enforce
  // that limit here.
  if (max_page_processing_delay_ms_ > 0) {
    int64 ms_since_start =
        server_context_->timer()->NowMs() - start_time_ms_;
    int64 ms_remaining = max_page_processing_delay_ms_ - ms_since_start;
    // If the deadline for the current flush window (deadline) is less
    // than the overall time remaining (ms_remaining), we enforce the
    // per-flush window deadline. Otherwise, we wait for the overall
    // page deadline.
    //
    // In any case, we require a minimum value of 1 millisecond since
    // a value <= 0 implies an unlimited wait.
    deadline =
        std::max(std::min(ms_remaining, deadline), static_cast<int64>(1));
  }
  return deadline;
}

void RewriteDriver::QueueFlushAsyncDone(int num_rewrites, Function* callback) {
  html_worker_->Add(MakeFunction(this, &RewriteDriver::FlushAsyncDone,
                                 num_rewrites, callback));
}

void RewriteDriver::FlushAsyncDone(int num_rewrites, Function* callback) {
  DCHECK(request_context_.get() != NULL);
  TraceLiteral("RewriteDriver::FlushAsyncDone()");

  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_EQ(0, possibly_quick_rewrites_);
    int still_pending_rewrites =
        ref_counts_.QueryCountMutexHeld(kRefPendingRewrites);
    int completed_rewrites = num_rewrites - still_pending_rewrites;

    // If the output cache lookup came as a HIT in after the deadline, that
    // means that (a) we can't use the result and (b) we don't need
    // to re-initiate the rewrite since it was in fact in cache.  Hopefully
    // the cache system will respond to HIT by making the next HIT faster
    // so it meets our deadline.  In either case we will track with stats.
    //
    RewriteStats* stats = server_context_->rewrite_stats();
    stats->cached_output_hits()->Add(completed_rewrites);
    stats->cached_output_missed_deadline()->Add(still_pending_rewrites);
    {
      // Add completed_rewrites (from this flush window) to the logged value.
      ScopedMutex lock(log_record()->mutex());
      MetadataCacheInfo* metadata_log_info =
          log_record()->logging_info()->mutable_metadata_cache_info();
      metadata_log_info->set_num_rewrites_completed(
          metadata_log_info->num_rewrites_completed() + completed_rewrites);
    }

    // Detach all rewrites that are still outstanding, by moving them from
    // initiated_rewrites_ to detached_rewrites_; also notify them that they
    // will not be rendered.
    for (RewriteContextSet::iterator p = initiated_rewrites_.begin(),
              e = initiated_rewrites_.end(); p != e; ++p) {
      RewriteContext* rewrite_context = *p;

      // If debugging is enabled, annotate that we have missed our rewrite
      // deadline.
      if (options()->Enabled(RewriteOptions::kDebug)) {
        for (int i = 0, n = rewrite_context->num_slots(); i < n; ++i) {
          ResourceSlotPtr slot = rewrite_context->slot(i);
          GoogleString suffix;
          const char* id = rewrite_context->id();
          StringFilterMap::const_iterator p = resource_filter_map_.find(id);
          if (p != resource_filter_map_.end()) {
            RewriteFilter* filter = p->second;
            InsertDebugComment(DeadlineExceededMessage(filter->Name()),
                               slot->element());
          } else {
            InsertDebugComment(kDeadlineExceeded, slot->element());
          }
        }
      }
      rewrite_context->WillNotRender();
      detached_rewrites_.insert(rewrite_context);
      ++num_detached_rewrites_;
      ref_counts_.AddRefMutexHeld(kRefDetachedRewrites);
      ref_counts_.ReleaseRefMutexHeld(kRefPendingRewrites);
    }
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefPendingRewrites));
    initiated_rewrites_.clear();

    slots_.clear();
    inline_slots_.clear();
    inline_attribute_slots_.clear();
    for(auto c : srcset_collections_) {
      c->Detach();
    }
    srcset_collections_.clear();
  }

  // Notify all enabled pre-render filters that rendering is done.
  if (debug_filter_ != NULL) {
    debug_filter_->RenderDone();
  }

  for (FilterList::iterator it = early_pre_render_filters_.begin();
       it != early_pre_render_filters_.end(); ++it) {
    HtmlFilter* filter = *it;
    if (filter->is_enabled()) {
      filter->RenderDone();
    }
  }
  for (FilterList::iterator it = pre_render_filters_.begin();
       it != pre_render_filters_.end(); ++it) {
    HtmlFilter* filter = *it;
    if (filter->is_enabled()) {
      filter->RenderDone();
    }
  }

  // Run all the post-render filters, and clear the event queue.
  HtmlParse::Flush();
  flush_occurred_ = true;
  callback->CallRun();
}

GoogleString RewriteDriver::DeadlineExceededMessage(StringPiece filter_name) {
  return StrCat(kDeadlineExceeded, " for filter ", filter_name);
}

void RewriteDriver::Initialize() {
  ++initialized_count_;
  if (initialized_count_ == 1) {
    RewriteOptions::Initialize();
    ImageRewriteFilter::Initialize();
    CssFilter::Initialize();
  }
}

void RewriteDriver::InitStats(Statistics* statistics) {
  AddInstrumentationFilter::InitStats(statistics);
  CacheExtender::InitStats(statistics);
  CriticalCssBeaconFilter::InitStats(statistics);
  CriticalImagesBeaconFilter::InitStats(statistics);
  CssCombineFilter::InitStats(statistics);
  CssFilter::InitStats(statistics);
  CssInlineFilter::InitStats(statistics);
  CssInlineImportToLinkFilter::InitStats(statistics);
  CssMoveToHeadFilter::InitStats(statistics);
  CssSummarizerBase::InitStats(statistics);
  DedupInlinedImagesFilter::InitStats(statistics);
  DomainRewriteFilter::InitStats(statistics);
  GoogleAnalyticsFilter::InitStats(statistics);
  GoogleFontCssInlineFilter::InitStats(statistics);
  ImageCombineFilter::InitStats(statistics);
  ImageRewriteFilter::InitStats(statistics);
  InPlaceRewriteContext::InitStats(statistics);
  InsertGAFilter::InitStats(statistics);
  JavascriptFilter::InitStats(statistics);
  JsCombineFilter::InitStats(statistics);
  JsInlineFilter::InitStats(statistics);
  LocalStorageCacheFilter::InitStats(statistics);
  MakeShowAdsAsyncFilter::InitStats(statistics);
  MetaTagFilter::InitStats(statistics);
  RewriteContext::InitStats(statistics);
  UrlInputResource::InitStats(statistics);
  UrlLeftTrimFilter::InitStats(statistics);
}

void RewriteDriver::Terminate() {
  // Clean up statics.
  --initialized_count_;
  if (initialized_count_ == 0) {
    CssFilter::Terminate();
    ImageRewriteFilter::Terminate();
    RewriteOptions::Terminate();
  }
}

void RewriteDriver::SetServerContext(ServerContext* server_context)
    NO_THREAD_SAFETY_ANALYSIS {
  DCHECK(server_context_ == NULL);
  server_context_ = server_context;
  scheduler_ = server_context_->scheduler();
  ref_counts_.set_mutex(rewrite_mutex());
  set_timer(server_context->timer());
  rewrite_worker_ = server_context_->rewrite_workers()->NewSequence();
  html_worker_ = server_context_->html_workers()->NewSequence();
  low_priority_rewrite_worker_ =
      server_context_->low_priority_rewrite_workers()->NewSequence();
  scheduler_->RegisterWorker(rewrite_worker_);
  scheduler_->RegisterWorker(html_worker_);
  scheduler_->RegisterWorker(low_priority_rewrite_worker_);
  dependency_tracker_->SetServerContext(server_context);

  DCHECK(resource_filter_map_.empty());

  // Add the rewriting filters to the map unconditionally -- we may
  // need them to process resource requests due to a query-specific
  // 'rewriters' specification.  We still use the passed-in options
  // to determine whether they get added to the html parse filter chain.
  // Note: RegisterRewriteFilter takes ownership of these filters.
  CacheExtender* cache_extender = new CacheExtender(this);
  ImageCombineFilter* image_combiner = new ImageCombineFilter(this);
  ImageRewriteFilter* image_rewriter = new ImageRewriteFilter(this);

  RegisterRewriteFilter(new CssCombineFilter(this));
  RegisterRewriteFilter(
      new CssFilter(this, cache_extender, image_rewriter, image_combiner));
  RegisterRewriteFilter(new JavascriptFilter(this));
  RegisterRewriteFilter(new JsCombineFilter(this));
  RegisterRewriteFilter(image_rewriter);
  RegisterRewriteFilter(cache_extender);
  RegisterRewriteFilter(image_combiner);
  RegisterRewriteFilter(new LocalStorageCacheFilter(this));
  RegisterRewriteFilter(new JavascriptSourceMapFilter(this));

  // These filters are needed to rewrite and trim urls in modified CSS files.
  domain_rewriter_.reset(new DomainRewriteFilter(this, statistics()));
  url_trim_filter_.reset(new UrlLeftTrimFilter(this, statistics()));
}

PropertyCache::CohortVector RewriteDriver::GetCohortList(
    const PropertyCache* pcache, const RewriteOptions* options,
    const ServerContext* server_context) {
  bool need_deps = options->NeedsDependenciesCohort();
  PropertyCache::CohortVector filtered_cohorts;
  for (const PropertyCache::Cohort* cohort : pcache->GetAllCohorts()) {
    if (need_deps || cohort != server_context->dependencies_cohort()) {
      filtered_cohorts.push_back(cohort);
    }
  }
  return filtered_cohorts;
}

void RewriteDriver::PropertyCacheSetupDone() {
  dependency_tracker_->Start();
}

RequestTrace* RewriteDriver::trace_context() {
  return request_context_.get() == NULL ? NULL :
      request_context_->root_trace_context();
}

void RewriteDriver::TracePrintf(const char* fmt, ...) {
  if (trace_context() == NULL || !trace_context()->tracing_enabled()) {
    return;
  }
  va_list argp;
  va_start(argp, fmt);
  trace_context()->TraceVPrintf(fmt, argp);
  va_end(argp);
}

void RewriteDriver::TraceLiteral(const char* literal) {
  if (trace_context() == NULL || !trace_context()->tracing_enabled()) {
    return;
  }
  trace_context()->TraceLiteral(literal);
}

void RewriteDriver::TraceString(const GoogleString& s) {
  if (trace_context() == NULL || !trace_context()->tracing_enabled()) {
    return;
  }
  trace_context()->TraceString(s);
}

void RewriteDriver::AddFilters() {
  CHECK(html_writer_filter_ == NULL);
  CHECK(!filters_added_);
  server_context_->ComputeSignature(options_.get());
  filters_added_ = true;

  AddPreRenderFilters();
  AddPostRenderFilters();
}

void RewriteDriver::AddPreRenderFilters() {
  // This function defines the order that filters are run.  We document
  // in pagespeed.conf.template that the order specified in the conf
  // file does not matter, but we give the filters there in the order
  // they are actually applied, for the benefit of the understanding
  // of the site owner.  So if you change that here, change it in
  // install/common/pagespeed.conf.template as well.
  //
  // Also be sure to update the doc in net/instaweb/doc/docs/config_filters.ezt.
  //
  // Now process boolean options, which may include propagating non-boolean
  // and boolean parameter settings to filters.
  const RewriteOptions* rewrite_options = options();

  if (rewrite_options->flush_html()) {
    // Note that this does not get hooked into the normal html-parse
    // filter-chain as it gets run immediately after every call to
    // ParseText, possibly inducing the system to trigger a Flush
    // based on the content it sees.
    add_event_listener(new FlushHtmlFilter(this));
  }
  add_event_listener(new AmpDocumentFilter(this, NewPermanentCallback(
      this, &RewriteDriver::SetIsAmpDocument)));

  if (rewrite_options->Enabled(RewriteOptions::kComputeStatistics)) {
    dom_stats_filter_ = new DomStatsFilter(this);
    AddOwnedEarlyPreRenderFilter(dom_stats_filter_);
  }
  if (!rewrite_options->preserve_subresource_hints()) {
    AddOwnedEarlyPreRenderFilter(new StripSubresourceHintsFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDecodeRewrittenUrls)) {
    AddOwnedEarlyPreRenderFilter(new DecodeRewrittenUrlsFilter(this));
  }

  if (rewrite_options->Enabled(RewriteOptions::kResponsiveImages) &&
      rewrite_options->Enabled(RewriteOptions::kResizeImages)) {
    ResponsiveImageFirstFilter* resp_filter1 =
        new ResponsiveImageFirstFilter(this);
    AddOwnedEarlyPreRenderFilter(resp_filter1);

    ResponsiveImageSecondFilter* resp_filter2 =
        new ResponsiveImageSecondFilter(this, resp_filter1);
    AddOwnedPostRenderFilter(resp_filter2);
  }

  if (rewrite_options->RequiresAddHead()) {
    // Adds a filter that adds a 'head' section to html documents if
    // none found prior to the body.
    AddOwnedEarlyPreRenderFilter(new AddHeadFilter(
        this, rewrite_options->Enabled(RewriteOptions::kCombineHeads)));
  }
  if (rewrite_options->Enabled(RewriteOptions::kAddBaseTag)) {
    AddOwnedEarlyPreRenderFilter(new BaseTagFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kAddIds)) {
    AddOwnedEarlyPreRenderFilter(new AddIdsFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kStripScripts)) {
    // Experimental filter that blindly strips all scripts from a page.
    AppendOwnedPreRenderFilter(new StripScriptsFilter(this));
  }
  if (is_critical_images_beacon_enabled()) {
    // This filter should be enabled early, at least before image rewriting,
    // because it depends on seeing the original image URLs.
    AppendOwnedPreRenderFilter(new CriticalImagesBeaconFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kMakeShowAdsAsync)) {
    // We want this filter early in case we ever inline the loader JS.
    AppendOwnedPreRenderFilter(new MakeShowAdsAsyncFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineImportToLink) ||
      (!rewrite_options->Forbidden(RewriteOptions::kInlineImportToLink) &&
       (rewrite_options->Enabled(RewriteOptions::kPrioritizeCriticalCss) ||
        rewrite_options->Enabled(RewriteOptions::kComputeCriticalCss)))) {
    // If we're converting simple embedded CSS @imports into a href link
    // then we need to do that before any other CSS processing.
    AppendOwnedPreRenderFilter(new CssInlineImportToLinkFilter(this,
                                                               statistics()));
  }
  if (!rewrite_options->Enabled(RewriteOptions::kPrioritizeCriticalCss) &&
      // If we're inlining styles that resolved initially, skip outlining
      // css since that works against this.
      rewrite_options->Enabled(RewriteOptions::kOutlineCss)) {
    // Cut out inlined styles and make them into external resources.
    // This can only be called once and requires a server_context_ to be set.
    CHECK(server_context_ != NULL);
    AppendOwnedPreRenderFilter(new CssOutlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineGoogleFontCss)) {
    // Inline small Google Font Service CSS files.
    // Do this before MoveCssToHead / MoveCssAboveScripts.
    AppendOwnedPreRenderFilter(new GoogleFontCssInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kMoveCssToHead) ||
      rewrite_options->Enabled(RewriteOptions::kMoveCssAboveScripts)) {
    // It's good to move CSS links to the head prior to running CSS combine,
    // which only combines CSS links that are already in the head.
    AppendOwnedPreRenderFilter(new CssMoveToHeadFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineCss)) {
    // Combine external CSS resources after we've outlined them.
    // CSS files in html document.  This can only be called
    // once and requires a server_context_ to be set.
    EnableRewriteFilter(RewriteOptions::kCssCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteCss) ||
      (!rewrite_options->Forbidden(RewriteOptions::kRewriteCss) &&
       FlattenCssImportsEnabled())) {
    // Since AddFilters only applies to the HTML rewrite path, we check here
    // if IPRO preemptive rewrites are disabled and skip the filter if so.
    if (!rewrite_options->css_preserve_urls() ||
        rewrite_options->in_place_preemptive_rewrite_css()) {
      EnableRewriteFilter(RewriteOptions::kCssFilterId);
    }
  }
  if ((rewrite_options->Enabled(RewriteOptions::kPrioritizeCriticalCss) &&
       server_context()->factory()->UseBeaconResultsInFilters()) ||
      rewrite_options->Enabled(RewriteOptions::kComputeCriticalCss)) {
    // Add the critical selector instrumentation before the rewriting filter.
    AppendOwnedPreRenderFilter(new CriticalCssBeaconFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kPrioritizeCriticalCss)) {
    AppendOwnedPreRenderFilter(new CriticalSelectorFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInlineCss)) {
    // Inline small CSS files.  Give CSS minification and flattening a chance to
    // run before we decide what counts as "small".
    CHECK(server_context_ != NULL);
    AppendOwnedPreRenderFilter(new CssInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kOutlineJavascript)) {
    // Cut out inlined scripts and make them into external resources.
    // This can only be called once and requires a server_context_ to be set.
    CHECK(server_context_ != NULL);
    AppendOwnedPreRenderFilter(new JsOutlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kMakeGoogleAnalyticsAsync)) {
    // Converts sync loads of Google Analytics javascript to async loads.
    // This needs to be listed before rewrite_javascript because it injects
    // javascript that has comments and extra whitespace.
    AppendOwnedPreRenderFilter(new GoogleAnalyticsFilter(this, statistics()));
  }
  if ((rewrite_options->Enabled(RewriteOptions::kInsertGA) ||
       rewrite_options->running_experiment()) &&
      rewrite_options->ga_id() != "") {
    // Like MakeGoogleAnalyticsAsync, InsertGA should be before js rewriting.
    AppendOwnedPreRenderFilter(new InsertGAFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCombineJavascript)) {
    // Combine external JS resources. Done after minification and analytics
    // detection, as it converts script sources into string literals, making
    // them opaque to analysis.
    EnableRewriteFilter(RewriteOptions::kJavascriptCombinerId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kRewriteJavascriptExternal) ||
      rewrite_options->Enabled(RewriteOptions::kRewriteJavascriptInline) ||
      rewrite_options->Enabled(
          RewriteOptions::kCanonicalizeJavascriptLibraries)) {
    // Since AddFilters only applies to the HTML rewrite path, we check here
    // if IPRO preemptive rewrites are disabled and skip the filter if so.
    //
    // Note that we minify before we inline, so if you enable
    // rewrite_javascript_inline but not rewrite_javascript_external, we
    // will only minify the already-inlined JavaScript, and we will not
    // minify external JS that we decided later to inline.  It seems unlikely
    // that someone would want to enable inline_javascript and not enable
    // rewrite_javascript_external though.
    if (!rewrite_options->js_preserve_urls() ||
        rewrite_options->in_place_preemptive_rewrite_javascript() ||
        rewrite_options->Enabled(RewriteOptions::kRewriteJavascriptInline)) {
      // Rewrite (minify etc.) JavaScript code to reduce time to first
      // interaction.
      EnableRewriteFilter(RewriteOptions::kJavascriptMinId);
    }
  }

  if (rewrite_options->Enabled(RewriteOptions::kInlineJavascript)) {
    // Inline small Javascript files.  Give JS minification a chance to run
    // before we decide what counts as "small".
    CHECK(server_context_ != NULL);
    AppendOwnedPreRenderFilter(new JsInlineFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kConvertJpegToProgressive) ||
      rewrite_options->ImageOptimizationEnabled() ||
      rewrite_options->Enabled(RewriteOptions::kResizeImages) ||
      rewrite_options->Enabled(
          RewriteOptions::kResizeToRenderedImageDimensions) ||
      rewrite_options->Enabled(RewriteOptions::kInlineImages) ||
      rewrite_options->Enabled(RewriteOptions::kInsertImageDimensions) ||
      rewrite_options->Enabled(RewriteOptions::kJpegSubsampling) ||
      rewrite_options->Enabled(RewriteOptions::kStripImageColorProfile) ||
      rewrite_options->Enabled(RewriteOptions::kStripImageMetaData) ||
      rewrite_options->Enabled(RewriteOptions::kDelayImages)) {
    // Since AddFilters only applies to the HTML rewrite path, we check here
    // if IPRO preemptive rewrites are disabled and skip the filter if so.
    if (!rewrite_options->image_preserve_urls() ||
        rewrite_options->in_place_preemptive_rewrite_images()) {
      EnableRewriteFilter(RewriteOptions::kImageCompressionId);
    }
  }
  if (rewrite_options->Enabled(RewriteOptions::kRemoveComments)) {
    AppendOwnedPreRenderFilter(new RemoveCommentsFilter(
        this, new RemoveCommentsFilterOptions(rewrite_options)));
  }
  if (rewrite_options->Enabled(RewriteOptions::kElideAttributes)) {
    // Remove HTML element attribute values where
    // http://www.w3.org/TR/html4/loose.dtd says that the name is all
    // that's necessary
    AppendOwnedPreRenderFilter(new ElideAttributesFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kExtendCacheCss) ||
      rewrite_options->Enabled(RewriteOptions::kExtendCacheImages) ||
      rewrite_options->Enabled(RewriteOptions::kExtendCachePdfs) ||
      rewrite_options->Enabled(RewriteOptions::kExtendCacheScripts)) {
    // Extend the cache lifetime of resources.
    EnableRewriteFilter(RewriteOptions::kCacheExtenderId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kSpriteImages)) {
    EnableRewriteFilter(RewriteOptions::kImageCombineId);
  }
  if (rewrite_options->Enabled(RewriteOptions::kLocalStorageCache)) {
    EnableRewriteFilter(RewriteOptions::kLocalStorageCacheId);
  }

  if (options()->NeedsDependenciesCohort()) {
    AppendOwnedPreRenderFilter(new CollectDependenciesFilter(this));
  }
}

void RewriteDriver::AddPostRenderFilters() {
  const RewriteOptions* rewrite_options = options();
  if (rewrite_options->Enabled(RewriteOptions::kFlushSubresources) &&
      !options()->pre_connect_url().empty()) {
    AddOwnedPostRenderFilter(new RewrittenContentScanningFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kInsertDnsPrefetch)) {
    InsertDnsPrefetchFilter* insert_dns_prefetch_filter =
        new InsertDnsPrefetchFilter(this);
    AddOwnedPostRenderFilter(insert_dns_prefetch_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kInsertAmpLink)) {
    InsertAmpLinkFilter* insert_amp_link_filter = new InsertAmpLinkFilter(this);
    AddOwnedPostRenderFilter(insert_amp_link_filter);
  }
  if (rewrite_options->Enabled(RewriteOptions::kAddInstrumentation)) {
    // Inject javascript to instrument loading-time. This should run before
    // defer js so that its onload handler can fire before JS starts executing.
    AddOwnedPostRenderFilter(new AddInstrumentationFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDeferJavascript)) {
    // Defers javascript download and execution to post onload. This filter
    // should be applied before JsDisableFilter and JsDeferFilter.
    // kDeferIframe filter should never be turned on when either defer_js
    // or disable_js is enabled.
    AddOwnedPostRenderFilter(new DeferIframeFilter(this));
    AddOwnedPostRenderFilter(new JsDisableFilter(this));
    // Though we are adding JsDeferDisabledFilter here, if we are flushing
    // cached html or we have flushed cached html, this filter will disable
    // itself.
    AddOwnedPostRenderFilter(new JsDeferDisabledFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kFixReflows)) {
    AddOwnedPostRenderFilter(new FixReflowFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDeterministicJs)) {
    AddOwnedPostRenderFilter(new DeterministicJsFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kConvertMetaTags)) {
    AddOwnedPostRenderFilter(new MetaTagFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDisableJavascript)) {
    // kDeferIframe filter should never be turned on when either defer_js
    // or disable_js is enabled.
    AddOwnedPostRenderFilter(new DeferIframeFilter(this));
    AddOwnedPostRenderFilter(new JsDisableFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDelayImages)) {
    // kInsertImageDimensions should be enabled to avoid drastic reflows.
    AddOwnedPostRenderFilter(new DelayImagesFilter(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kDedupInlinedImages)) {
    AddOwnedPostRenderFilter(new DedupInlinedImagesFilter(this));
  }
  // TODO(nikhilmadan): Should we disable this for bots?
  // LazyLoadImagesFilter should be applied after DelayImagesFilter.
  if (rewrite_options->Enabled(RewriteOptions::kLazyloadImages)) {
    AddOwnedPostRenderFilter(new LazyloadImagesFilter(this));
  }
  if (rewrite_options->support_noscript_enabled()) {
    AddOwnedPostRenderFilter(new SupportNoscriptFilter(this));
  }

  if (rewrite_options->Enabled(RewriteOptions::kHandleNoscriptRedirect)) {
    AddOwnedPostRenderFilter(new HandleNoscriptRedirectFilter(this));
  }

  if (rewrite_options->max_html_parse_bytes() > 0) {
    AddOwnedPostRenderFilter(new RedirectOnSizeLimitFilter(this));
    set_size_limit(rewrite_options->max_html_parse_bytes());
  }

  if (rewrite_options->Enabled(RewriteOptions::kPedantic)) {
    // Add HTML type attributes where HTML4 says that it's necessary.
    PedanticFilter* filter = new PedanticFilter(this);
    AddOwnedPostRenderFilter(filter);
  }
  // All filters that might add urls should come before the domain rewriter,
  // so they'll get rewritten.
  if (rewrite_options->domain_lawyer()->can_rewrite_domains() &&
      rewrite_options->Enabled(RewriteOptions::kRewriteDomains)) {
    // Rewrite mapped domains and shard any resources not otherwise rewritten.
    // We want do do this after all the content-changing rewrites, because they
    // will map & shard as part of their execution.
    //
    // TODO(jmarantz): Consider removing all the domain-mapping functionality
    // from other rewrites and do it exclusively in this filter.  Before we
    // do that we'll need to validate this filter so we can turn it on by
    // default.
    //
    // Note that the "domain_lawyer" filter controls whether we rewrite
    // domains for resources in HTML files.  However, when we cache-extend
    // CSS files, we rewrite the domains in them whether this filter is
    // specified or not.
    AddUnownedPostRenderFilter(domain_rewriter_.get());
  }
  if (rewrite_options->Enabled(RewriteOptions::kLeftTrimUrls)) {
    // Trim extraneous prefixes from urls in attribute values.
    // Happens before RemoveQuotes but after everything else.  Note:
    // we Must left trim urls BEFORE quote removal.
    AddUnownedPostRenderFilter(url_trim_filter_.get());
  }
  // Remove quotes and collapse whitespace at the very end for maximum effect.
  if (rewrite_options->Enabled(RewriteOptions::kRemoveQuotes)) {
    // Remove extraneous quotes from html attributes.
    AddOwnedPostRenderFilter(new HtmlAttributeQuoteRemoval(this));
  }
  if (rewrite_options->Enabled(RewriteOptions::kCollapseWhitespace)) {
    // Remove excess whitespace in HTML.
    AddOwnedPostRenderFilter(new CollapseWhitespaceFilter(this));
  }
  if (options()->Enabled(RewriteOptions::kHintPreloadSubresources)) {
    AppendOwnedPreRenderFilter(new PushPreloadFilter(this));
  }

  if (DebugMode()) {
    debug_filter_ = new DebugFilter(this);
    AddOwnedPostRenderFilter(debug_filter_);
  }

  // NOTE(abliss): Adding a new filter?  Does it export any statistics?  If it
  // doesn't, it probably should.  If it does, be sure to add it to the
  // InitStats() function above or it will break under Apache!
}

void RewriteDriver::AddOwnedEarlyPreRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  early_pre_render_filters_.push_back(filter);
}

void RewriteDriver::PrependOwnedPreRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  pre_render_filters_.push_front(filter);
}

void RewriteDriver::AppendOwnedPreRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::AppendUnownedPreRenderFilter(HtmlFilter* filter) {
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::AddOwnedPostRenderFilter(HtmlFilter* filter) {
  filters_to_delete_.push_back(filter);
  AddUnownedPostRenderFilter(filter);
}

void RewriteDriver::AddUnownedPostRenderFilter(HtmlFilter* filter) {
  HtmlParse::AddFilter(filter);
}

void RewriteDriver::AppendRewriteFilter(RewriteFilter* filter) {
  CHECK(filter != NULL);
  RegisterRewriteFilter(filter);
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::PrependRewriteFilter(RewriteFilter* filter) {
  CHECK(filter != NULL);
  RegisterRewriteFilter(filter);
  pre_render_filters_.push_front(filter);
}

void RewriteDriver::AddResourceUrlClaimant(ResourceUrlClaimant* claimant) {
  CHECK(claimant != NULL);
  resource_claimants_.push_back(claimant);
}

void RewriteDriver::EnableRewriteFilter(const char* id) {
  RewriteFilter* filter = resource_filter_map_[id];
  CHECK(filter != NULL);
  pre_render_filters_.push_back(filter);
}

void RewriteDriver::RegisterRewriteFilter(RewriteFilter* filter) {
  // Track resource_fetches if we care about statistics.  Note that
  // the statistics are owned by the server context, which generally
  // should be set up prior to the rewrite_driver.
  //
  // TODO(sligocki): It'd be nice to get this into the constructor.
  resource_filter_map_[filter->id()] = filter;
  filters_to_delete_.push_back(filter);
}

void RewriteDriver::SetWriter(Writer* writer) {
  writer_ = writer;
  if (html_writer_filter_ == NULL) {
    html_writer_filter_.reset(new HtmlWriterFilter(this));
    html_writer_filter_->set_case_fold(options()->lowercase_html_names());
    if (options()->Enabled(RewriteOptions::kHtmlWriterFilter)) {
      HtmlParse::AddFilter(html_writer_filter_.get());
    }
  }

  html_writer_filter_->set_writer(writer);
}

Statistics* RewriteDriver::statistics() const {
  return (server_context_ == NULL) ? NULL : server_context_->statistics();
}

void RewriteDriver::SetSessionFetcher(UrlAsyncFetcher* f) {
  url_async_fetcher_ = f;
  owned_url_async_fetchers_.push_back(f);
}

CacheUrlAsyncFetcher* RewriteDriver::CreateCustomCacheFetcher(
    UrlAsyncFetcher* base_fetcher) {
  return server_context()->CreateCustomCacheFetcher(
      options(), CacheFragment(), cache_url_async_fetcher_async_op_hooks_.get(),
      base_fetcher);
}

CacheUrlAsyncFetcher* RewriteDriver::CreateCacheFetcher() {
  return CreateCustomCacheFetcher(url_async_fetcher_);
}

CacheUrlAsyncFetcher* RewriteDriver::CreateCacheOnlyFetcher() {
  CacheUrlAsyncFetcher* fetcher = CreateCustomCacheFetcher(NULL);
  if (scheduler_sequence_.get() != NULL) {
    fetcher->set_response_sequence(scheduler_sequence_.get());
  }
  return fetcher;
}

bool RewriteDriver::Decode(StringPiece leaf,
                           ResourceNamer* resource_namer) const {
  return resource_namer->Decode(
      leaf, server_context()->hasher()->HashSizeInChars(), SignatureLength());
}

int RewriteDriver::SignatureLength() const {
  return options()->url_signing_key().empty()
             ? 0
             : options()->sha1signature()->SignatureSizeInChars();
}

bool RewriteDriver::DecodeOutputResourceNameHelper(
    const GoogleUrl& gurl,
    const RewriteOptions* options_to_use,
    const UrlNamer* url_namer,
    ResourceNamer* namer_out,
    OutputResourceKind* kind_out,
    RewriteFilter** filter_out,
    GoogleString* url_base,
    StringVector* urls) const {
  // In forward proxy in preserve-URLs mode we want to fetch .pagespeed.
  // resource, i.e. do not decode and and do not fetch original (especially
  // that encoded one will never be cached internally).
  if (options_to_use != NULL && options_to_use->oblivious_pagespeed_urls()) {
    return false;
  }

  // First, we can't handle anything that's not a valid URL nor is named
  // properly as our resource.
  if (!gurl.IsWebValid()) {
    return false;
  }

  StringPiece name = gurl.LeafSansQuery();
  if (!Decode(name, namer_out)) {
    return false;
  }

  // URLs without any hash are rejected as well, as they do not produce
  // OutputResources with a computable URL. (We do accept 'wrong' hashes since
  // they could come up legitimately under some asynchrony scenarios)
  if (namer_out->hash().empty()) {
    return false;
  }

  GoogleString decoded_url;
  // If we are running in full proxy mode we need to ignore URLs where the leaf
  // is encoded but the URL as a whole isn't proxy encoded, since that can
  // happen when proxying from a server using mod_pagespeed.
  //
  // This is also important for XSS avoidance when running in proxy mode with
  // a relaxed lawyer, as it ensures that resources will only ever go under
  // the low-privilege proxy domain and not the trusted site domain.
  //
  // For input-only mode, we can't do this, however, as URLs we produce aren't
  // proxy encoded, and we need to be able to fetch (and therefore decode)
  // our own urls.
  //
  // TODO(morlovich): Send out PageSpeed = off header in features that use it
  // (measurement proxy?) to avoid the dual-pagespeed issue?
  //
  // If we are running in proxy mode and the URL is in the proxy domain, we
  // also need to ensure that the URL decodes correctly as otherwise we end
  // up with an invalid decoded base URL, which ultimately leads to inability
  // to rewrite the URL.
  UrlNamer::ProxyExtent proxy_mode = url_namer->ProxyMode();
  if (proxy_mode == UrlNamer::ProxyExtent::kFull ||
      (proxy_mode == UrlNamer::ProxyExtent::kInputOnly &&
       url_namer->IsProxyEncoded(gurl))) {
    if (!url_namer->IsProxyEncoded(gurl)) {
      message_handler()->Message(kInfo,
                                 "Decoding of resource name %s failed because "
                                 "it is not proxy encoded.",
                                 gurl.spec_c_str());
      return false;
    } else if (!url_namer->Decode(gurl, options_to_use, &decoded_url)) {
      message_handler()->Message(kInfo,
                                 "Decoding of resource name %s failed because "
                                 " the URL namer cannot decode it.",
                                 gurl.spec_c_str());
      return false;
    }
    GoogleUrl decoded_gurl(decoded_url);
    if (decoded_gurl.IsWebValid()) {
      *url_base = (decoded_gurl.AllExceptLeaf()).as_string();
    } else {
      return false;
    }
  } else {
    *url_base = (gurl.AllExceptLeaf()).as_string();
  }

  // Now let's reject as mal-formed if the id string is not
  // in the rewrite drivers. Also figure out the filter's preferred
  // resource kind.
  StringPiece id = namer_out->id();
  GoogleString id_str(id.data(), id.size());
  *kind_out = kRewrittenResource;
  StringFilterMap::const_iterator p = resource_filter_map_.find(id_str);
  if (p != resource_filter_map_.end()) {
    *filter_out = p->second;
    if ((*filter_out)->ComputeOnTheFly()) {
      *kind_out = kOnTheFlyResource;
    }
  } else if ((id == CssOutlineFilter::kFilterId) ||
             (id == JsOutlineFilter::kFilterId)) {
    // OutlineFilter is special because it's not a RewriteFilter -- it's
    // just an HtmlFilter, but it does encode rewritten resources that
    // must be served from the cache.
    //
    // TODO(jmarantz): figure out a better way to refactor this.
    // TODO(jmarantz): add a unit-test to show serving outline-filter resources.
    *kind_out = kOutlinedResource;
    *filter_out = NULL;
  } else {
    message_handler()->Message(kInfo,
                               "Decoding of resource name %s failed because "
                               " there is no filter with id %s.",
                               gurl.spec_c_str(), id_str.c_str());
    return false;
  }

  // Check if filter-specific decoding works as well.
  // TODO(morlovich): This is doing some redundant work.
  if (*filter_out != NULL) {
    ResourceContext resource_context;
    if (!(*filter_out)->encoder()->Decode(
            namer_out->name(), urls, &resource_context, message_handler())) {
      message_handler()->Message(kInfo,
                                 "Decoding of resource name %s failed because "
                                 " filter %s cannot decode the URL.",
                                 gurl.spec_c_str(), (*filter_out)->Name());
      return false;
    }
  }

  // Check if the id string's filter is forbidden and reject the URL if so.
  if (options_to_use->Forbidden(id_str)) {
    message_handler()->Message(kInfo,
                               "Decoding of resource name %s failed because "
                               " filter_id %s is forbidden.",
                               gurl.spec_c_str(), id_str.c_str());
    return false;
  }

  return true;
}

bool RewriteDriver::DecodeOutputResourceName(
    const GoogleUrl& gurl,
    const RewriteOptions* options_to_use,
    const UrlNamer* url_namer,
    ResourceNamer* namer_out,
    OutputResourceKind* kind_out,
    RewriteFilter** filter_out) const {
  StringVector urls;
  GoogleString url_base;
  return DecodeOutputResourceNameHelper(
      gurl, options_to_use, url_namer, namer_out, kind_out,
      filter_out, &url_base, &urls);
}

bool RewriteDriver::DecodeUrl(const GoogleUrl& url,
                              StringVector* decoded_urls) const {
  return DecodeUrlGivenOptions(url, options(),
                               server_context()->url_namer(), decoded_urls);
}

bool RewriteDriver::DecodeUrlGivenOptions(
    const GoogleUrl& url,
    const RewriteOptions* options,
    const UrlNamer* url_namer,
    StringVector* decoded_urls) const {
  ResourceNamer namer;
  OutputResourceKind kind;
  RewriteFilter* filter = NULL;
  GoogleString url_base;
  bool is_decoded =  DecodeOutputResourceNameHelper(
      url, options, url_namer, &namer, &kind, &filter, &url_base, decoded_urls);
  if (is_decoded) {
    GoogleUrl gurl_base(url_base);
    for (int i = 0, n = decoded_urls->size(); i < n; ++i) {
      GoogleUrl full_url(gurl_base, (*decoded_urls)[i]);
      (*decoded_urls)[i] = full_url.Spec().as_string();
    }
  }
  return is_decoded;
}

OutputResourcePtr RewriteDriver::DecodeOutputResource(
    const GoogleUrl& gurl,
    RewriteFilter** filter) const {
  ResourceNamer namer;
  OutputResourceKind kind;
  if (!DecodeOutputResourceName(gurl, options(), server_context()->url_namer(),
                                &namer, &kind, filter)) {
    return OutputResourcePtr();
  }

  StringPiece base = gurl.AllExceptLeaf();
  OutputResourcePtr output_resource(
      new OutputResource(this, base, base, base, namer, kind));
  if (!output_resource.get()->CheckSignature()) {
    output_resource.clear();
  }
  return output_resource;
}

namespace {

class FilterFetch : public SharedAsyncFetch {
 public:
  FilterFetch(RewriteDriver* driver, AsyncFetch* async_fetch)
      : SharedAsyncFetch(async_fetch),
        driver_(driver) {
  }
  virtual ~FilterFetch() {}

  static bool Start(RewriteFilter* filter,
                    const OutputResourcePtr& output_resource,
                    AsyncFetch* async_fetch,
                    MessageHandler* handler) {
    RewriteDriver* driver = filter->driver();
    FilterFetch* filter_fetch = new FilterFetch(driver, async_fetch);

    bool queued = false;
    RewriteContext* context = filter->MakeRewriteContext();
    DCHECK(context != NULL);
    if (context != NULL) {
      queued = context->Fetch(output_resource, filter_fetch, handler);
    }
    if (!queued) {
      RewriteStats* stats = driver->server_context()->rewrite_stats();
      stats->failed_filter_resource_fetches()->Add(1);
      async_fetch->Done(false);
      driver->FetchComplete();
      delete filter_fetch;
    }
    return queued;
  }

 protected:
  virtual void HandleDone(bool success) {
    RewriteStats* stats = driver_->server_context()->rewrite_stats();
    if (success) {
      stats->succeeded_filter_resource_fetches()->Add(1);
    } else {
      stats->failed_filter_resource_fetches()->Add(1);
    }
    SharedAsyncFetch::HandleDone(success);
    driver_->FetchComplete();
    delete this;
  }

 private:
  RewriteDriver* driver_;
};

class CacheCallback : public OptionsAwareHTTPCacheCallback {
 public:
  CacheCallback(RewriteDriver* driver,
                RewriteFilter* filter,
                const OutputResourcePtr& output_resource,
                AsyncFetch* async_fetch,
                MessageHandler* handler)
      : OptionsAwareHTTPCacheCallback(driver->options(),
                                      async_fetch->request_context()),
        driver_(driver),
        filter_(filter),
        output_resource_(output_resource),
        async_fetch_(async_fetch),
        handler_(handler) {
    // Canonicalize the URL before looking it up.  Applies
    // rewrite-domain mappings, and reverses any sharding.  E.g.
    // if you have
    //     ModPagespeedMapRewriteDomain master alias
    //     ModPagespeedShardDomain master shard1,shard2
    // then this will convert:
    //    http://alias/foo    -->   http://master/foo
    //    http://shard1/foo   -->   http://master/foo
    //    http://shard2/foo   -->   http://master/foo
    //    http://master/foo   -->   http://master/foo
    canonical_url_ = output_resource_->HttpCacheKey();
  }

  virtual ~CacheCallback() {}

  void Find() {
    ServerContext* server_context = driver_->server_context();
    HTTPCache* http_cache = server_context->http_cache();
    http_cache->Find(canonical_url_, driver_->CacheFragment(), handler_, this);
  }

  bool IsCacheValid(const GoogleString& key, const ResponseHeaders& headers) {
    // If the user cares, don't try to send a rewritten .pagespeed. webp
    // resources to a browser that can't handle it.
    if (!driver_->options()->serve_rewritten_webp_urls_to_any_agent() &&
        (headers.DetermineContentType() == &kContentTypeWebp) &&
        !async_fetch_->request_context()->accepts_webp()) {
      return false;
    }
    return OptionsAwareHTTPCacheCallback::IsCacheValid(key, headers);
  }

  virtual void Done(HTTPCache::FindResult find_result) {
    StringPiece content;
    ResponseHeaders* response_headers = async_fetch_->response_headers();
    if (find_result.status == HTTPCache::kFound) {
      RewriteStats* stats = driver_->server_context()->rewrite_stats();
      stats->cached_resource_fetches()->Add(1);

      HTTPValue* value = http_value();
      bool success = (value->ExtractContents(&content) &&
                      value->ExtractHeaders(response_headers, handler_));
      if (success) {
        output_resource_->Link(value, handler_);
        output_resource_->SetWritten(true);
        async_fetch_->set_content_length(content.size());
        async_fetch_->FixCacheControlForGoogleCache();
        async_fetch_->HeadersComplete();
        success = async_fetch_->Write(content, handler_);
      }
      async_fetch_->Done(success);
      driver_->FetchComplete();
      delete this;
    } else {
      if (output_resource_->IsWritten()) {
        // OutputResources can also be loaded while not in cache if
        // FetchOutputResource() somehow got called on an already written
        // resource object (while the cache somehow decided not to store it).
        content = output_resource_->ExtractUncompressedContents();
        response_headers->CopyFrom(*output_resource_->response_headers());
        ServerContext* server_context = driver_->server_context();
        HTTPCache* http_cache = server_context->http_cache();
        http_cache->Put(canonical_url_, driver_->CacheFragment(),
                        RequestHeaders::Properties(),
                        (ResponseHeaders::GetVaryOption(
                            driver_->options()->respect_vary())),
                        response_headers, content, handler_);
        async_fetch_->Done(async_fetch_->Write(content, handler_));
        driver_->FetchComplete();
      } else {
        // Use the filter to reconstruct.
        if (filter_ != NULL) {
          FilterFetch::Start(filter_, output_resource_, async_fetch_, handler_);
        } else {
          response_headers->SetStatusAndReason(HttpStatus::kNotFound);
          async_fetch_->Done(false);
          driver_->FetchComplete();
        }
      }
      delete this;
    }
  }

 private:
  RewriteDriver* driver_;
  RewriteFilter* filter_;
  OutputResourcePtr output_resource_;
  AsyncFetch* async_fetch_;
  MessageHandler* handler_;
  GoogleString canonical_url_;
};

}  // namespace

bool RewriteDriver::FetchResource(const StringPiece& url,
                                  AsyncFetch* async_fetch) {
  DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
  DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchBackground));
  DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefParsing));
  bool handled = false;

  fetch_url_ = url.as_string();

  // Set the request headers if they haven't been yet.
  if (request_headers_ == NULL && async_fetch->request_headers() != NULL) {
    SetRequestHeaders(*async_fetch->request_headers());
  }

  // Note that this does permission checking and parsing of the url, but doesn't
  // actually fetch any data until we specifically ask it to.
  RewriteFilter* filter = NULL;
  GoogleUrl gurl(url);
  OutputResourcePtr output_resource(DecodeOutputResource(gurl, &filter));

  if (output_resource.get() != NULL) {
    handled = true;
    if (filter != NULL) {
      // TODO(marq): This is a gross generalization. Remove this and properly
      // log the application of each rewrite filter.
      filter->LogFilterModifiedContent();
    }
    FetchOutputResource(output_resource, filter, async_fetch);
  } else if (options()->in_place_rewriting_enabled()) {
    // TODO(jcrowell): Make URLs with signatures take this path so they will 403
    // instead of 404.
    // This is an ajax resource.
    handled = true;
    // TODO(sligocki): Get rid of this fallback and make all callers call
    // FetchInPlaceResource when that is what they want.
    FetchInPlaceResource(gurl, true /* proxy_mode */, async_fetch);
  }

  // Note: "this" may have been deleted by this point. It is not safe to
  // reference data members.

  return handled;
}

void RewriteDriver::FetchInPlaceResource(const GoogleUrl& gurl,
                                         bool proxy_mode,
                                         AsyncFetch* async_fetch) {
  CHECK(gurl.IsWebValid()) << "Invalid URL " << gurl.spec_c_str();
  CHECK(request_headers_.get() != NULL);
  gurl.Spec().CopyToString(&fetch_url_);
  StringPiece base = gurl.AllExceptLeaf();
  ResourceNamer namer;
  OutputResourcePtr output_resource(
      new OutputResource(this, base, base, base, namer, kRewrittenResource));
  SetBaseUrlForFetch(gurl.Spec());
  // Set the request headers if they haven't been yet.
  if (request_headers_ == NULL && async_fetch->request_headers() != NULL) {
    SetRequestHeaders(*async_fetch->request_headers());
  }

  ref_counts_.AddRef(kRefFetchUserFacing);
  InPlaceRewriteContext* context = new InPlaceRewriteContext(this, gurl.Spec());
  context->set_proxy_mode(proxy_mode);

  // Save pointer to stats_logger before "this" is deleted.
  StatisticsLogger* stats_logger =
      server_context_->statistics()->console_logger();

  if (!context->Fetch(output_resource, async_fetch, message_handler())) {
    // RewriteContext::Fetch can fail if the input URLs are undecodeable
    // or unfetchable. There is no decoding in this case, but unfetchability
    // is possible if we're given an https URL but have a fetcher that
    // can't do it. In that case, the only thing we can do is fail
    // and cleanup.
    async_fetch->Done(false);
    FetchComplete();
  }

  // Note: "this" may have been deleted by this point. It is not safe to
  // reference data members.

  // Update statistics log.
  if (stats_logger != NULL) {
    stats_logger->UpdateAndDumpIfRequired();
  }
}

bool RewriteDriver::FetchOutputResource(
    const OutputResourcePtr& output_resource,
    RewriteFilter* filter,
    AsyncFetch* async_fetch) {

  // None of our resources ever change -- the hash of the content is embedded
  // in the filename.  This is why we serve them with very long cache
  // lifetimes.  However, when the user presses Reload, the browser may
  // attempt to validate that the cached copy is still fresh by sending a GET
  // with an If-Modified-Since header.  If this header is present, we should
  // return a 304 Not Modified, since any representation of the resource
  // that's in the browser's cache must be correct.
  bool queued = false;
  ConstStringStarVector values;
  // Save pointer to stats_logger before "this" is deleted.
  StatisticsLogger* stats_logger =
      server_context_->statistics()->console_logger();
  if (async_fetch->request_headers()->Lookup(HttpAttributes::kIfModifiedSince,
                                             &values)) {
    async_fetch->response_headers()->SetStatusAndReason(
        HttpStatus::kNotModified);
    async_fetch->HeadersComplete();
    async_fetch->Done(true);
    queued = false;
  } else {
    SetBaseUrlForFetch(output_resource->url());
    ref_counts_.AddRef(kRefFetchUserFacing);
    if (output_resource->kind() == kOnTheFlyResource) {
      // Don't bother to look up the resource in the cache: ask the filter.
      if (filter != NULL) {
        queued = FilterFetch::Start(filter, output_resource, async_fetch,
                                    message_handler());
      }
    } else {
      CacheCallback* cache_callback = new CacheCallback(
          this, filter, output_resource, async_fetch, message_handler());
      cache_callback->Find();
      queued = true;
    }
  }

  // Update statistics log.
  if (stats_logger != NULL) {
    stats_logger->UpdateAndDumpIfRequired();
  }

  return queued;
}

void RewriteDriver::FetchComplete() {
  DropReference(kRefFetchUserFacing);
}

void RewriteDriver::DetachFetch() {
  ScopedMutex lock(rewrite_mutex());
  CHECK_EQ(1, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
  CHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchBackground));
  ref_counts_.AddRefMutexHeld(kRefFetchBackground);
}

void RewriteDriver::DetachedFetchComplete() {
  DropReference(kRefFetchBackground);
}

bool RewriteDriver::MayRewriteUrl(
    const GoogleUrl& domain_url,
    const GoogleUrl& input_url,
    InlineAuthorizationPolicy inline_authorization_policy,
    IntendedFor intended_for,
    bool* is_authorized_domain) const {
  *is_authorized_domain = false;
  if (domain_url.IsWebValid()) {
    if (options()->IsAllowed(input_url.Spec()) ||
        (intended_for == kIntendedForInlining &&
         options()->IsAllowedWhenInlining(input_url.Spec()))) {
      *is_authorized_domain = options()->domain_lawyer()->IsDomainAuthorized(
          domain_url, input_url);
      if (!*is_authorized_domain &&
          inline_authorization_policy == kInlineUnauthorizedResources) {
        // We decide that this URL can be rewritten (true) but
        // is_authorized_domain will be retained as false to allow creation of
        // the Resource object in the correct cache key space.
        return true;
      }
    }
  }
  return *is_authorized_domain;
}

bool RewriteDriver::MatchesBaseUrl(const GoogleUrl& input_url) const {
  return (decoded_base_url_.IsWebValid() &&
          options()->IsAllowed(input_url.Spec()) &&
          decoded_base_url_.Origin() == input_url.Origin());
}

ResourcePtr RewriteDriver::CreateInputResource(const GoogleUrl& input_url,
                                               InputRole role,
                                               bool* is_authorized) {
  return CreateInputResource(
      input_url, kInlineOnlyAuthorizedResources, kIntendedForGeneral,
      role, is_authorized);
}

ResourcePtr RewriteDriver::CreateInputResource(
    const GoogleUrl& input_url,
    InlineAuthorizationPolicy inline_authorization_policy,
    IntendedFor intended_for,
    InputRole role,
    bool* is_authorized) {
  *is_authorized = true;  // Must be false iff we fail b/c of authorization.
  ResourcePtr resource;
  bool may_rewrite = false;
  if (input_url.SchemeIs("data")) {
    // Skip and silently ignore; don't log a failure.
    // For the moment we assume data: urls are small enough to not be worth
    // optimizing.  We have optimized them in the past, but that code is likely
    // to have bit-rotted since it was disabled.
    return resource;
  } else if (decoded_base_url_.IsAnyValid()) {
    if (!IsLoadPermittedByCsp(input_url, role)) {
      *is_authorized = false;
      message_handler()->Message(kInfo, "CSP prevents use of '%s'",
                                input_url.spec_c_str());
      return resource;
    }

    may_rewrite = MayRewriteUrl(decoded_base_url_, input_url,
                                inline_authorization_policy,
                                intended_for,
                                is_authorized);
    // In the case where we are proxying and we have resources that have been
    // rewritten multiple times, input_url will still have the encoded domain,
    // and we can rewrite that, so test again but against the encoded base url.
    if (!may_rewrite) {
      UrlNamer* namer = server_context()->url_namer();
      GoogleString decoded_input;
      if (namer->Decode(input_url, options(), &decoded_input)) {
        GoogleUrl decoded_url(decoded_input);
        may_rewrite = MayRewriteUrl(decoded_base_url_, decoded_url,
                                    inline_authorization_policy,
                                    intended_for,
                                    is_authorized);
      }
    }
  } else {
    // Shouldn't happen?
    message_handler()->Message(
        kFatal, "invalid decoded_base_url_ for '%s'", input_url.spec_c_str());
    LOG(DFATAL);
  }
  RewriteStats* stats = server_context_->rewrite_stats();
  if (may_rewrite) {
    // *is_authorized may be true or false (if inlining an unauth'd URL).
    resource = CreateInputResourceUnchecked(input_url, *is_authorized);
    stats->resource_url_domain_acceptances()->Add(1);
  } else {
    DCHECK(!*is_authorized);
    message_handler()->Message(kInfo, "No permission to rewrite '%s'",
                               input_url.spec_c_str());
    stats->resource_url_domain_rejections()->Add(1);
  }
  return resource;
}

ResourcePtr RewriteDriver::CreateInputResourceAbsoluteUncheckedForTestsOnly(
    const StringPiece& absolute_url) {
  GoogleUrl url(absolute_url);
  if (!url.IsWebOrDataValid()) {
    // Note: Bad user-content can leave us here.  But it's really hard
    // to concatenate a valid protocol and domain onto an arbitrary string
    // and end up with an invalid GURL.
    message_handler()->Message(kInfo, "Invalid resource url '%s'",
                               url.spec_c_str());
    return ResourcePtr();
  }
  return CreateInputResourceUnchecked(url, true);
}

ResourcePtr RewriteDriver::CreateInputResourceUnchecked(
    const GoogleUrl& url,
    bool is_authorized_domain) {
  StringPiece url_string = url.Spec();
  ResourcePtr resource;

  if (IsResourceUrlClaimed(url)) {
    return resource;
  }

  if (url.SchemeIs("data")) {
    resource = DataUrlInputResource::Make(url_string, this);
    if (resource.get() == NULL) {
      // Note: Bad user-content can leave us here.
      message_handler()->Message(kWarning, "Badly formatted data url '%s'",
                                 url.spec_c_str());
    }
  } else if (url.SchemeIs("http") || url.SchemeIs("https")) {
    // Note: type may be NULL if url has an unexpected or malformed extension.
    const ContentType* type = NameExtensionToContentType(url.LeafSansQuery());
    GoogleString filename;
    if (options()->file_load_policy()->ShouldLoadFromFile(url, &filename)) {
      resource.reset(
          new FileInputResource(this, type, url_string, filename));
    } else {
      // If the scheme is https and the fetcher doesn't support https, map
      // the URL to what will ultimately be fetched to see if that will be
      // http, in which case the fetcher will be able to handle it.
      GoogleString mapped_url;
      GoogleString host_header;
      bool is_proxy = false;
      options()->domain_lawyer()->MapOriginUrl(url, &mapped_url,
                                               &host_header, &is_proxy);
      GoogleUrl mapped_gurl(mapped_url);
      if (mapped_gurl.SchemeIs("http") ||
          (mapped_gurl.SchemeIs("https") &&
           url_async_fetcher_->SupportsHttps())) {
        resource.reset(new UrlInputResource(this, type, url_string,
                                            is_authorized_domain));
      } else {
        message_handler()->Message(
            kInfo, "Cannot fetch url '%s': as %s is not supported",
            url.spec_c_str(), mapped_gurl.Scheme().as_string().c_str());
      }
    }
  } else {
    // Note: Valid user-content can leave us here.
    // Specifically, any URLs with scheme other than data: or http: or https:.
    // TODO(sligocki): Is this true? Or will such URLs not make it this far?
    message_handler()->Message(kWarning, "Unsupported scheme '%s' for url '%s'",
                               url.Scheme().as_string().c_str(),
                               url.spec_c_str());
  }
  return resource;
}

bool RewriteDriver::IsResourceUrlClaimed(const GoogleUrl& url) const {
  for (int i = 0, n = resource_claimants_.size(); i < n; ++i) {
    bool claims = false;
    resource_claimants_[i]->Run(url, &claims);
    if (claims) {
      return true;
    }
  }
  return false;
}

bool RewriteDriver::StartParseId(const StringPiece& url, const StringPiece& id,
                                 const ContentType& content_type) {
  if (response_headers_ != NULL) {
    status_code_ = response_headers_->status_code();
  }
  start_time_ms_ = server_context_->timer()->NowMs();
  set_log_rewrite_timing(options()->log_rewrite_timing());

  if (debug_filter_ != NULL) {
    debug_filter_->InitParse();
  }

  bool ret = HtmlParse::StartParseId(url, id, content_type);
  if (ret) {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefParsing));
    ref_counts_.AddRefMutexHeld(kRefParsing);
  }

  if (ret) {
    DCHECK(filters_added_);
    set_buffer_events(true);  // Release buffer when AMPness is discovered.
    base_was_set_ = false;
    if (is_url_valid()) {
      base_url_.Reset(google_url());
      SetDecodedUrlFromBase();
    }
  }

  can_rewrite_resources_ = server_context_->metadata_cache()->IsHealthy();
  return ret;
}

void RewriteDriver::ParseTextInternal(const char* content, int size) {
  num_bytes_in_ += size;
  if (ShouldSkipParsing()) {
    StringPiece sp(content, size);
    writer()->Write(sp, message_handler());
  } else if (debug_filter_ != NULL) {
    debug_filter_->StartParse();
    HtmlParse::ParseTextInternal(content, size);
    debug_filter_->EndParse();
  } else {
    HtmlParse::ParseTextInternal(content, size);
  }
}

void RewriteDriver::SetDecodedUrlFromBase() {
  UrlNamer* namer = server_context()->url_namer();
  GoogleString decoded_base;
  if (namer->Decode(base_url_, options(), &decoded_base)) {
    decoded_base_url_.Reset(decoded_base);
  } else {
    decoded_base_url_.Reset(base_url_);
  }
  DCHECK(decoded_base_url_.IsAnyValid());
}

bool RewriteDriver::ShouldSkipParsing() {
  if (should_skip_parsing_ == kNotSet) {
    bool should_skip = false;
    PropertyPage* page = property_page();
    if (page != NULL) {
      PropertyCache* pcache = server_context_->page_property_cache();
      const PropertyCache::Cohort* dom_cohort = pcache->GetCohort(kDomCohort);
      if (dom_cohort != NULL) {
        PropertyValue* property_value = property_page()->GetProperty(
            dom_cohort, kParseSizeLimitExceeded);
        should_skip = property_value->has_value() &&
            StringCaseEqual(property_value->value(), "1");
      }
    }
    should_skip_parsing_ = should_skip ? kTrue : kFalse;
  }
  return (should_skip_parsing_ == kTrue);
}

bool RewriteDriver::PrepareShouldSignal() {
  // Basically, we just save IsDone() from before state changes.
  return IsDone(waiting_, waiting_deadline_reached_);
}

void RewriteDriver::SignalIfRequired(bool result_of_prepare_should_signal) {
  // If we were already done before, or no one is waiting, no need to signal
  if (result_of_prepare_should_signal || waiting_ == kNoWait) {
    return;
  }

  if (IsDone(waiting_, waiting_deadline_reached_)) {
    // If someone is waiting, refcount shouldn't be 0!
    DCHECK(!release_driver_);
    scheduler_->Signal();
  }
}

void RewriteDriver::RewriteComplete(RewriteContext* rewrite_context,
                                    RenderOp render_op) {
  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
    bool signal_cookie = PrepareShouldSignal();
    bool attached = false;

    // Rewrite transitions either pending -> deleting or detached -> deleting
    ref_counts_.AddRefMutexHeld(kRefDeletingRewrites);
    RewriteContextSet::iterator p = initiated_rewrites_.find(rewrite_context);
    if (p != initiated_rewrites_.end()) {
      if (rewrite_context->is_metadata_cache_miss()) {
        // If the rewrite completed within the deadline and it actually involved
        // and fetch rewrite (not a metadata hit or successful revalidate) then
        // bump up the corresponding counter in log record.
        ScopedMutex lock(log_record()->mutex());
        MetadataCacheInfo* metadata_log_info =
            log_record()->logging_info()->mutable_metadata_cache_info();
        metadata_log_info->set_num_successful_rewrites_on_miss(
            metadata_log_info->num_successful_rewrites_on_miss() + 1);
      }
      initiated_rewrites_.erase(p);
      attached = true;

      ref_counts_.ReleaseRefMutexHeld(kRefPendingRewrites);
      if (!rewrite_context->slow()) {
        --possibly_quick_rewrites_;
      }
    } else {
      int erased = detached_rewrites_.erase(rewrite_context);
      CHECK_EQ(1, erased) << " rewrite_context " << rewrite_context
                          << " not in either detached_rewrites or "
                          << "initiated_rewrites_";
      ref_counts_.ReleaseRefMutexHeld(kRefDetachedRewrites);
    }
    // release_driver_ should be false since we moved a count between
    // categories, and didn't change the total.
    DCHECK(!release_driver_) << ref_counts_.DebugStringMutexHeld();
    if (!attached) {
      render_op = RenderOp::kDontRender;
    }
    rewrite_context->Propagate(render_op);
    SignalIfRequired(signal_cookie);
  }
}

void RewriteDriver::ReportSlowRewrites(int num) {
  ScopedMutex lock(rewrite_mutex());
  bool signal_cookie = PrepareShouldSignal();
  possibly_quick_rewrites_ -= num;
  CHECK_LE(0, possibly_quick_rewrites_) << base_url_.Spec();
  SignalIfRequired(signal_cookie);
}

void RewriteDriver::DeleteRewriteContext(RewriteContext* rewrite_context) {
  delete rewrite_context;
  DropReference(kRefDeletingRewrites);
}


void RewriteDriver::PossiblyPurgeCachedResponseAndReleaseDriver() {
  DCHECK(!externally_managed_);
  // We might temporarily (due to purging) revive the object here, so
  // better clear the "we were told it's dead!" bit.
  release_driver_ = false;
  if (downstream_cache_purger_.MaybeIssuePurge(google_url())) {
    return;
  }
  server_context_->ReleaseRewriteDriver(this);
}

RewriteContext* RewriteDriver::RegisterForPartitionKey(
    const GoogleString& partition_key, RewriteContext* candidate) {
  std::pair<PrimaryRewriteContextMap::iterator, bool> insert_result =
      primary_rewrite_context_map_.insert(
          std::make_pair(partition_key, candidate));
  if (insert_result.second) {
    // Our value is new, so just return NULL.
    return NULL;
  } else {
    // Insert failed, return the old value.
    return insert_result.first->second;
  }
}

void RewriteDriver::DeregisterForPartitionKey(const GoogleString& partition_key,
                                              RewriteContext* rewrite_context) {
  // If the context being deleted is the primary for some cache key,
  // deregister it.
  PrimaryRewriteContextMap::iterator i =
      primary_rewrite_context_map_.find(partition_key);
  if ((i != primary_rewrite_context_map_.end()) &&
      (i->second == rewrite_context)) {
    primary_rewrite_context_map_.erase(i);
  }
}

void RewriteDriver::WriteDomCohortIntoPropertyCache() {
  // Only update the property cache if there is a filter or option enabled that
  // actually makes use of it.
  if (!(write_property_cache_dom_cohort_ ||
        options()->max_html_parse_bytes() > 0)) {
    return;
  }

  PropertyPage* page = property_page();
  // Dont update property cache value if we are flushing early.
  // TODO(jud): Is this the best place to check for shutting down? It might
  // make more sense for this check to be done at the property cache or
  // lower level.
  if (server_context_->shutting_down() ||
      page == NULL ||
      !owns_property_page_) {
    return;
  }
  // Update the timestamp of the last request in both actual property page
  // and property page with fallback values.
  UpdatePropertyValueInDomCohort(
    fallback_property_page(),
    kLastRequestTimestamp,
    Integer64ToString(server_context()->timer()->NowMs()));
  // Update the status code of the last request.
  if (status_code_ != HttpStatus::kUnknownStatusCode) {
    UpdatePropertyValueInDomCohort(
        fallback_property_page(),
        kStatusCodePropertyName, IntegerToString(status_code_));
  }
  if (options()->max_html_parse_bytes() > 0) {
    // Update whether the page exceeded the html parse size limit.
    UpdatePropertyValueInDomCohort(
        page, kParseSizeLimitExceeded,
        num_bytes_in_ > options()->max_html_parse_bytes() ? "1" : "0");
  }
  if (flush_early_info_.get() != NULL) {
    GoogleString value;
    flush_early_info_->SerializeToString(&value);
    UpdatePropertyValueInDomCohort(
        fallback_property_page(), kSubresourcesPropertyName, value);
  }
  // Write dom cohort for both actual property page and property page with
  // fallback values.
  fallback_property_page()->WriteCohort(server_context()->dom_cohort());
}

void RewriteDriver::UpdatePropertyValueInDomCohort(
    AbstractPropertyPage* page,
    StringPiece property_name,
    StringPiece property_value) {
  if (page == NULL || !owns_property_page_) {
    return;
  }
  page->UpdateValue(
      server_context()->dom_cohort(), property_name, property_value);
}

void RewriteDriver::Cleanup() {
  {
    // TODO(morlovich): Clean this up, it's a rather inappropriate place to
    // do this.
    ScopedMutex lock(log_record()->mutex());
    if (!log_record()->logging_info()->has_experiment_id()) {
      log_record()->logging_info()->set_experiment_id(
          options()->experiment_id());
    }
  }
  DropReference(kRefUser);
}

void RewriteDriver::AddUserReference() {
  ref_counts_.AddRef(kRefUser);
}

namespace {

void AppendBool(GoogleString* out, const char* name, bool val) {
  StrAppend(out, name, ": ", val ? "true\n": "false\n");
}

}  // namespace

GoogleString RewriteDriver::ToStringLockHeld(bool show_detached_contexts)
    const {
  GoogleString out;
  StrAppend(&out, "URL: ", google_url().Spec(), "\n");
  StrAppend(&out, "decoded_base: ", decoded_base_url().Spec(), "\n");
  AppendBool(&out, "base_was_set", base_was_set_);
  StrAppend(&out, "containing_charset: ", containing_charset_, "\n");
  AppendBool(&out, "filters_added", filters_added_);
  AppendBool(&out, "externally_managed", externally_managed_);
  switch (waiting_) {
    case kNoWait:
      StrAppend(&out, "waiting: kNoWait\n");
      break;
    case kWaitForCompletion:
      StrAppend(&out, "waiting: kWaitForCompletion\n");
      break;
    case kWaitForCachedRender:
      StrAppend(&out, "waiting: kWaitForCachedRender\n");
      break;
    case kWaitForShutDown:
      StrAppend(&out, "waiting: kWaitForShutDown\n");
      break;
    default:
      StrAppend(&out, "waiting: ", IntegerToString(waiting_));
      break;
  }
  AppendBool(&out, "waiting_deadline_reached", waiting_deadline_reached_);
  StrAppend(&out, "detached_rewrites_.size(): ",
            IntegerToString(detached_rewrites_.size()), "\n");

  if (show_detached_contexts) {
    for (RewriteContextSet::iterator p = detached_rewrites_.begin(),
             e = detached_rewrites_.end(); p != e; ++p) {
      RewriteContext* detached_rewrite = *p;
      StrAppend(&out, "  Detached Rewrite:\n",
                detached_rewrite->ToStringWithPrefix("  "));
    }
  }
  AppendBool(&out, "RewritesComplete()", RewritesComplete());
  AppendBool(&out, "fully_rewrite_on_flush", fully_rewrite_on_flush_);
  AppendBool(&out, "fast_blocking_rewrite", fast_blocking_rewrite_);
  AppendBool(&out, "flush_requested", flush_requested_);
  AppendBool(&out, "flush_occurred", flush_occurred_);
  AppendBool(&out, "is_lazyload_script_flushed", is_lazyload_script_flushed_);
  AppendBool(&out, "release_driver", release_driver_);
  AppendBool(&out, "write_property_cache_dom_cohort",
             write_property_cache_dom_cohort_);
  AppendBool(&out, "owns_property_page", owns_property_page_);
  AppendBool(&out, "xhtml_mimetype_computed", xhtml_mimetype_computed_);
  AppendBool(&out, "can_rewrite_resources", can_rewrite_resources_);
  AppendBool(&out, "is_nested", is_nested());
  StrAppend(&out, "ref counts:\n", ref_counts_.DebugStringMutexHeld());
  return out;
}

GoogleString RewriteDriver::ToString(bool show_detached_contexts) const {
  ScopedMutex lock(rewrite_mutex());
  return ToStringLockHeld(show_detached_contexts);
}

void RewriteDriver::PrintState(bool show_detached_contexts) {
  fputs(ToString(show_detached_contexts).c_str(), stderr);
  fputc('\n', stderr);
}

void RewriteDriver::PrintStateToErrorLog(bool show_detached_contexts) {
  message_handler()->MessageS(kError, ToString(show_detached_contexts));
}

void RewriteDriver::LogStats() {
  if (dom_stats_filter_ != NULL && log_record() != NULL) {
    log_record()->SetImageStats(dom_stats_filter_->num_img_tags(),
                                dom_stats_filter_->num_inlined_img_tags(),
                                dom_stats_filter_->num_critical_images_used());
    log_record()->SetResourceCounts(dom_stats_filter_->num_external_css(),
                                    dom_stats_filter_->num_scripts());
  }
  request_properties_->LogDeviceInfo(
      log_record(), options()->enable_aggressive_rewriters_for_mobile());
  bool is_xhr = request_headers() != NULL &&
      request_headers()->IsXmlHttpRequest();
  log_record()->LogIsXhr(is_xhr);
}

void RewriteDriver::FinishParse() {
  SchedulerBlockingFunction wait(scheduler_);
  FinishParseAsync(&wait);
  wait.Block();
}

void RewriteDriver::FinishParseAsync(Function* callback) {
  HtmlParse::BeginFinishParse();
  FlushAsync(
      MakeFunction(this, &RewriteDriver::QueueFinishParseAfterFlush, callback));
}

void RewriteDriver::QueueFinishParseAfterFlush(Function* user_callback) {
  Function* finish_parse = MakeFunction(this,
                                        &RewriteDriver::FinishParseAfterFlush,
                                        user_callback);
  html_worker_->Add(finish_parse);
}

void RewriteDriver::FinishParseAfterFlush(Function* user_callback) {
  DCHECK_EQ(0U, GetEventQueueSize());
  HtmlParse::EndFinishParse();
  LogStats();
  WriteDomCohortIntoPropertyCache();
  dependency_tracker_->FinishedParsing();

  // Update stats.
  RewriteStats* stats = server_context_->rewrite_stats();
  stats->rewrite_latency_histogram()->Add(
      server_context_->timer()->NowMs() - start_time_ms_);
  stats->total_rewrite_count()->IncBy(1);

  // Update statistics log.
  StatisticsLogger* stats_logger =
      server_context_->statistics()->console_logger();
  if (stats_logger != NULL) {
    stats_logger->UpdateAndDumpIfRequired();
  }

  DropReference(kRefParsing);
  Cleanup();
  if (user_callback != NULL) {
    user_callback->CallRun();
  }
}

void RewriteDriver::InfoAt(const RewriteContext* context,
                           const char* msg, ...) {
  va_list args;
  va_start(args, msg);

  if ((context == NULL) || (context->num_slots() == 0)) {
    InfoHereV(msg, args);
  } else {
    GoogleString new_msg;
    for (int c = 0; c < context->num_slots(); ++c) {
      StrAppend(&new_msg, context->slot(c)->LocationString(),
                ((c == context->num_slots() - 1) ? ": " : " "));
    }
    StringAppendV(&new_msg, msg, args);
    message_handler()->MessageS(kInfo, new_msg);
  }

  va_end(args);
}

// Constructs name and URL for the specified input resource and encoder.
bool RewriteDriver::GenerateOutputResourceNameAndUrl(
    const UrlSegmentEncoder* encoder,
    const ResourceContext* data,
    const ResourcePtr& input_resource,
    GoogleString* name,
    GoogleUrl* mapped_gurl,
    GoogleString* failure_reason) {
  if (input_resource.get() == NULL) {
    *failure_reason = "No input resource.";
    return false;
  }

  // TODO(jmarantz): It would be more efficient to pass in the base
  // document GURL or save that in the input resource.
  GoogleUrl unmapped_gurl(input_resource->url());
  GoogleString mapped_domain;  // Unused. TODO(sligocki): Stop setting this?
  // Get the domain and URL after any domain lawyer rewriting.
  if (!options()->IsAllowed(unmapped_gurl.Spec())) {
    *failure_reason = StrCat("Rewriting disallowed for ", unmapped_gurl.Spec());
    return false;
  }

  if (!options()->domain_lawyer()->MapRequestToDomain(
          unmapped_gurl, unmapped_gurl.Spec(), &mapped_domain, mapped_gurl,
          server_context_->message_handler())) {
    *failure_reason = StrCat("Domain not authorized for ",
                             unmapped_gurl.Spec());
    return false;
  }

  StringVector v;
  v.push_back(mapped_gurl->LeafWithQuery().as_string());
  encoder->Encode(v, data, name);
  return true;
}

// Constructs an output resource corresponding to the specified input resource
// and encoded using the provided encoder.
OutputResourcePtr RewriteDriver::CreateOutputResourceFromResource(
    const char* filter_id,
    const UrlSegmentEncoder* encoder,
    const ResourceContext* data,
    const ResourcePtr& input_resource,
    OutputResourceKind kind,
    GoogleString* failure_reason) {
  OutputResourcePtr result;
  GoogleString name;
  GoogleUrl mapped_gurl;
  if (!GenerateOutputResourceNameAndUrl(encoder, data, input_resource, &name,
                                        &mapped_gurl, failure_reason)) {
    return result;
  }

  // TODO(jmarantz): It would be more efficient to pass in the base
  // document GURL or save that in the input resource.
  GoogleUrl unmapped_gurl(input_resource->url());

  result.reset(CreateOutputResourceWithMappedPath(
      mapped_gurl.AllExceptLeaf(), unmapped_gurl.AllExceptLeaf(),
      filter_id, name, kind, failure_reason));

  CHECK(input_resource->is_authorized_domain());
  return result;
}

void RewriteDriver::PopulateResourceNamer(
    const StringPiece& filter_id,
    const StringPiece& name,
    ResourceNamer* full_name) {
  full_name->set_id(filter_id);
  full_name->set_name(name);
  full_name->set_experiment(options()->GetExperimentStateStr());

  // Note that we never populate ResourceNamer::options for in place resource
  // rewrites.
  if (filter_id != RewriteOptions::kInPlaceRewriteId &&
      !full_name->has_experiment() && options()->add_options_to_urls()) {
    GoogleString resource_option = RewriteQuery::GenerateResourceOption(
        filter_id, this);
    full_name->set_options(resource_option);
  } else {
    full_name->set_options("");
  }
}

OutputResourcePtr RewriteDriver::CreateOutputResourceWithPath(
    const StringPiece& mapped_path,
    const StringPiece& unmapped_path,
    const StringPiece& base_url,
    const StringPiece& filter_id,
    const StringPiece& name,
    OutputResourceKind kind,
    GoogleString* failure_reason) {
  ResourceNamer full_name;
  PopulateResourceNamer(filter_id, name, &full_name);
  OutputResourcePtr resource;
  int max_leaf_size =
      full_name.EventualSize(*server_context_->hasher(), SignatureLength()) +
      ContentType::MaxProducedExtensionLength();
  if (max_leaf_size > options()->max_url_segment_size()) {
    *failure_reason = "Rewritten URL segment too long.";
    return resource;
  }

  bool no_hash = false;
  int extra_len = 0;
  Hasher* hasher = server_context()->hasher();
  if (full_name.hash().empty()) {
    // Content and content type are not present. So set some nonzero hash and
    // assume largest possible extension.
    no_hash = true;
    full_name.set_hash(GoogleString(hasher->HashSizeInChars(), '#'));
    extra_len = ContentType::MaxProducedExtensionLength();
  }
  resource.reset(new OutputResource(
      this, mapped_path, unmapped_path, base_url, full_name, kind));

  if (options()->max_url_size() <
      (static_cast<int>(resource->url().size()) + extra_len)) {
    *failure_reason = StrCat("Rewritten URL too long: ",  resource->url());
    resource.clear();
    return resource;
  }
  if (no_hash) {
    resource->clear_hash();
  }
  return resource;
}

OutputResourcePtr RewriteDriver::CreateOutputResourceWithUnmappedUrl(
    const GoogleUrl& unmapped_gurl, const StringPiece& filter_id,
    const StringPiece& name, OutputResourceKind kind,
    GoogleString* failure_reason) {
  OutputResourcePtr resource;
  GoogleString mapped_domain;  // Unused. TODO(sligocki): Stop setting this?
  GoogleUrl mapped_gurl;
  // Get the domain and URL after any domain lawyer rewriting.
  if (!options()->IsAllowed(unmapped_gurl.Spec())) {
    *failure_reason = StrCat("Rewriting disallowed for ", unmapped_gurl.Spec());
    return resource;
  }
  if (!options()->domain_lawyer()->MapRequestToDomain(
          unmapped_gurl, unmapped_gurl.Spec(), &mapped_domain, &mapped_gurl,
          server_context_->message_handler())) {
    *failure_reason = StrCat("Domain not authorized for ",
                             unmapped_gurl.Spec());
    return resource;
  }

  resource.reset(CreateOutputResourceWithMappedPath(
      mapped_gurl.AllExceptLeaf(), unmapped_gurl.AllExceptLeaf(),
      filter_id, name, kind, failure_reason));
  return resource;
}

void RewriteDriver::SetBaseUrlIfUnset(const StringPiece& new_base) {
  // Base url is relative to the document URL in HTML5, but not in
  // HTML4.01.  FF3.x does it HTML4.01 way, Chrome, Opera 11 and FF4
  // betas do it according to HTML5, as is our implementation here.
  GoogleUrl new_base_url(base_url_, new_base);
  if (new_base_url.IsAnyValid()) {
    if (base_was_set_) {
      if (new_base_url.Spec() != base_url_.Spec()) {
        InfoHere("Conflicting base tags: %s and %s",
                 new_base_url.spec_c_str(),
                 base_url_.spec_c_str());
      }
    } else {
      base_was_set_ = true;
      base_url_.Swap(&new_base_url);
      SetDecodedUrlFromBase();
    }
  } else {
    InfoHere("Invalid base tag %s relative to %s",
             new_base.as_string().c_str(),
             base_url_.spec_c_str());
  }
}

void RewriteDriver::SetBaseUrlForFetch(const StringPiece& url) {
  // Set the base url for the resource fetch.  This corresponds to where the
  // fetched resource resides (which might or might not be where the original
  // resource lived).

  // TODO(jmaessen): we're re-constructing a GoogleUrl after having already
  // done so (repeatedly over several calls) in DecodeOutputResource!  Gah!
  // We at least assume that base_url_ is valid since it was checked when
  // output_resource was created.
  base_url_.Reset(url);
  DCHECK(base_url_.IsAnyValid());
  SetDecodedUrlFromBase();
  base_was_set_ = false;
}

RewriteFilter* RewriteDriver::FindFilter(const StringPiece& id) const {
  RewriteFilter* filter = NULL;
  StringFilterMap::const_iterator p = resource_filter_map_.find(id.as_string());
  if (p != resource_filter_map_.end()) {
    filter = p->second;
  }
  return filter;
}

HtmlResourceSlotPtr RewriteDriver::GetSlot(
    const ResourcePtr& resource, HtmlElement* elt,
    HtmlElement::Attribute* attr) {
  HtmlResourceSlotPtr slot(new HtmlResourceSlot(resource, elt, attr, this));
  std::pair<HtmlResourceSlotSet::iterator, bool> iter_inserted =
      slots_.insert(slot);
  if (!iter_inserted.second) {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in.
    HtmlResourceSlotSet::iterator iter = iter_inserted.first;
    slot.reset(*iter);
  }
  return slot;
}

InlineResourceSlotPtr RewriteDriver::GetInlineSlot(
    const ResourcePtr& resource, HtmlCharactersNode* char_node) {
  InlineResourceSlotPtr slot(
      new InlineResourceSlot(resource, char_node, UrlLine()));
  std::pair<InlineResourceSlotSet::iterator, bool> iter_inserted =
      inline_slots_.insert(slot);
  if (!iter_inserted.second) {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in.
    InlineResourceSlotSet::iterator iter = iter_inserted.first;
    slot.reset(*iter);
  }
  return slot;
}

InlineAttributeSlotPtr RewriteDriver::GetInlineAttributeSlot(
    const ResourcePtr& resource, HtmlElement* element,
    HtmlElement::Attribute* attribute) {
  InlineAttributeSlotPtr slot(
      new InlineAttributeSlot(resource, element, attribute, UrlLine()));
  std::pair<InlineAttributeSlotSet::iterator, bool> iter_inserted =
      inline_attribute_slots_.insert(slot);
  if (!iter_inserted.second) {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in.
    InlineAttributeSlotSet::iterator iter = iter_inserted.first;
    slot.reset(*iter);
  }
  return slot;
}

SrcSetSlotCollectionPtr RewriteDriver::GetSrcSetSlotCollection(
    CommonFilter* filter, HtmlElement* element, HtmlElement::Attribute* attr) {
  SrcSetSlotCollectionPtr collection(
      new SrcSetSlotCollection(this, element, attr));
  std::pair<SrcSetSlotCollectionSet::iterator, bool> iter_inserted =
      srcset_collections_.insert(collection);
  if (iter_inserted.second) {
    // Inserted successfully, we are first here, so actually parse the
    // attribute, create resources, slots, etc.
    collection->Initialize(filter);
  } else {
    // The slot was already in the set.  Release the one we just
    // allocated and use the one already in. Sanity check policy stuff,
    // though --- all filters sharing this slot must have a consistent
    // policy on what resources can be created.
    SrcSetSlotCollectionSet::iterator iter = iter_inserted.first;
    CHECK_EQ(filter->AllowUnauthorizedDomain(),
             (*iter)->filter()->AllowUnauthorizedDomain());
    CHECK_EQ(filter->IntendedForInlining(),
             (*iter)->filter()->IntendedForInlining());

    collection.reset(*iter);
  }
  return collection;
}

bool RewriteDriver::InitiateRewrite(RewriteContext* rewrite_context) {
#ifndef NDEBUG
  {
    ScopedMutex lock(rewrite_mutex());
    DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
  }
#endif

  // Drop all rewrites if metadata_cache is unhealthy.  This has
  // got to be done 100% or not at all, otherwise we can wind up with
  // a broken slot-context graph.
  //
  // Note that we strobe cache health at the beginning of request
  // (StartParseId), so that we don't decide in the middle of an HTML
  // rewrite that we won't be able to initialize the resource, thus leaving
  // us with a partially constructed slot-graph.
  if (!can_rewrite_resources_) {
    if (rewrites_.empty()) {
      rewrite_context->DetachSlots();
      delete rewrite_context;
      return false;
    } else {
      // A programming error has allowed a RewriteContext to be added
      // despite not being able to rewrite resources.  Log a fatal for
      // debug builds, and otherwise fall through to keep the context-slot
      // graph coherent.
      LOG(DFATAL)
          << "Unexpected queued RewriteContext when cannot rewrite resources";
    }
  }
  rewrites_.push_back(rewrite_context);
  {
    ScopedMutex lock(rewrite_mutex());
    ref_counts_.AddRefMutexHeld(kRefPendingRewrites);
    ++possibly_quick_rewrites_;
  }
  return true;
}

void RewriteDriver::InitiateFetch(RewriteContext* rewrite_context) {
  // TODO(jmarantz): consider setting a bit in the RewriteContext
  // based on server_context_->metadata_cache()->IsHealthy() to tell
  // the system not to perform any optimization on single resources,
  // since the results would not wind up cached.  Instead, just serve
  // the origin resource as it's fetched.  For combined resources, of
  // course, we'll have to run the combiner logic on the fetched data
  // after we collect it all in memory.
  DCHECK_EQ(0, ref_counts_.QueryCountMutexHeld(kRefParsing));
  DCHECK_EQ(1, ref_counts_.QueryCountMutexHeld(kRefFetchUserFacing));
  fetch_rewrites_.push_back(rewrite_context);
}

bool RewriteDriver::MayCacheExtendCss() const {
  return options()->Enabled(RewriteOptions::kExtendCacheCss);
}

bool RewriteDriver::MayCacheExtendImages() const {
  return options()->Enabled(RewriteOptions::kExtendCacheImages);
}

bool RewriteDriver::MayCacheExtendPdfs() const {
  return options()->Enabled(RewriteOptions::kExtendCachePdfs);
}

bool RewriteDriver::MayCacheExtendScripts() const {
  return options()->Enabled(RewriteOptions::kExtendCacheScripts);
}

void RewriteDriver::AddRewriteTask(Function* task) {
  // Note that we hold no locks when we make the decision about whether to
  // schedule on the scheduler_sequence_, so once the driver starts running
  // tasks, we must consider scheduler_sequence_ to be immutable.  This
  // bool helps enforce that invariant.
  executing_rewrite_tasks_.set_value(true);

  if (scheduler_sequence_.get() != NULL) {
    scheduler_sequence_->Add(task);
  } else {
    rewrite_worker_->Add(task);
  }
}

void RewriteDriver::AddLowPriorityRewriteTask(Function* task) {
  low_priority_rewrite_worker_->Add(task);
}

OptionsAwareHTTPCacheCallback::OptionsAwareHTTPCacheCallback(
    const RewriteOptions* rewrite_options, const RequestContextPtr& request_ctx)
    : HTTPCache::Callback(request_ctx, RequestHeaders::Properties()),
      rewrite_options_(rewrite_options) {
  // We initialize the callback with a blank RequestHeaders::Properties,
  // rather than extracing the actual request properties from
  // request_ctx->request_headers().  This is because, with our domain
  // mapping, we don't know for sure whether cookies should apply
  // to Vary:Cacheable resources.  So we pessimistically assume there
  // are cookies by initializing a blank one.

  response_headers()->set_implicit_cache_ttl_ms(
      rewrite_options->implicit_cache_ttl_ms());
}

OptionsAwareHTTPCacheCallback::~OptionsAwareHTTPCacheCallback() {}

bool OptionsAwareHTTPCacheCallback::IsCacheValid(
    const GoogleString& key, const ResponseHeaders& headers) {
  return IsCacheValid(key, *rewrite_options_, request_context(), headers);
}

ResponseHeaders::VaryOption
OptionsAwareHTTPCacheCallback::RespectVaryOnResources() const {
  return ResponseHeaders::GetVaryOption(rewrite_options_->respect_vary());
}

// static
bool OptionsAwareHTTPCacheCallback::IsCacheValid(
    const GoogleString& url,
    const RewriteOptions& rewrite_options,
    const RequestContextPtr& request_ctx,
    const ResponseHeaders& headers) {
  if ((headers.DetermineContentType() == &kContentTypeWebp) &&
      !request_ctx->accepts_webp() &&
      headers.HasValue(HttpAttributes::kVary, HttpAttributes::kAccept)) {
    return false;
  }

  return (headers.has_date_ms() &&
          rewrite_options.IsUrlCacheValid(url, headers.date_ms(),
                                          true /* search_wildcards */));
}

int64 OptionsAwareHTTPCacheCallback::OverrideCacheTtlMs(
    const GoogleString& key) {
  if (rewrite_options_->IsCacheTtlOverridden(key)) {
    return rewrite_options_->override_caching_ttl_ms();
  }
  return -1;
}

RewriteDriver::CssResolutionStatus RewriteDriver::ResolveCssUrls(
    const GoogleUrl& input_css_base,
    const StringPiece& output_css_base,
    const StringPiece& contents,
    Writer* writer,
    MessageHandler* handler) {
  GoogleUrl output_base(output_css_base);
  bool proxy_mode;
  if (ShouldAbsolutifyUrl(input_css_base, output_base, &proxy_mode)) {
    RewriteDomainTransformer transformer(&input_css_base, &output_base,
                                         server_context(), options(),
                                         message_handler());
    if (proxy_mode) {
      // If URLs are being rewritten to a proxy domain, then trimming
      // them based purely on the domain-lawyer mappings is going to
      // relativize them so that they cannot be resolved properly in
      // their intended context.
      //
      // TODO(jmarantz): Consider merging the url_namer with DomainLawyer
      // so that DomainLawyer::WillDomainChange will be accurate.
      transformer.set_trim_urls(false);
    }
    if (CssTagScanner::TransformUrls(contents, writer, &transformer, handler)) {
      return kSuccess;
    } else {
      return kWriteFailed;
    }
  }
  return kNoResolutionNeeded;
}

bool RewriteDriver::ShouldAbsolutifyUrl(const GoogleUrl& input_base,
                                        const GoogleUrl& output_base,
                                        bool* proxy_mode) const {
  bool result = false;
  const UrlNamer* url_namer = server_context_->url_namer();
  bool proxying_on_output =
      (url_namer->ProxyMode() == UrlNamer::ProxyExtent::kFull);

  if (proxying_on_output) {
    result = true;
  } else if (input_base.AllExceptLeaf() != output_base.AllExceptLeaf()) {
    result = true;
  } else {
    const DomainLawyer* domain_lawyer = options()->domain_lawyer();
    result = domain_lawyer->WillDomainChange(input_base);
  }

  if (proxy_mode != NULL) {
    *proxy_mode = proxying_on_output;
  }

  return result;
}

PropertyPage* RewriteDriver::property_page() const {
  return fallback_property_page_ == NULL ?
      NULL : fallback_property_page_->actual_property_page();
}

PropertyPage* RewriteDriver::origin_property_page() const {
  return origin_property_page_.get();
}

// This is in the .cc rather than the header to avoid the need to
// include property_cache.h in the header.
void RewriteDriver::set_property_page(PropertyPage* page) {
  if (page == NULL) {
    set_fallback_property_page(NULL);
    return;
  }
  FallbackPropertyPage* fallback_page = new FallbackPropertyPage(page, NULL);
  set_fallback_property_page(fallback_page);
}

void RewriteDriver::set_fallback_property_page(FallbackPropertyPage* page) {
  if (owns_property_page_) {
    delete fallback_property_page_;
  }
  fallback_property_page_ = page;
  owns_property_page_ = true;
}

void RewriteDriver::set_unowned_fallback_property_page(
    FallbackPropertyPage* page) {
  if (owns_property_page_) {
    delete fallback_property_page_;
  }
  fallback_property_page_ = page;
  owns_property_page_ = false;
}

void RewriteDriver::set_origin_property_page(PropertyPage* page) {
  origin_property_page_.reset(page);
}

void RewriteDriver::increment_num_inline_preview_images() {
  ++num_inline_preview_images_;
}

StringPiece RewriteDriver::RefCategoryName(RefCategory cat) {
  switch (cat) {
    case kRefUser:
      return "User references";
    case kRefParsing:
      return "Parsing";
    case kRefPendingRewrites:
      return "Pending rewrites";
    case kRefDetachedRewrites:
      return "Detached rewrites";
    case kRefDeletingRewrites:
      return "Deleting rewrites";
    case kRefFetchUserFacing:
      return "User-facing fetch rewrite";
    case kRefFetchBackground:
      return "Background fetch rewrite";
    case kRefAsyncEvents:
      return "Misc async event";
    case kRefRenderBlockingAsyncEvents:
      return "Misc async event that's render-blocking";
    case kNumRefCategories:
      break;
  }
  LOG(DFATAL) << "Invalid argument to RefCategoryName" << cat;
  return "";
}

void RewriteDriver::LastRefRemoved() {
  if (!externally_managed_) {
    release_driver_ = true;
  } else {
    ref_counts_.DCheckAllCountsZeroMutexHeld();

    // In externally managed mode, we always keep at least one "user"
    // reference to the driver for our bookkeeping purposes.
    ref_counts_.AddRefMutexHeld(kRefUser);
  }
}

void RewriteDriver::DropReference(RefCategory ref_cat) {
  bool should_release = false;
  {
    ScopedMutex lock(rewrite_mutex());
    bool signal_cookie = PrepareShouldSignal();
    ref_counts_.ReleaseRefMutexHeld(ref_cat);
    should_release = release_driver_;
    SignalIfRequired(signal_cookie);
  }
  if (should_release) {
    PossiblyPurgeCachedResponseAndReleaseDriver();
  }
}

void RewriteDriver::IncrementAsyncEventsCount() {
  ref_counts_.AddRef(kRefAsyncEvents);
}

void RewriteDriver::DecrementAsyncEventsCount() {
  DropReference(kRefAsyncEvents);
}

void RewriteDriver::IncrementRenderBlockingAsyncEventsCount() {
  ref_counts_.AddRef(kRefRenderBlockingAsyncEvents);
}

void RewriteDriver::DecrementRenderBlockingAsyncEventsCount() {
  DropReference(kRefRenderBlockingAsyncEvents);
}

void RewriteDriver::EnableBlockingRewrite(RequestHeaders* request_headers) {
  if (!options()->blocking_rewrite_key().empty()) {
    const char* blocking_rewrite_key = request_headers->Lookup1(
        HttpAttributes::kXPsaBlockingRewrite);
    if (blocking_rewrite_key != NULL) {
      if (options()->blocking_rewrite_key() == blocking_rewrite_key) {
        set_fully_rewrite_on_flush(true);
      }
      // TODO(bharathbhushan): Allow for multiple PSAs on the request path by
      // interpreting the value as a comma separated list of keys and avoid
      // removing this header unconditionally.
      request_headers->RemoveAll(HttpAttributes::kXPsaBlockingRewrite);
    }
  }
  if (!fully_rewrite_on_flush() &&
      options()->IsBlockingRewriteRefererUrlPatternPresent()) {
    const char* referer = request_headers->Lookup1(
        HttpAttributes::kReferer);
    if (referer != NULL &&
        options()->IsBlockingRewriteEnabledForReferer(referer)) {
      set_fully_rewrite_on_flush(true);
    }
  }
  if (fully_rewrite_on_flush()) {
    const char* blocking_rewrite_mode(request_headers->Lookup1(
        HttpAttributes::kXPsaBlockingRewriteMode));
    if (blocking_rewrite_mode != NULL) {
      StringPiece mode(HttpAttributes::kXPsaBlockingRewriteModeSlow);
      if (blocking_rewrite_mode == mode) {
        // Don't wait for async events.
        set_fast_blocking_rewrite(false);
      }
      request_headers->RemoveAll(HttpAttributes::kXPsaBlockingRewriteMode);
    }
  }
}

RewriteDriver::XhtmlStatus RewriteDriver::MimeTypeXhtmlStatus() {
  if (!xhtml_mimetype_computed_ &&
      server_context_->response_headers_finalized() &&
      (response_headers_ != NULL)) {
    xhtml_mimetype_computed_ = true;
    const ContentType* content_type = response_headers_->DetermineContentType();
    if (content_type != NULL) {
      if (content_type->IsXmlLike()) {
        xhtml_status_ = kIsXhtml;
      } else {
        xhtml_status_ = kIsNotXhtml;
      }
    }
  }
  return xhtml_status_;
}

FlushEarlyInfo* RewriteDriver::flush_early_info() {
  if (flush_early_info_.get() == NULL) {
    PropertyCacheDecodeResult status;
    flush_early_info_.reset(DecodeFromPropertyCache<FlushEarlyInfo>(
        server_context()->page_property_cache(),
        fallback_property_page(),
        server_context()->dom_cohort(),
        kSubresourcesPropertyName,
        -1 /* no ttl checking*/,
        &status));
    if (status != kPropertyCacheDecodeOk) {
      flush_early_info_.reset(new FlushEarlyInfo);
    }
  }
  return flush_early_info_.get();
}

void RewriteDriver::InsertDebugComment(StringPiece unescaped,
                                       HtmlNode* node) {
  if (DebugMode() && node != NULL && IsRewritable(node)) {
    GoogleString escaped;
    HtmlKeywords::Escape(unescaped, &escaped);

    HtmlNode* comment_node = NewCommentNode(node->parent(), escaped);
    InsertNodeAfterNode(node, comment_node);
  }
}

void RewriteDriver::InsertDebugComments(
    const protobuf::RepeatedPtrField<GoogleString>& unescaped_messages,
    HtmlElement* element) {
  if (DebugMode() && element != NULL && IsRewritable(element)) {
    HtmlNode* preceding_node = element;
    for (protobuf::RepeatedPtrField<GoogleString>::const_iterator unescaped =
             unescaped_messages.begin();
         unescaped != unescaped_messages.end(); ++unescaped) {
      GoogleString escaped;
      HtmlKeywords::Escape(*unescaped, &escaped);

      HtmlNode* comment_node =
          NewCommentNode(preceding_node->parent(), escaped);
      InsertNodeAfterNode(preceding_node, comment_node);
      preceding_node = comment_node;
    }
  }
}

void RewriteDriver::InsertUnauthorizedDomainDebugComment(
    StringPiece url, InputRole role, HtmlElement* element) {
  if (DebugMode() && element != NULL && IsRewritable(element)) {
    GoogleUrl gurl(url);
    InsertNodeAfterNode(
        element,
        NewCommentNode(element->parent(),
                       GenerateUnauthorizedDomainDebugComment(gurl, role)));
  }
}

GoogleString RewriteDriver::GenerateUnauthorizedDomainDebugComment(
    const GoogleUrl& gurl, InputRole role) {
  GoogleString comment("The preceding resource was not rewritten because ");
  // Note: this is all being defensive - at the time of writing I believe
  // url will always be a valid URL.
  if (gurl.IsWebValid()) {
    StrAppend(&comment, "its domain (", gurl.Host(), ") is not authorized");
  } else if (gurl.IsWebOrDataValid()) {
    StrAppend(&comment, "it is a data URI");
  } else if (!IsLoadPermittedByCsp(gurl, role)) {
    StrAppend(&comment, "CSP disallows its fetch");
  } else {
    StrAppend(&comment, "it is not authorized");
  }
  GoogleString escaped;
  HtmlKeywords::Escape(comment, &escaped);
  return escaped;
}

bool RewriteDriver::is_critical_images_beacon_enabled() {
  return (options()->Enabled(RewriteOptions::kLazyloadImages) ||
          options()->Enabled(RewriteOptions::kInlineImages) ||
          options()->Enabled(RewriteOptions::kDelayImages) ||
          options()->Enabled(
              RewriteOptions::kResizeToRenderedImageDimensions)) &&
         options()->critical_images_beacon_enabled() &&
         server_context_->factory()->UseBeaconResultsInFilters() &&
         server_context_->page_property_cache()->enabled();
}

bool RewriteDriver::Write(const ResourceVector& inputs,
                          const StringPiece& contents,
                          const ContentType* type,
                          StringPiece charset,
                          OutputResource* output) {
  output->SetType(type);
  output->set_charset(charset);
  ResponseHeaders* meta_data = output->response_headers();
  bool clear_last_modified = false;

  // Transfer Last-Modified from the input for single-input on-the-fly
  // resources.
  if ((inputs.size() == 1) && (output->kind() == kOnTheFlyResource)) {
    const ResponseHeaders* input_headers = inputs[0]->response_headers();
    const char* last_modified = input_headers->Lookup1(
        HttpAttributes::kLastModified);
    if (last_modified == NULL) {
      clear_last_modified = true;
    } else {
      meta_data->Add(HttpAttributes::kLastModified, last_modified);
    }
  }

  server_context_->SetDefaultLongCacheHeaders(
      type, charset, output->cache_control_suffix(), meta_data);
  if (clear_last_modified) {
    meta_data->RemoveAll(HttpAttributes::kLastModified);
  }
  meta_data->SetStatusAndReason(HttpStatus::kOK);
  server_context_->ApplyInputCacheControl(inputs, meta_data);
  server_context_->AddOriginalContentLengthHeader(inputs, meta_data);

  // The URL for any resource we will write includes the hash of contents,
  // so it can can live, essentially, forever. So compute this hash,
  // and cache the output using meta_data's default headers which are to cache
  // forever.
  MessageHandler* handler = message_handler();
  Writer* writer = output->BeginWrite(handler);
  bool ret = (writer != NULL);
  if (ret) {
    ret = writer->Write(contents, handler);
    output->EndWrite(handler);

    HTTPCache* http_cache = server_context_->http_cache();
    if (output->kind() != kOnTheFlyResource &&
        output->kind() != kInlineResource &&
        (http_cache->force_caching() || meta_data->IsProxyCacheable())) {
      // This URL should already be mapped to the canonical rewrite domain,
      // But we should store its unsharded form in the cache.
      http_cache->Put(output->HttpCacheKey(), CacheFragment(),
                      RequestHeaders::Properties(),
                      options()->ComputeHttpOptions(),
                      &output->value_, handler);
    }

    // If we're asked to, also save a debug dump
    if (server_context_->store_outputs_in_file_system()) {
      output->DumpToDisk(handler);
    }

    // If our URL is derived from some pre-existing URL (and not invented by
    // us due to something like outlining), cache the mapping from original URL
    // to the constructed one.
    if (output->kind() == kRewrittenResource ||
        output->kind() == kOnTheFlyResource) {
      CachedResult* cached = output->EnsureCachedResultCreated();
      cached->set_optimizable(true);
      cached->set_url(output->url());  // Note: output->url() will be sharded.
    }
  } else {
    // Note that we've already gotten a "could not open file" message;
    // this just serves to explain why and suggest a remedy.
    handler->Message(kInfo, "Could not create output resource"
                     " (bad filename prefix '%s'?)",
                     server_context_->filename_prefix().as_string().c_str());
  }
  return ret;
}

void RewriteDriver::DetermineFiltersBehaviorImpl() {
  DetermineFilterListBehavior(early_pre_render_filters_);
  DetermineFilterListBehavior(pre_render_filters_);

  // Call parent to set up post render filters.
  HtmlParse::DetermineFiltersBehaviorImpl();
}

void RewriteDriver::ClearRequestProperties() {
  request_properties_.reset(new RequestProperties(
      server_context_->user_agent_matcher()));
}

const GoogleString& RewriteDriver::CacheFragment() const {
  CHECK(options_ != NULL);
  const GoogleString& fragment = options_->cache_fragment();
  if (!fragment.empty()) {
    return fragment;
  }
  CHECK(request_context_.get() != NULL) << "NULL request context in "
                                        << "RewriteDriver::CacheFragment";
  return request_context_->minimal_private_suffix();
}

bool RewriteDriver::SetOrClearPageSpeedOptionCookies(
    const GoogleUrl& gurl, ResponseHeaders* response_headers) {
  StringPiece required_token(options_->sticky_query_parameters());
  StringPiece provided_token(request_context_->sticky_query_parameters_token());
  // These are mutually exclusive but provide a way of specifying "do nothing".
  bool set_cookies = false;
  bool clear_cookies = false;

  if (options_->allow_options_to_be_set_by_cookies() &&
      !required_token.empty() &&
      required_token == provided_token) {
    // Make the current options sticky if we allow options to be set by
    // cookies (otherwise why bother?), there is a token specified in the
    // configuration, and the token specified in the request matches the
    // one in the configuration.
    set_cookies = true;
  } else if (!pagespeed_option_cookies_.empty() &&
             !required_token.empty() && !provided_token.empty() &&
             required_token != provided_token) {
    // Clear the current option cookies if there are any, there is a token
    // specified in the configuration, there is a token in the request, and
    // the token specified in the request does NOT match the one in the
    // configuration - treat that as a specific request to clear the cookies.
    clear_cookies = true;
  } else if (!pagespeed_option_cookies_.empty() &&
             !options_->allow_options_to_be_set_by_cookies()) {
    // Clear the current option cookies if there any but we no longer allow
    // options to be set by cookies.
    clear_cookies = true;
  }

  if (!set_cookies && !clear_cookies) {
    return false;
  }

  // We need to not set cookies for the option that triggered this.
  const GoogleString old_option_name(
      StrCat(RewriteQuery::kPageSpeed,
             RewriteOptions::kStickyQueryParameters));
  const GoogleString new_option_name(
      StrCat(RewriteQuery::kModPagespeed,
             RewriteOptions::kStickyQueryParameters));
  StringPieceVector exclusions;
  exclusions.push_back(old_option_name);
  exclusions.push_back(new_option_name);
  bool result = false;
  if (set_cookies) {
    int64 expiration_time_ms = (server_context()->timer()->NowMs() +
                                options_->option_cookies_duration_ms());
    result = response_headers->SetQueryParamsAsCookies(gurl,
                                                       pagespeed_query_params_,
                                                       exclusions,
                                                       expiration_time_ms);
  } else /* ASSERT: clear_cookies == true */ {
    result = response_headers->ClearOptionCookies(gurl,
                                                  pagespeed_option_cookies_,
                                                  exclusions);
  }
  if (result) {
    response_headers->ComputeCaching();
  }

  return result;
}

bool RewriteDriver::LookupMetadataForOutputResource(
    StringPiece url, GoogleString* error_out,
    RewriteContext::CacheLookupResultCallback* callback) {
  RewriteFilter* filter = NULL;
  GoogleUrl gurl(url);

  if (!gurl.IsWebValid()) {
    *error_out = "Unable to parse URL.";
    return false;
  }

  // The setup is different depending on if url is .pagespeed. resource or an
  // in-place rewritten one.
  bool is_pagespeed_resource = server_context_->IsPagespeedResource(gurl);

  SetBaseUrlForFetch(gurl.Spec());
  OutputResourcePtr output_resource;

  if (is_pagespeed_resource) {
    output_resource.reset(DecodeOutputResource(gurl, &filter));
  } else {
    StringPiece base = gurl.AllExceptLeaf();
    ResourceNamer namer;
    output_resource.reset(
        new OutputResource(this, base, base, base, namer, kRewrittenResource));
  }

  if (output_resource.get() == NULL ||
      (filter == NULL && is_pagespeed_resource)) {
    *error_out = "Unable to decode resource.";
    return false;
  }

  scoped_ptr<RewriteContext> context;
  if (is_pagespeed_resource) {
    context.reset(filter->MakeRewriteContext());
  } else {
    context.reset(new InPlaceRewriteContext(this, gurl.Spec()));
  }

  return RewriteContext::LookupMetadataForOutputResourceImpl(
             output_resource, gurl, context.release(),
             this, error_out, callback);
}

void RewriteDriver::RunTasksOnRequestThread() {
  // Note that we hold no locks when we make the decision about whether to
  // add rewrite tasks on the scheduler_sequence_, so RunTasksOnRequestThread
  // can only be called prior to running tasks.
  CHECK(!executing_rewrite_tasks_.value());
  scheduler_sequence_.reset(scheduler_->NewSequence());
}

void RewriteDriver::SwitchToQueuedWorkerPool() {
  scheduler_sequence_->ForwardToSequence(rewrite_worker_);
}

void RewriteDriver::CleanupRequestThread() {
  ScopedMutex lock(rewrite_mutex());
  scheduler_sequence_.reset();
}

Sequence* RewriteDriver::rewrite_worker() {
  if (scheduler_sequence_.get() == nullptr) {
    return rewrite_worker_;
  }
  return scheduler_sequence_.get();
}

void RewriteDriver::SetIsAmpDocument(bool is_amp) {
  if (is_amp) {
    DisableFiltersInjectingScripts(early_pre_render_filters_);
    DisableFiltersInjectingScripts(pre_render_filters_);
    HtmlParse::DisableFiltersInjectingScripts();
  }
  is_amp_ = is_amp;
  set_buffer_events(false);
}

bool RewriteDriver::IsLoadPermittedByCsp(
    const GoogleUrl& url, CspDirective role) {
  if (csp_context_.empty()) {
    return true;
  }

  return csp_context_.CanLoadUrl(role, google_url(), url);
}

bool RewriteDriver::IsLoadPermittedByCsp(const GoogleUrl& url, InputRole role) {
  switch (role) {
    case InputRole::kScript:
      return IsLoadPermittedByCsp(url, CspDirective::kScriptSrc);
    case InputRole::kStyle:
      return IsLoadPermittedByCsp(url, CspDirective::kStyleSrc);
    case InputRole::kImg:
      return IsLoadPermittedByCsp(url, CspDirective::kImgSrc);
    case InputRole::kUnknown:
      // Weird type, not sure what policy to check.
      return csp_context_.empty();
    case InputRole::kReconstruction:
      // All OK.
      return true;
  }
  LOG(DFATAL) << "Weird input as role= to IsLoadPermittedByCsp";
  return false;
}

}  // namespace net_instaweb
