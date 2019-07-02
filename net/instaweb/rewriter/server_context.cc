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


#include "net/instaweb/rewriter/public/server_context.h"

#include <algorithm>                   // for std::binary_search
#include <cstddef>                     // for size_t
#include <set>

#include "base/logging.h"               // for operator<<, etc
#include "net/instaweb/config/rewrite_options_manager.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/input_info.pb.h"
#include "net/instaweb/rewriter/public/beacon_critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/critical_selector_finder.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_driver_pool.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/rewriter/rendered_image.pb.h"
#include "net/instaweb/util/public/cache_property_store.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/named_lock_manager.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"          // for STLDeleteElements
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/thread/thread_synchronizer.h"
#include "pagespeed/opt/http/property_store.h"

namespace net_instaweb {

class RewriteFilter;

namespace {

// Define the various query parameter keys sent by instrumentation beacons.
const char kBeaconUrlQueryParam[] = "url";
const char kBeaconEtsQueryParam[] = "ets";
const char kBeaconOptionsHashQueryParam[] = "oh";
const char kBeaconCriticalImagesQueryParam[] = "ci";
const char kBeaconRenderedDimensionsQueryParam[] = "rd";
const char kBeaconCriticalCssQueryParam[] = "cs";
const char kBeaconNonceQueryParam[] = "n";

// Attributes that should not be automatically copied from inputs to outputs
const char* kExcludedAttributes[] = {
  HttpAttributes::kCacheControl,
  HttpAttributes::kContentEncoding,
  HttpAttributes::kContentLength,
  HttpAttributes::kContentType,
  HttpAttributes::kDate,
  HttpAttributes::kEtag,
  HttpAttributes::kExpires,
  HttpAttributes::kLastModified,
  // Rewritten resources are publicly cached, so we should avoid cookies
  // which are generally meant for private data.
  HttpAttributes::kSetCookie,
  HttpAttributes::kSetCookie2,
  HttpAttributes::kTransferEncoding,
  HttpAttributes::kVary
};

StringSet* CommaSeparatedStringToSet(StringPiece str) {
  StringPieceVector str_values;
  // Note that 'str' must be unescaped before calling this function, because
  // "," is technically supposed to be escaped in URL query parameters, per
  // http://en.wikipedia.org/wiki/Query_string#URL_encoding.
  SplitStringPieceToVector(str, ",", &str_values, true);
  StringSet* set = new StringSet();
  for (StringPieceVector::const_iterator it = str_values.begin();
       it != str_values.end(); ++it) {
    set->insert(it->as_string());
  }
  return set;
}

// Track a property cache lookup triggered from a beacon response. When
// complete, Done will update and and writeback the beacon cohort with the
// critical image set.
class BeaconPropertyCallback : public PropertyPage {
 public:
  BeaconPropertyCallback(ServerContext* server_context, StringPiece url,
                         StringPiece options_signature_hash,
                         UserAgentMatcher::DeviceType device_type,
                         const RequestContextPtr& request_context,
                         StringSet* html_critical_images_set,
                         StringSet* css_critical_images_set,
                         StringSet* critical_css_selector_set,
                         RenderedImages* rendered_images_set, StringPiece nonce)
      : PropertyPage(kPropertyCachePage, url, options_signature_hash,
                     UserAgentMatcher::DeviceTypeSuffix(device_type),
                     request_context,
                     server_context->thread_system()->NewMutex(),
                     server_context->page_property_cache()),
        server_context_(server_context),
        html_critical_images_set_(html_critical_images_set),
        css_critical_images_set_(css_critical_images_set),
        critical_css_selector_set_(critical_css_selector_set),
        rendered_images_set_(rendered_images_set) {
    nonce.CopyToString(&nonce_);
  }

  const PropertyCache::CohortVector CohortList() {
    PropertyCache::CohortVector cohort_list;
    cohort_list.push_back(
         server_context_->page_property_cache()->GetCohort(
             RewriteDriver::kBeaconCohort));
    return cohort_list;
  }

  virtual ~BeaconPropertyCallback() {}

  virtual void Done(bool success) {
    // TODO(jud): Clean up the call to UpdateCriticalImagesCacheEntry with a
    // struct to nicely package up all of the pcache arguments.
    BeaconCriticalImagesFinder::UpdateCriticalImagesCacheEntry(
        html_critical_images_set_.get(), css_critical_images_set_.get(),
        rendered_images_set_.get(), nonce_, server_context_->beacon_cohort(),
        this, server_context_->timer());
    if (critical_css_selector_set_ != NULL) {
      BeaconCriticalSelectorFinder::
          WriteCriticalSelectorsToPropertyCacheFromBeacon(
              *critical_css_selector_set_, nonce_,
              server_context_->page_property_cache(),
              server_context_->beacon_cohort(), this,
              server_context_->message_handler(), server_context_->timer());
    }

    WriteCohort(server_context_->beacon_cohort());
    delete this;
  }

 private:
  ServerContext* server_context_;
  scoped_ptr<StringSet> html_critical_images_set_;
  scoped_ptr<StringSet> css_critical_images_set_;
  scoped_ptr<StringSet> critical_css_selector_set_;
  scoped_ptr<RenderedImages> rendered_images_set_;
  GoogleString nonce_;
  DISALLOW_COPY_AND_ASSIGN(BeaconPropertyCallback);
};

}  // namespace

const int64 ServerContext::kGeneratedMaxAgeMs = Timer::kYearMs;
const int64 ServerContext::kCacheTtlForMismatchedContentMs =
    5 * Timer::kMinuteMs;

// Our HTTP cache mostly stores full URLs, including the http: prefix,
// mapping them into the URL contents and HTTP headers.  However, we
// also put name->hash mappings into the HTTP cache, and we prefix
// these with "ResourceName:" to disambiguate them.
//
// Cache entries prefixed this way map the base name of a resource
// into the hash-code of the contents.  This mapping has a TTL based
// on the minimum TTL of the input resources used to construct the
// resource.  After that TTL has expired, we will need to re-fetch the
// resources from their origin, and recompute the hash.
//
// Whenever we change the hashing function we can bust caches by
// changing this prefix.
//
// TODO(jmarantz): inject the SVN version number here to automatically bust
// caches whenever pagespeed is upgraded.
const char ServerContext::kCacheKeyResourceNamePrefix[] = "rname/";

// We set etags for our output resources to "W/0".  The "W" means
// that this etag indicates a functional consistency, but is not
// guaranteeing byte-consistency.  This distinction is important because
// we serve different bytes for clients that do not accept gzip.
//
// This value is a shared constant so that it can also be used in
// the Apache-specific code that repairs headers after mod_headers
// alters them.
const char ServerContext::kResourceEtagValue[] = "W/\"0\"";

class GlobalOptionsRewriteDriverPool : public RewriteDriverPool {
 public:
  explicit GlobalOptionsRewriteDriverPool(ServerContext* context)
      : server_context_(context) {
  }

