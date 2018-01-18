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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_IN_PLACE_REWRITE_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_IN_PLACE_REWRITE_CONTEXT_H_

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/http/public/http_value_writer.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

class CachedResult;
class CacheUrlAsyncFetcher;
class HtmlElement;
class InputInfo;
class MessageHandler;
class ResourceContext;
class ResponseHeaders;
class RewriteDriver;
class RewriteFilter;
class Statistics;
class Variable;

// A resource-slot created for an in-place rewrite. This has an empty render
// method. Note that this class is usually used as a RefCountedPtr and gets
//  deleted when there are no references remaining.
class InPlaceRewriteResourceSlot : public ResourceSlot {
 public:
  static const char kIproSlotLocation[];
  explicit InPlaceRewriteResourceSlot(const ResourcePtr& resource);

  virtual HtmlElement* element() const { return NULL; }

  // Implements ResourceSlot::Render().
  virtual void Render();

  // Implements ResourceSlot::LocationString().
  virtual GoogleString LocationString() const;

 protected:
  virtual ~InPlaceRewriteResourceSlot();

 private:
  DISALLOW_COPY_AND_ASSIGN(InPlaceRewriteResourceSlot);
};

// Context that is used for an in-place rewrite.
class InPlaceRewriteContext : public SingleRewriteContext {
 public:
  // Stats variable name to keep track of how often in-place falls back to
  // stream (due to a large resource) when Options->in_place_wait_for_optimized
  // is true.
  static const char kInPlaceOversizedOptStream[];
  static const char kInPlaceUncacheableRewrites[];

  InPlaceRewriteContext(RewriteDriver* driver, const StringPiece& url);
  virtual ~InPlaceRewriteContext();

  // Implements SingleRewriteContext::RewriteSingle().
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  // Implements RewriteContext::id().
  virtual const char* id() const { return RewriteOptions::kInPlaceRewriteId; }
  // Implements RewriteContext::kind().
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  // Implements RewriteContext::DecodeFetchUrls().
  virtual bool DecodeFetchUrls(const OutputResourcePtr& output_resource,
                               MessageHandler* message_handler,
                               GoogleUrlStarVector* url_vector);
  // Implements RewriteContext::StartFetchReconstruction().
  virtual void StartFetchReconstruction();

  static void InitStats(Statistics* statistics);

  bool proxy_mode() const { return proxy_mode_; }
  void set_proxy_mode(bool x) { proxy_mode_ = x; }

  virtual int64 GetRewriteDeadlineAlarmMs() const;

  virtual GoogleString UserAgentCacheKey(
      const ResourceContext* resource_context) const;
  virtual void EncodeUserAgentIntoResourceContext(ResourceContext* context);

  // We don't lock for IPRO because IPRO would rather stream back the original
  // resource than wait for the optimization.
  virtual bool CreationLockBeforeStartFetch() const { return false; }

  // The context nested inside this context can be scheduled via the
  // CentralController. See comment in RewriteContext::ObtainLockForCreation.
  bool ScheduleNestedContextViaCentalController() const override {
    return true;
  }

 private:
  friend class RecordingFetch;
  bool PolicyPermitsRendering() const override;
  // Implements RewriteContext::Harvest().
  virtual void Harvest();
  void StartFetchReconstructionParent();
  // Implements RewriteContext::FixFetchFallbackHeaders().
  virtual void FixFetchFallbackHeaders(const CachedResult& cached_result,
                                       ResponseHeaders* headers);
  // Implements RewriteContext::FetchTryFallback().
  virtual void FetchTryFallback(const GoogleString& url,
                                const StringPiece& hash);
  // Implements RewriteContext::FetchCallbackDone().
  virtual void FetchCallbackDone(bool success);

  RewriteFilter* GetRewriteFilter(const ContentType& type);

  // Update the date and expiry time based on the InputInfo's.
  void UpdateDateAndExpiry(const protobuf::RepeatedPtrField<InputInfo>& inputs,
                           int64* date_ms, int64* expiry_ms);
  // Returns true if kInPlaceOptimizeForBrowser is enabled and we
  // actually need to do browser specific rewriting based on options.
  bool InPlaceOptimizeForBrowserEnabled() const;
  // Add a Vary: user-agent or Vary: Accept header as appropriate
  // if the fetch result may be browser dependent.
  void AddVaryIfRequired(const CachedResult& cached_result,
                         ResponseHeaders* headers) const;
  // Image rewriting adds a Link rel=canonical header.  Because a single cached
  // result can be served from multiple urls we do need to keep generating it.
  // But when serving via IPRO we should remove it if the url hasn't changed.
  void RemoveRedundantRelCanonicalHeader(const CachedResult& cached_result,
                                         ResponseHeaders* headers);

  // Returns true iff the single underlying resource is mapped by LoadFromFile.
  bool IsLoadFromFileBased();

  GoogleString url_;
  // Boolean indicating whether or not the resource was rewritten successfully.
  bool is_rewritten_;
  // The hash of the rewritten resource. Note that this should only be used if
  // is_rewritten_ is true. This may be empty.
  GoogleString rewritten_hash_;

  // Information needed for nested rewrites.
  ResourcePtr input_resource_;
  OutputResourcePtr output_resource_;

  scoped_ptr<CacheUrlAsyncFetcher> cache_fetcher_;

  // Are we in proxy mode?
  //
  // True means that we are acting as a proxy and the user is depending on us
  // to serve them the resource, thus we will fetch the contents over HTTP if
  // not found in cache and ignore kRecentFetchNotCacheable and
  // kRecentFetchFailed since we'll have to fetch the resource for users anyway.
  //
  // False means we are running on the origin, so we respect kRecent* messages
  // and let the origin itself serve the resource.
  bool proxy_mode_;

  DISALLOW_COPY_AND_ASSIGN(InPlaceRewriteContext);
};

// Records the fetch into the provided resource and passes through events to the
// underlying writer, response headers and callback.
class RecordingFetch : public SharedAsyncFetch {
 public:
  RecordingFetch(bool proxy_mode,
                 AsyncFetch* async_fetch,
                 const ResourcePtr& resource,
                 InPlaceRewriteContext* context,
                 int desired_s_maxage_sec,
                 MessageHandler* handler);

  virtual ~RecordingFetch();

  // Implements SharedAsyncFetch::HandleHeadersComplete().
  virtual void HandleHeadersComplete();
  // Implements SharedAsyncFetch::HandleWrite().
  virtual bool HandleWrite(const StringPiece& content, MessageHandler* handler);
  // Implements SharedAsyncFetch::HandleFlush().
  virtual bool HandleFlush(MessageHandler* handler);
  // Implements SharedAsyncFetch::HandleDone().
  virtual void HandleDone(bool success);

 private:
  void FreeDriver();

  bool CanInPlaceRewrite();

  // By default RecordingFetch streams back the original content to the browser.
  // If this returns false then the RecordingFetch should cache the original
  // content but not stream it.
  bool ShouldStream() const;

  bool proxy_mode_;
  MessageHandler* handler_;
  ResourcePtr resource_;
  InPlaceRewriteContext* context_;

  // If this is set to something other than -1, use it to set "Cache-Control:
  // s-maxage=..." on the unoptimized response we send back to the browser.  See
  // https://tools.ietf.org/html/rfc7234#section-5.2.2.9 for the s-maxage
  // specification.
  int desired_s_maxage_sec_;

  // True if resource is of rewritable type and is cacheable or if we're forcing
  // rewriting of uncacheable resources.
  bool can_in_place_rewrite_;

  // True if we're streaming data as it is being fetched.
  bool streaming_;
  HTTPValue cache_value_;
  HTTPValueWriter cache_value_writer_;
  scoped_ptr<ResponseHeaders> saved_headers_;
  Variable* in_place_oversized_opt_stream_;
  Variable* in_place_uncacheable_rewrites_;
  DISALLOW_COPY_AND_ASSIGN(RecordingFetch);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_IN_PLACE_REWRITE_CONTEXT_H_