  virtual const RewriteOptions* TargetOptions() const {
    return server_context_->global_options();
  }

 private:
  ServerContext* server_context_;
};

ServerContext::ServerContext(RewriteDriverFactory* factory)
    : thread_system_(factory->thread_system()),
      rewrite_stats_(NULL),
      file_system_(factory->file_system()),
      url_namer_(NULL),
      user_agent_matcher_(NULL),
      scheduler_(factory->scheduler()),
      default_system_fetcher_(NULL),
      hasher_(NULL),
      signature_(NULL),
      lock_hasher_(RewriteOptions::kHashBytes),
      contents_hasher_(21),
      statistics_(NULL),
      timer_(NULL),
      filesystem_metadata_cache_(NULL),
      metadata_cache_(NULL),
      store_outputs_in_file_system_(false),
      response_headers_finalized_(true),
      enable_property_cache_(true),
      lock_manager_(NULL),
      message_handler_(NULL),
      dom_cohort_(NULL),
      beacon_cohort_(NULL),
      dependencies_cohort_(NULL),
      fix_reflow_cohort_(NULL),
      available_rewrite_drivers_(new GlobalOptionsRewriteDriverPool(this)),
      trying_to_cleanup_rewrite_drivers_(false),
      shutdown_drivers_called_(false),
      factory_(factory),
      rewrite_drivers_mutex_(thread_system_->NewMutex()),
      decoding_driver_(NULL),
      html_workers_(NULL),
      rewrite_workers_(NULL),
      low_priority_rewrite_workers_(NULL),
      static_asset_manager_(NULL),
      thread_synchronizer_(new ThreadSynchronizer(thread_system_)),
      experiment_matcher_(factory_->NewExperimentMatcher()),
      usage_data_reporter_(factory_->usage_data_reporter()),
      simple_random_(thread_system_->NewMutex()),
      js_tokenizer_patterns_(factory_->js_tokenizer_patterns()) {
  // Make sure the excluded-attributes are in abc order so binary_search works.
  // Make sure to use the same comparator that we pass to the binary_search.
#ifndef NDEBUG
  for (int i = 1, n = arraysize(kExcludedAttributes); i < n; ++i) {
    DCHECK(CharStarCompareInsensitive()(kExcludedAttributes[i - 1],
                                        kExcludedAttributes[i]));
  }
#endif
}

ServerContext::~ServerContext() {
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());

    // Actually release anything that got deferred above.
    trying_to_cleanup_rewrite_drivers_ = false;
    for (RewriteDriverSet::iterator i =
             deferred_release_rewrite_drivers_.begin();
         i != deferred_release_rewrite_drivers_.end(); ++i) {
      ReleaseRewriteDriverImpl(*i);
    }
    deferred_release_rewrite_drivers_.clear();
  }

  // We scan for "leaked_rewrite_drivers" in install/Makefile.tests
  if (!active_rewrite_drivers_.empty()) {
    message_handler_->Message(
#ifdef NDEBUG
        kInfo,
#else
        kError,
#endif
        "ServerContext: %d leaked_rewrite_drivers on destruction",
        static_cast<int>(active_rewrite_drivers_.size()));
#ifndef NDEBUG
    for (RewriteDriverSet::iterator p = active_rewrite_drivers_.begin(),
             e = active_rewrite_drivers_.end(); p != e; ++p) {
      RewriteDriver* driver = *p;
      // During load-test, print some detail about leaked drivers.  It
      // appears that looking deep into the leaked driver's detached
      // contexts crashes during shutdown, however, so disable that.
      //
      // TODO(jmarantz): investigate why that is so we can show the detail.
      driver->PrintStateToErrorLog(false /* show_detached_contexts */);
    }
#endif
  }
  STLDeleteElements(&active_rewrite_drivers_);
  available_rewrite_drivers_.reset();
  STLDeleteElements(&additional_driver_pools_);
}

// TODO(gee): These methods are out of order with respect to the .h #tech-debt
void ServerContext::InitWorkers() {
  html_workers_ = factory_->WorkerPool(RewriteDriverFactory::kHtmlWorkers);
  rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kRewriteWorkers);
  low_priority_rewrite_workers_ = factory_->WorkerPool(
      RewriteDriverFactory::kLowPriorityRewriteWorkers);
}

void ServerContext::PostInitHook() {
  InitWorkers();
}

void ServerContext::SetDefaultLongCacheHeaders(
    const ContentType* content_type, StringPiece charset,
    StringPiece suffix, ResponseHeaders* header) const {
  header->set_major_version(1);
  header->set_minor_version(1);
  header->SetStatusAndReason(HttpStatus::kOK);

  header->RemoveAll(HttpAttributes::kContentType);
  if (content_type != NULL) {
    GoogleString header_val = content_type->mime_type();
    if (!charset.empty()) {
      // Note: if charset was quoted, content_type's parsing would not unquote
      // it, so here we just append it back in instead of quoting it again.
      StrAppend(&header_val, "; charset=", charset);
    }
    header->Add(HttpAttributes::kContentType, header_val);
  }

  int64 now_ms = timer()->NowMs();
  header->SetDateAndCaching(now_ms, kGeneratedMaxAgeMs, suffix);

  // While PageSpeed claims the "Vary" header is needed to avoid proxy cache
  // issues for clients where some accept gzipped content and some don't, it
  // should not be done here.  It should instead be done by whatever code is
  // conditionally gzipping the content based on user-agent, e.g. mod_deflate.
  // header->Add(HttpAttributes::kVary, HttpAttributes::kAcceptEncoding);

  // ETag is superfluous for mod_pagespeed as we sign the URL with the
  // content hash.  However, we have seen evidence that IE8 will not
  // serve images from its cache when the image lacks an ETag.  Since
  // we sign URLs, there is no reason to have a unique signature in
  // the ETag.
  header->Replace(HttpAttributes::kEtag, kResourceEtagValue);

  // TODO(jmarantz): Replace last-modified headers by default?
  ConstStringStarVector v;
  if (!header->Lookup(HttpAttributes::kLastModified, &v)) {
    header->SetLastModified(now_ms);
  }

  // TODO(jmarantz): Page-speed suggested adding a "Last-Modified" header
  // for cache validation.  To do this we must track the max of all
  // Last-Modified values for all input resources that are used to
  // create this output resource.  For now we are using the current
  // time.

  header->ComputeCaching();
}

void ServerContext::MergeNonCachingResponseHeaders(
    const ResponseHeaders& input_headers,
    ResponseHeaders* output_headers) {
  for (int i = 0, n = input_headers.NumAttributes(); i < n; ++i) {
    const GoogleString& name = input_headers.Name(i);
    if (!IsExcludedAttribute(name.c_str())) {
      output_headers->Add(name, input_headers.Value(i));
    }
  }
}

void ServerContext::set_filename_prefix(const StringPiece& file_prefix) {
  file_prefix.CopyToString(&file_prefix_);
}

void ServerContext::ApplyInputCacheControl(const ResourceVector& inputs,
                                           ResponseHeaders* headers) {
  headers->ComputeCaching();

  // We always turn off respect_vary in this context, as this is being
  // used to clean up the headers of a generated resource, to which we
  // may have applied vary:user-agent if (for example) we are transcoding
  // to webp during in-place resource optimization.
  //
  // TODO(jmarantz): Add a suite of tests to ensure that we are preserving
  // Vary headers from inputs to output, or converting them to
  // cache-control:private if needed.
  bool proxy_cacheable = headers->IsProxyCacheable(
      RequestHeaders::Properties(),
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kHasValidator);

  bool browser_cacheable = headers->IsBrowserCacheable();
  bool no_store = headers->HasValue(HttpAttributes::kCacheControl, "no-store");
  bool is_public = true;  // Only used if we see a non-empty resource.
  bool saw_nonempty_resource = false;
  int64 max_age = headers->cache_ttl_ms();
  for (int i = 0, n = inputs.size(); i < n; ++i) {
    const ResourcePtr& input_resource(inputs[i]);
    if (input_resource.get() != NULL && input_resource->HttpStatusOk()) {
      ResponseHeaders* input_headers = input_resource->response_headers();
      input_headers->ComputeCaching();
      if (input_headers->cache_ttl_ms() < max_age) {
        max_age = input_headers->cache_ttl_ms();
      }
      bool resource_cacheable = input_headers->IsProxyCacheable(
          RequestHeaders::Properties(),
          ResponseHeaders::kIgnoreVaryOnResources,
          ResponseHeaders::kHasValidator);
      proxy_cacheable &= resource_cacheable;
      browser_cacheable &= input_headers->IsBrowserCacheable();
      no_store |= input_headers->HasValue(HttpAttributes::kCacheControl,
                                          "no-store");
      is_public &= input_headers->HasValue(HttpAttributes::kCacheControl,
                                           "public");
      saw_nonempty_resource = true;
    }
  }
  DCHECK(!(proxy_cacheable && !browser_cacheable)) <<
      "You can't have a proxy-cacheable result that is not browser-cacheable";
  if (proxy_cacheable) {
    if (is_public && saw_nonempty_resource) {
      headers->SetCacheControlPublic();
    }
  } else {
    const char* directives = NULL;
    if (browser_cacheable) {
      directives = ",private";
    } else {
      max_age = 0;
      directives = no_store ? ",no-cache,no-store" : ",no-cache";
    }
    headers->SetDateAndCaching(headers->date_ms(), max_age, directives);
    headers->Remove(HttpAttributes::kEtag, kResourceEtagValue);
    headers->ComputeCaching();
  }
}

void ServerContext::AddOriginalContentLengthHeader(
    const ResourceVector& inputs, ResponseHeaders* headers) {
  // Determine the total original content length for input resource, and
  // use this to set the X-Original-Content-Length header in the output.
  int64 input_size = 0;
  bool all_known = !inputs.empty();
  for (int i = 0, n = inputs.size(); i < n; ++i) {
    const ResourcePtr& input_resource(inputs[i]);
    ResponseHeaders* input_headers = input_resource->response_headers();
    const char* original_content_length_header = input_headers->Lookup1(
        HttpAttributes::kXOriginalContentLength);
    int64 original_content_length;
    if (original_content_length_header != NULL &&
        StringToInt64(original_content_length_header,
                      &original_content_length)) {
      input_size += original_content_length;
    } else if (input_resource->loaded()) {
      input_size += input_resource->UncompressedContentsSize();
    } else {
      all_known = false;
    }
  }
  // Only add the header if there were actual input resources with
  // known sizes involved (which is not always the case, e.g., in tests where
  // synthetic input resources are used).
  if (all_known) {
    headers->SetOriginalContentLength(input_size);
  }
}

bool ServerContext::IsPagespeedResource(const GoogleUrl& url) const {
  ResourceNamer namer;
  OutputResourceKind kind;
  RewriteFilter* filter;
  return decoding_driver_->DecodeOutputResourceName(
      url, global_options(), url_namer(), &namer, &kind, &filter);
}

const RewriteFilter* ServerContext::FindFilterForDecoding(
    const StringPiece& id) const {
  return decoding_driver_->FindFilter(id);
}

bool ServerContext::DecodeUrlGivenOptions(const GoogleUrl& url,
                                          const RewriteOptions* options,
                                          const UrlNamer* url_namer,
                                          StringVector* decoded_urls) const {
  return decoding_driver_->DecodeUrlGivenOptions(url, options, url_namer,
                                                 decoded_urls);
}

NamedLock* ServerContext::MakeCreationLock(const GoogleString& name) {
  const char kLockSuffix[] = ".outputlock";

  GoogleString lock_name = StrCat(lock_hasher_.Hash(name), kLockSuffix);
  return lock_manager_->CreateNamedLock(lock_name);
}

namespace {
// Constants governing resource lock timeouts.
// TODO(jmaessen): Set more appropriately?
const int64 kBreakLockMs = 30 * Timer::kSecondMs;
const int64 kBlockLockMs = 5 * Timer::kSecondMs;
}  // namespace

void ServerContext::TryLockForCreation(NamedLock* creation_lock,
                                       Function* callback) {
  return creation_lock->LockTimedWaitStealOld(
      0 /* wait_ms */, kBreakLockMs, callback);
}

void ServerContext::LockForCreation(NamedLock* creation_lock,
                                    Sequence* worker,
                                    Function* callback) {
  // TODO(jmaessen): It occurs to me that we probably ought to be
  // doing something like this if we *really* care about lock aging:
  // if (!creation_lock->LockTimedWaitStealOld(kBlockLockMs,
  //                                           kBreakLockMs)) {
  //   creation_lock->TryLockStealOld(0);  // Force lock steal
  // }
  // This updates the lock hold time so that another thread is less likely
  // to steal the lock while we're doing the blocking rewrite.
  creation_lock->LockTimedWaitStealOld(
      kBlockLockMs, kBreakLockMs,
      new QueuedWorkerPool::Sequence::AddFunction(worker, callback));
}

bool ServerContext::HandleBeacon(StringPiece params,
                                 StringPiece user_agent,
                                 const RequestContextPtr& request_context) {
  // Beacons are of the form ets=load:xxx&url=.... and can be sent in either the
  // query params of a GET or the body of a POST.
  // Extract the URL. A valid URL parameter is required to attempt parsing of
  // the ets and critimg params. However, an invalid ets or critimg param will
  // not prevent attempting parsing of the other. This is because these values
  // are generated by separate client-side JS and that failure of one should not
  // prevent attempting to parse the other.
  QueryParams query_params;
  query_params.ParseFromUntrustedString(params);
  GoogleString query_param_str;
  GoogleUrl url_query_param;

  // If the beacon was sent by the mobilization filter, then just return true.
  // TODO(jud): Handle these beacons and add some statistics and tracking for
  // them.
  if (query_params.Lookup1Unescaped("id", &query_param_str) &&
      (query_param_str == "psmob")) {
    return true;
  }

  if (query_params.Lookup1Unescaped(kBeaconUrlQueryParam, &query_param_str)) {
    url_query_param.Reset(query_param_str);

    if (!url_query_param.IsWebValid()) {
      message_handler_->Message(kWarning,
                                "Invalid URL parameter in beacon: %s",
                                query_param_str.c_str());
      return false;
    }
  } else {
    message_handler_->Message(kWarning, "Missing URL parameter in beacon: %s",
                              params.as_string().c_str());
    return false;
  }

  bool status = true;

  // Extract the onload time from the ets query param.
  if (query_params.Lookup1Unescaped(kBeaconEtsQueryParam, &query_param_str)) {
    int value = -1;

    size_t index = query_param_str.find(":");
    if (index != GoogleString::npos && index < query_param_str.size()) {
      GoogleString load_time_str = query_param_str.substr(index + 1);
      if (!(StringToInt(load_time_str, &value) && value >= 0)) {
        status = false;
      } else {
        rewrite_stats_->total_page_load_ms()->Add(value);
        rewrite_stats_->page_load_count()->Add(1);
        rewrite_stats_->beacon_timings_ms_histogram()->Add(value);
      }
    }
  }

  // Process data from critical image and CSS beacons.
  // Beacon contents are stored in the property cache, so bail out if it isn't
  // enabled.
  if (page_property_cache() == NULL || !page_property_cache()->enabled()) {
    return status;
  }
  // Make sure the beacon has the options hash, which is included in the
  // property cache key.
  GoogleString options_hash_param;
  if (!query_params.Lookup1Unescaped(kBeaconOptionsHashQueryParam,
                                     &options_hash_param)) {
    return status;
  }

  // Extract critical image URLs
  // TODO(jud): Add css critical image detection to the beacon.
  // Beacon property callback takes ownership of both critical images sets.
  scoped_ptr<StringSet> html_critical_images_set;
  scoped_ptr<StringSet> css_critical_images_set;
  if (query_params.Lookup1Unescaped(kBeaconCriticalImagesQueryParam,
                                    &query_param_str)) {
    html_critical_images_set.reset(
        CommaSeparatedStringToSet(query_param_str));
  }

  scoped_ptr<StringSet> critical_css_selector_set;
  if (query_params.Lookup1Unescaped(kBeaconCriticalCssQueryParam,
                                    &query_param_str)) {
    critical_css_selector_set.reset(
        CommaSeparatedStringToSet(query_param_str));
  }

  scoped_ptr<RenderedImages> rendered_images;
  if (query_params.Lookup1Unescaped(kBeaconRenderedDimensionsQueryParam,
                                    &query_param_str)) {
    rendered_images.reset(
        critical_images_finder_->JsonMapToRenderedImagesMap(
            query_param_str, global_options()));
  }

  StringPiece nonce;
  if (query_params.Lookup1Unescaped(kBeaconNonceQueryParam, &query_param_str)) {
    nonce.set(query_param_str.data(), query_param_str.size());
  }

  // Store the critical information in the property cache. This is done by
  // looking up the property page for the URL specified in the beacon, and
  // performing the page update and cohort write in
  // BeaconPropertyCallback::Done(). Done() is called when the read completes.
  if (html_critical_images_set != NULL || css_critical_images_set != NULL ||
      critical_css_selector_set != NULL || rendered_images != NULL) {
    UserAgentMatcher::DeviceType device_type =
        user_agent_matcher()->GetDeviceTypeForUA(user_agent);

    BeaconPropertyCallback* beacon_property_cb = new BeaconPropertyCallback(
        this,
        url_query_param.Spec(),
        options_hash_param,
        device_type,
        request_context,
        html_critical_images_set.release(),
        css_critical_images_set.release(),
        critical_css_selector_set.release(),
        rendered_images.release(),
        nonce);
    page_property_cache()->ReadWithCohorts(beacon_property_cb->CohortList(),
                                           beacon_property_cb);
  }

  return status;
}

// TODO(jmaessen): Note that we *could* re-structure the
// rewrite_driver freelist code as follows: Keep a
// std::vector<RewriteDriver*> of all rewrite drivers.  Have each
// driver hold its index in the vector (as a number or iterator).
// Keep index of first in use.  To free, swap with first in use,
// adjusting indexes, and increment first in use.  To allocate,
// decrement first in use and return that driver.  If first in use was
// 0, allocate a fresh driver and push it.
//
// The benefit of Jan's idea is that we could avoid the overhead
// of keeping the RewriteDrivers in a std::set, which has log n
// insert/remove behavior, and instead get constant time and less
// memory overhead.

RewriteDriver* ServerContext::NewCustomRewriteDriver(
    RewriteOptions* options, const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = NewUnmanagedRewriteDriver(
      NULL /* no pool as custom*/,
      options,
      request_ctx);
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    active_rewrite_drivers_.insert(rewrite_driver);
  }
  if (factory_ != NULL) {
    factory_->ApplyPlatformSpecificConfiguration(rewrite_driver);
  }
  rewrite_driver->AddFilters();
  if (factory_ != NULL) {
    factory_->AddPlatformSpecificRewritePasses(rewrite_driver);
  }
  return rewrite_driver;
}

RewriteDriver* ServerContext::NewUnmanagedRewriteDriver(
    RewriteDriverPool* pool, RewriteOptions* options,
    const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = new RewriteDriver(
      message_handler_, file_system_, default_system_fetcher_);
  rewrite_driver->set_options_for_pool(pool, options);
  rewrite_driver->SetServerContext(this);
  rewrite_driver->ClearRequestProperties();
  rewrite_driver->set_request_context(request_ctx);
  // Set the initial reference, as the expectation is that the client
  // will need to call Cleanup() or FinishParse()
  rewrite_driver->AddUserReference();

  ApplySessionFetchers(request_ctx, rewrite_driver);
  return rewrite_driver;
}

RewriteDriver* ServerContext::NewRewriteDriver(
    const RequestContextPtr& request_ctx) {
  return NewRewriteDriverFromPool(standard_rewrite_driver_pool(), request_ctx);
}

RewriteDriver* ServerContext::NewRewriteDriverFromPool(
    RewriteDriverPool* pool, const RequestContextPtr& request_ctx) {
  RewriteDriver* rewrite_driver = NULL;

  const RewriteOptions* options = pool->TargetOptions();
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    while ((rewrite_driver = pool->PopDriver()) != NULL) {
      // Note: there is currently some activity to make the RewriteOptions
      // signature insensitive to changes that need not affect the metadata
      // cache key.  As we are dependent on a comprehensive signature in
      // order to correctly determine whether we can recycle a RewriteDriver,
      // we would have to use a separate signature for metadata_cache_key
      // vs this purpose.
      //
      // So for now, let us keep all the options incorporated into the
      // signature, and revisit the issue of pulling options out if we
      // find we are having poor hit-rate in the metadata cache during
      // operations.
      if (rewrite_driver->options()->IsEqual(*options)) {
        break;
      } else {
        delete rewrite_driver;
        rewrite_driver = NULL;
      }
    }
  }

  if (rewrite_driver == NULL) {
    rewrite_driver = NewUnmanagedRewriteDriver(
        pool, options->Clone(), request_ctx);
    if (factory_ != NULL) {
      factory_->ApplyPlatformSpecificConfiguration(rewrite_driver);
    }
    rewrite_driver->AddFilters();
    if (factory_ != NULL) {
      factory_->AddPlatformSpecificRewritePasses(rewrite_driver);
    }
  } else {
    rewrite_driver->AddUserReference();
    rewrite_driver->set_request_context(request_ctx);
    ApplySessionFetchers(request_ctx, rewrite_driver);
  }

  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    active_rewrite_drivers_.insert(rewrite_driver);
  }
  return rewrite_driver;
}

void ServerContext::ReleaseRewriteDriver(RewriteDriver* rewrite_driver) {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  ReleaseRewriteDriverImpl(rewrite_driver);
}

void ServerContext::ReleaseRewriteDriverImpl(RewriteDriver* rewrite_driver) {
  if (trying_to_cleanup_rewrite_drivers_) {
    deferred_release_rewrite_drivers_.insert(rewrite_driver);
    return;
  }

  int count = active_rewrite_drivers_.erase(rewrite_driver);
  if (count != 1) {
    LOG(DFATAL) << "ReleaseRewriteDriver called with driver not in active set.";
  } else {
    RewriteDriverPool* pool = rewrite_driver->controlling_pool();
    if (pool == NULL) {
      delete rewrite_driver;
    } else {
      pool->RecycleDriver(rewrite_driver);
    }
  }
}

void ServerContext::ShutDownDrivers(int64 cutoff_time_ms) {
  // Try to get any outstanding rewrites to complete, one-by-one.
  {
    ScopedMutex lock(rewrite_drivers_mutex_.get());
    // Prevent any rewrite completions from directly deleting drivers or
    // affecting active_rewrite_drivers_. We can now release the lock so
    // that the rewrites can call ReleaseRewriteDriver. Note that this is
    // making an assumption that we're not allocating new rewrite drivers
    // during the shutdown.
    trying_to_cleanup_rewrite_drivers_ = true;
  }

  // Don't do this twice if subclassing of RewriteDriverFactory causes us
  // to get called twice.
  // TODO(morlovich): Fix the ShutDown code to not get run many times instead.
  if (shutdown_drivers_called_) {
    return;
  }
  shutdown_drivers_called_ = true;

  if (!active_rewrite_drivers_.empty()) {
    message_handler_->Message(kInfo, "%d rewrite(s) still ongoing at exit",
                              static_cast<int>(active_rewrite_drivers_.size()));
  }

  // In the startup phase, we can be shutdown without having had a timer set.
  // In that case we'll have no drivers, so we just bail.
  if (active_rewrite_drivers_.empty()) {
    return;
  }

  for (RewriteDriver* active : active_rewrite_drivers_) {
    // <= 0 wait means forever, so we must guard against that.
    int64 wait_ms = cutoff_time_ms - timer_->NowMs();
    if (wait_ms <= 0) {
      wait_ms = 1;
    }
    active->BoundedWaitFor(RewriteDriver::kWaitForShutDown, wait_ms);
    // Note: It is not safe to call Cleanup() on the driver here. Something
    // else is planning to do that and if it happens after this point, they
    // can DCHECK fail because the refcount is already 0. Instead we just cross
    // our fingers and wait. If the driver is still active by the time we get
    // to the destructor, we will log a warning and force delete it.
  }
}

size_t ServerContext::num_active_rewrite_drivers() {
  ScopedMutex lock(rewrite_drivers_mutex_.get());
  return active_rewrite_drivers_.size();
}

RewriteOptions* ServerContext::global_options() {
  if (base_class_options_.get() == NULL) {
    base_class_options_.reset(factory_->default_options()->Clone());
  }
  return base_class_options_.get();
}

const RewriteOptions* ServerContext::global_options() const {
  if (base_class_options_.get() == NULL) {
    return factory_->default_options();
  }
  return base_class_options_.get();
}

void ServerContext::reset_global_options(RewriteOptions* options) {
  base_class_options_.reset(options);
}

RewriteOptions* ServerContext::NewOptions() {
  return factory_->NewRewriteOptions();
}

void ServerContext::GetRemoteOptions(RewriteOptions* remote_options,
                                     bool on_startup) {
  if (remote_options == NULL) {
    return;
  }
  HttpOptions fetch_options;
  fetch_options.implicit_cache_ttl_ms =
      remote_options->implicit_cache_ttl_ms();
  fetch_options.respect_vary = false;
  if (!remote_options->remote_configuration_url().empty()) {
    RequestContextPtr request_ctx(new RequestContext(
        fetch_options, thread_system()->NewMutex(), timer()));
    GoogleString config =
        FetchRemoteConfig(remote_options->remote_configuration_url(),
                          remote_options->remote_configuration_timeout_ms(),
                          on_startup, request_ctx);
    if (!on_startup) {
      ApplyRemoteConfig(config, remote_options);
    }
  }
}

bool ServerContext::GetQueryOptions(
    const RequestContextPtr& request_context,
    const RewriteOptions* domain_options, GoogleUrl* request_url,
    RequestHeaders* request_headers, ResponseHeaders* response_headers,
    RewriteQuery* rewrite_query) {
  if (!request_url->IsWebValid()) {
    message_handler_->Message(kError, "GetQueryOptions: Invalid URL: %s",
                              request_url->spec_c_str());
    return false;
  }
  if (domain_options == NULL) {
    domain_options = global_options();
  }
  // Note: success==false is treated as an error (we return 405 in
  // proxy_interface.cc).
  return RewriteQuery::IsOK(rewrite_query->Scan(
      domain_options->add_options_to_urls(),
      domain_options->allow_options_to_be_set_by_cookies(),
      domain_options->request_option_override(), request_context, factory(),
      this, request_url, request_headers, response_headers, message_handler_));
}

// TODO(gee): Seems like this should all be in RewriteOptionsManager.
RewriteOptions* ServerContext::GetCustomOptions(RequestHeaders* request_headers,
                                                RewriteOptions* domain_options,
                                                RewriteOptions* query_options) {
  RewriteOptions* options = global_options();
  scoped_ptr<RewriteOptions> custom_options;
  scoped_ptr<RewriteOptions> scoped_domain_options(domain_options);
  if (scoped_domain_options.get() != NULL) {
    custom_options.reset(NewOptions());
    custom_options->Merge(*options);
    scoped_domain_options->Freeze();
    custom_options->Merge(*scoped_domain_options);
    options = custom_options.get();
  }

  scoped_ptr<RewriteOptions> query_options_ptr(query_options);
  // Check query params & request-headers
  if (query_options_ptr.get() != NULL) {
    // Subtle memory management to handle deleting any domain_options
    // after the merge, and transferring ownership to the caller for
    // the new merged options.
    scoped_ptr<RewriteOptions> options_buffer(custom_options.release());
    custom_options.reset(NewOptions());
    custom_options->Merge(*options);
    query_options->Freeze();
    custom_options->Merge(*query_options);
    // Don't run any experiments if this is a special query-params request,
    // unless EnrollExperiment is on.
    if (!custom_options->enroll_experiment()) {
      custom_options->set_running_experiment(false);
    }
  }

  url_namer()->ConfigureCustomOptions(*request_headers, custom_options.get());

  return custom_options.release();
}

GoogleString ServerContext::GetRewriteOptionsSignatureHash(
    const RewriteOptions* options) {
  if (options == NULL) {
    return "";
  }
  return hasher()->Hash(options->signature());
}

void ServerContext::ComputeSignature(RewriteOptions* rewrite_options) const {
  rewrite_options->ComputeSignature();
}

void ServerContext::SetRewriteOptionsManager(RewriteOptionsManager* rom) {
  rewrite_options_manager_.reset(rom);
}

bool ServerContext::IsExcludedAttribute(const char* attribute) {
  const char** end = kExcludedAttributes + arraysize(kExcludedAttributes);
  return std::binary_search(kExcludedAttributes, end, attribute,
                            CharStarCompareInsensitive());
}

void ServerContext::set_enable_property_cache(bool enabled) {
  enable_property_cache_ = enabled;
  if (page_property_cache_.get() != NULL) {
    page_property_cache_->set_enabled(enabled);
  }
}

void ServerContext::MakePagePropertyCache(PropertyStore* property_store) {
  PropertyCache* pcache = new PropertyCache(
      property_store,
      timer(),
      statistics(),
      thread_system_);
  // TODO(pulkitg): Remove set_enabled method from property_cache.
  pcache->set_enabled(enable_property_cache_);
  page_property_cache_.reset(pcache);
}

void ServerContext::set_critical_images_finder(CriticalImagesFinder* finder) {
  critical_images_finder_.reset(finder);
}

void ServerContext::set_critical_selector_finder(
    CriticalSelectorFinder* finder) {
  critical_selector_finder_.reset(finder);
}

void ServerContext::ApplySessionFetchers(const RequestContextPtr& req,
                                         RewriteDriver* driver) {
}

RequestProperties* ServerContext::NewRequestProperties() {
  RequestProperties* request_properties =
      new RequestProperties(user_agent_matcher());
  return request_properties;
}

void ServerContext::DeleteCacheOnDestruction(CacheInterface* cache) {
  factory_->TakeOwnership(cache);
}

const PropertyCache::Cohort* ServerContext::AddCohort(
    const GoogleString& cohort_name,
    PropertyCache* pcache) {
  return AddCohortWithCache(cohort_name, NULL, pcache);
}

const PropertyCache::Cohort* ServerContext::AddCohortWithCache(
    const GoogleString& cohort_name,
    CacheInterface* cache,
    PropertyCache* pcache) {
  CHECK(pcache->GetCohort(cohort_name) == NULL) << cohort_name
                                                << " is added twice.";
  if (cache_property_store_ != NULL) {
    if (cache != NULL) {
      cache_property_store_->AddCohortWithCache(cohort_name, cache);
    } else {
      cache_property_store_->AddCohort(cohort_name);
    }
  }
  return pcache->AddCohort(cohort_name);
}

void ServerContext::set_cache_property_store(CachePropertyStore* p) {
  cache_property_store_.reset(p);
}

PropertyStore* ServerContext::CreatePropertyStore(
    CacheInterface* cache_backend) {
  CachePropertyStore* cache_property_store =
      new CachePropertyStore(CachePropertyStore::kPagePropertyCacheKeyPrefix,
                             cache_backend,
                             timer(),
                             statistics(),
                             thread_system_);
  set_cache_property_store(cache_property_store);
  return cache_property_store;
}

const CacheInterface* ServerContext::pcache_cache_backend() {
  if (cache_property_store_ == NULL) {
    return NULL;
  }
  return cache_property_store_->cache_backend();
}

namespace {

void FormatResponse(ServerContext::Format format,
                    const GoogleString& html,
                    const GoogleString& text,
                    AsyncFetch* fetch,
                    MessageHandler* handler) {
  ResponseHeaders* response_headers = fetch->response_headers();
  response_headers->SetStatusAndReason(HttpStatus::kOK);
  response_headers->Add(HttpAttributes::kCacheControl,
                        HttpAttributes::kNoStore);
  response_headers->Add(RewriteQuery::kPageSpeed, "off");

  if (format == ServerContext::kFormatAsHtml) {
    response_headers->Add(HttpAttributes::kContentType, "text/html");
    fetch->Write(html, handler);
    HtmlKeywords::WritePre(text, "", fetch, handler);
  } else {
    response_headers->Add(
        HttpAttributes::kContentType, "application/javascript; charset=utf-8");
    response_headers->Add("X-Content-Type-Options", "nosniff");
    // Prevent some cases of improper embedding of data, which risks
    // misinterpreting it.
    response_headers->Add("Content-Disposition",
                          "attachment; filename=\"data.json\"");
    fetch->Write(")]}\n", handler);

    GoogleString escaped;
    EscapeToJsonStringLiteral(text, true, &escaped);
    fetch->Write(StrCat("{\"value\":", escaped, "}"), handler);
  }
  fetch->Done(true);
}

class MetadataCacheResultCallback
    : public RewriteContext::CacheLookupResultCallback {
 public:
  // Will cleanup the driver
  MetadataCacheResultCallback(ServerContext::Format format,
                              bool should_delete,
                              StringPiece url,
                              StringPiece ua,
                              ServerContext* server_context,
                              RewriteDriver* driver,
                              AsyncFetch* fetch,
                              MessageHandler* handler)
      : format_(format),
        should_delete_(should_delete),
        url_(url.as_string()),
        ua_(ua.as_string()),
        server_context_(server_context),
        driver_(driver),
        fetch_(fetch),
        handler_(handler) {
  }

  virtual ~MetadataCacheResultCallback() {}

  virtual void Done(const GoogleString& cache_key,
                    RewriteContext::CacheLookupResult* in_result) {
    scoped_ptr<RewriteContext::CacheLookupResult> result(in_result);
    driver_->Cleanup();

    if (should_delete_) {
      server_context_->metadata_cache()->Delete(cache_key);
    }

    // Add a little form for delete button if OK. Careful: html is html,
    // so quoting is our responsibility here.
    GoogleString html;
    if (result->cache_ok && !should_delete_) {
      html = "<form><input type=hidden name=url value=\"";
      GoogleString escaped_url;
      HtmlKeywords::Escape(url_, &escaped_url);
      StrAppend(&html, escaped_url);
      StrAppend(&html, "\">");
      if (!ua_.empty()) {
        StrAppend(&html, "<input type=hidden name=user_agent value=\"");
        GoogleString escaped_ua;
        HtmlKeywords::Escape(ua_, &escaped_ua);
        StrAppend(&html, escaped_ua);
        StrAppend(&html, "\">");
      }
      StrAppend(&html, "<input type=submit name=Delete value=Delete>");
    } else if (should_delete_) {
      html = "<i>Delete request sent to cache.</i>";
    }

    GoogleString cache_dump;
    StringWriter cache_writer(&cache_dump);
    cache_writer.Write(StrCat("Metadata cache key:", cache_key, "\n"),
                       handler_);
    cache_writer.Write(StrCat("cache_ok:",
                              (result->cache_ok ? "true" : "false"),
                         "\n"), handler_);
    cache_writer.Write(
        StrCat("can_revalidate:", (result->can_revalidate ? "true" : "false"),
        "\n"), handler_);
    if (result->partitions.get() != NULL) {
      // Display the input info which has the minimum expiration time of all
      // the inputs.
      cache_writer.Write(
          StrCat("partitions:", result->partitions->DebugString(), "\n"),
          handler_);
    } else {
      cache_writer.Write("partitions is NULL\n", handler_);
    }
    for (int i = 0, n = result->revalidate.size(); i < n; ++i) {
      cache_writer.Write(StrCat("Revalidate entry ", IntegerToString(i), " ",
                                result->revalidate[i]->DebugString(), "\n"),
                    handler_);
    }
    FormatResponse(format_, html, cache_dump, fetch_, handler_);
    delete this;
  }

 private:
  ServerContext::Format format_;
  bool should_delete_;
  GoogleString url_;
  GoogleString ua_;
  ServerContext* server_context_;
  RewriteDriver* driver_;
  AsyncFetch* fetch_;
  MessageHandler* handler_;
};

}  // namespace

void ServerContext::ShowCacheHandler(
    Format format,  StringPiece url, StringPiece ua, bool should_delete,
    AsyncFetch* fetch, RewriteOptions* options_arg) {
  scoped_ptr<RewriteOptions> options(options_arg);
  if (url.empty()) {
    FormatResponse(format, "", "Empty URL", fetch, message_handler_);
  } else if (!GoogleUrl(url).IsWebValid()) {
    FormatResponse(format, "", "Invalid URL", fetch, message_handler_);
  } else {
    RewriteDriver* driver = NewCustomRewriteDriver(
        options.release(), fetch->request_context());
    fetch->request_headers()->Replace(HttpAttributes::kUserAgent, ua);
    driver->SetRequestHeaders(*fetch->request_headers());

    GoogleString error_out;
    MetadataCacheResultCallback* callback = new MetadataCacheResultCallback(
        format, should_delete, url, ua, this, driver, fetch, message_handler_);
    if (!driver->LookupMetadataForOutputResource(url, &error_out, callback)) {
      driver->Cleanup();
      delete callback;
      FormatResponse(format, "", error_out, fetch, message_handler_);
    }
  }
}

GoogleString ServerContext::FetchRemoteConfig(const GoogleString& url,
                                              int64 timeout_ms,
                                              bool on_startup,
                                              RequestContextPtr request_ctx) {
  CHECK(!url.empty());
  // Set up the fetcher.
  GoogleString out_str;
  StringWriter out_writer(&out_str);
  SyncFetcherAdapterCallback* remote_config_fetch =
      new SyncFetcherAdapterCallback(thread_system_, &out_writer, request_ctx);
  CacheUrlAsyncFetcher remote_config_fetcher(
      hasher(), lock_manager(), http_cache(),
      global_options()->cache_fragment(), NULL, DefaultSystemFetcher());
  remote_config_fetcher.set_proactively_freshen_user_facing_request(true);
  // Fetch to a string.
  remote_config_fetcher.Fetch(url, message_handler_, remote_config_fetch);
  if (on_startup) {
    remote_config_fetch->Release();
    return "";
  }
  // Now block waiting for the callback for up to timeout_ms milliseconds.
  bool locked_ok = remote_config_fetch->LockIfNotReleased();
  if (!locked_ok) {
    message_handler_->Message(kWarning, "Failed to take fetch lock.");
    remote_config_fetch->Release();
    return "";
  }
  int64 now_ms = timer_->NowMs();
  for (int64 end_ms = now_ms + timeout_ms;
       !remote_config_fetch->IsDoneLockHeld() && (now_ms < end_ms);
       now_ms = timer_->NowMs()) {
    int64 remaining_ms = std::max(static_cast<int64>(0), end_ms - now_ms);
    remote_config_fetch->TimedWait(remaining_ms);
  }
  remote_config_fetch->Unlock();

  if (!remote_config_fetch->success()) {
    message_handler_->Message(
        kWarning, "Fetching remote configuration %s failed.", url.c_str());
    remote_config_fetch->Release();
    return "";
  } else if (remote_config_fetch->response_headers()->status_code() !=
             HttpStatus::kNotModified) {
    message_handler_->Message(
        kWarning,
        "Fetching remote configuration %s. Configuration was not in cache.",
        url.c_str());
  }
  remote_config_fetch->Release();
  return out_str;
}

void ServerContext::ApplyConfigLine(StringPiece linesp,
                                    RewriteOptions* options) {
  // Strip whitespace from beginning and end of the line.
  TrimWhitespace(&linesp);
  // Ignore comments after stripping whitespace.
  // Comments must be on their own line.
  if (linesp.size() == 0 || linesp[0] == '#') {
    return;
  }
  // Split on the first space.
  StringPiece::size_type space = linesp.find(' ');
  if (space != StringPiece::npos) {
    StringPiece name = linesp.substr(0, space);
    StringPiece value = linesp.substr(space + 1);
    // Strip whitespace from the value.
    TrimWhitespace(&value);
    // Apply the options.
    GoogleString msg;
    RewriteOptions::OptionSettingResult result =
        options->ParseAndSetOptionFromNameWithScope(
            name, value, RewriteOptions::kDirectoryScope, &msg,
            message_handler_);
    if (result != RewriteOptions::kOptionOk) {
      // Continue applying remaining options.
      message_handler_->Message(
          kWarning, "Setting option %s with value %s failed: %s",
          name.as_string().c_str(), value.as_string().c_str(), msg.c_str());
    }
  }
}

void ServerContext::ApplyRemoteConfig(const GoogleString& config,
                                      RewriteOptions* options) {
  // Split the remote config file line by line, and apply each line with
  // ServerContext::ApplyConfigLine
  StringPieceVector str_values;
  int cfg_complete = 0;
  SplitStringPieceToVector(config, "\n", &str_values,
                           true /*omit empty strings*/);
  // If the configuration file does not contain "EndRemoteConfig", discard the
  // entire configuration.
  for (int i = 0, n = str_values.size(); i < n; ++i) {
    if (str_values[i].starts_with("EndRemoteConfig")) {
      cfg_complete = i;
      break;
    }
  }
  if (cfg_complete == 0) {
    message_handler_->Message(kWarning,
                              "Remote Configuration end token not received.");
    return;
  }
  for (int i = 0, n = cfg_complete; i < n; ++i) {
    ApplyConfigLine(str_values[i], options);
  }
}

GoogleString ServerContext::ShowCacheForm(StringPiece user_agent) {
  GoogleString ua_default;
  if (!user_agent.empty()) {
    GoogleString buf;
    ua_default = StrCat("value=\"", HtmlKeywords::Escape(user_agent, &buf),
                        "\" ");
  }
  // The styling on this form could use some love, but the 110/103 sizing
  // is to make those input fields decently wide to fit large URLs and UAs
  // and to roughly line up.
  GoogleString out = StrCat(
      "<form>\n",
      "  URL: <input id=metadata_text type=text name=url size=110 /><br>\n"
      "  User-Agent: <input id=user_agent type=text size=103 name=user_agent ",
      ua_default,
      "/><br> \n",
      "  <input id=metadata_submit type=submit "
      "   value='Show Metadata Cache Entry' />"
      "  <input id=metadata_clear type=reset value='Clear' />",
      "</form>\n");
  return out;
}

GoogleString ServerContext::FormatOption(StringPiece option_name,
                                         StringPiece args) {
  return StrCat(option_name, " ", args);
}

CacheUrlAsyncFetcher* ServerContext::CreateCustomCacheFetcher(
    const RewriteOptions* options, const GoogleString& fragment,
    CacheUrlAsyncFetcher::AsyncOpHooks* hooks, UrlAsyncFetcher* fetcher) {
  CacheUrlAsyncFetcher* cache_fetcher = new CacheUrlAsyncFetcher(
      lock_hasher(),
      lock_manager(),
      http_cache(),
      fragment,
      hooks,
      fetcher);
  RewriteStats* stats = rewrite_stats();
  cache_fetcher->set_respect_vary(options->respect_vary());
  cache_fetcher->set_default_cache_html(options->default_cache_html());
  cache_fetcher->set_backend_first_byte_latency_histogram(
      stats->backend_latency_histogram());
  cache_fetcher->set_fallback_responses_served(
      stats->fallback_responses_served());
  cache_fetcher->set_fallback_responses_served_while_revalidate(
      stats->fallback_responses_served_while_revalidate());
  cache_fetcher->set_num_conditional_refreshes(
      stats->num_conditional_refreshes());
  cache_fetcher->set_serve_stale_if_fetch_error(
      options->serve_stale_if_fetch_error());
  cache_fetcher->set_proactively_freshen_user_facing_request(
      options->proactively_freshen_user_facing_request());
  cache_fetcher->set_num_proactively_freshen_user_facing_request(
      stats->num_proactively_freshen_user_facing_request());
  cache_fetcher->set_serve_stale_while_revalidate_threshold_sec(
      options->serve_stale_while_revalidate_threshold_sec());
  return cache_fetcher;
}

}  // namespace net_instaweb
