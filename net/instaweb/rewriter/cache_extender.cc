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


#include "net/instaweb/rewriter/public/cache_extender.h"

#include <memory>

#include "base/logging.h"
#include "net/instaweb/http/public/http_cache.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/srcset_slot.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {
class MessageHandler;

// names for Statistics variables.
const char CacheExtender::kCacheExtensions[] = "cache_extensions";
const char CacheExtender::kNotCacheable[] = "not_cacheable";

// We do not want to bother to extend the cache lifetime for any resource
// that is already cached for a month.
const int64 kMinThresholdMs = Timer::kMonthMs;

class CacheExtender::Context : public SingleRewriteContext {
 public:
  Context(RewriteDriver::InputRole input_role,
          CacheExtender* extender, RewriteDriver* driver,
          RewriteContext* parent)
      : SingleRewriteContext(driver, parent,
                             NULL /* no resource context */),
        input_role_(input_role), extender_(extender) {}
  virtual ~Context() {}

  bool PolicyPermitsRendering() const override;
  virtual void Render();
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return extender_->id(); }
  virtual OutputResourceKind kind() const { return kOnTheFlyResource; }

  virtual void FixFetchFallbackHeaders(const CachedResult& cached_result,
                               ResponseHeaders* headers) {
    SingleRewriteContext::FixFetchFallbackHeaders(cached_result, headers);
    if (num_slots() != 1 || slot(0)->resource().get() == NULL) {
      return;
    }
    ResourcePtr input_resource(slot(0)->resource());

    if (ShouldAddCanonical(input_resource)) {
      AddLinkRelCanonicalForFallbackHeaders(headers);
    }
  }

  // We only add link: rel = canonical to images and PDF; people don't normally
  // use search engines to look for .css and .js files, so adding it
  // there would just be a waste of bytes.
  bool ShouldAddCanonical(const ResourcePtr& input_resource) {
    return input_resource->type() != NULL &&
           (input_resource->type()->IsImage() ||
            input_resource->type()->type() == ContentType::kPdf);
  }

 private:
  RewriteDriver::InputRole input_role_;
  CacheExtender* extender_;
  DISALLOW_COPY_AND_ASSIGN(Context);
};

CacheExtender::CacheExtender(RewriteDriver* driver)
    : RewriteFilter(driver) {
  Statistics* stats = server_context()->statistics();
  extension_count_ = stats->GetVariable(kCacheExtensions);
  not_cacheable_count_ = stats->GetVariable(kNotCacheable);
}

CacheExtender::~CacheExtender() {}

void CacheExtender::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCacheExtensions);
  statistics->AddVariable(kNotCacheable);
}

bool CacheExtender::ShouldRewriteResource(
    const ResponseHeaders* headers, int64 now_ms,
    const ResourcePtr& input_resource, const StringPiece& url,
    CachedResult* result) const {
  const ContentType* input_resource_type = input_resource->type();
  if (input_resource_type == NULL) {
    return false;
  }
  if (input_resource_type->type() == ContentType::kJavascript &&
      driver()->options()->avoid_renaming_introspective_javascript() &&
      JavascriptCodeBlock::UnsafeToRename(
          input_resource->ExtractUncompressedContents())) {
    CHECK(result != NULL);
    result->add_debug_message(JavascriptCodeBlock::kIntrospectionComment);
    return false;
  }
  if ((headers->CacheExpirationTimeMs() - now_ms) < kMinThresholdMs) {
    // This also includes the case where a previous filter rewrote this.
    return true;
  }
  UrlNamer* url_namer = driver()->server_context()->url_namer();
  GoogleUrl origin_gurl(url);

  // We won't initiate a CacheExtender::Context with a pagespeed
  // resource URL.  However, an upstream filter might have rewritten
  // the resource after we queued the request, but before our
  // context is asked to rewrite it.  So we have to check again now
  // that the resource URL is finalized.
  if (server_context()->IsPagespeedResource(origin_gurl)) {
    return false;
  }

  if (url_namer->ProxyMode() == UrlNamer::ProxyExtent::kFull) {
    return !url_namer->IsProxyEncoded(origin_gurl);
  }
  const DomainLawyer* lawyer = driver()->options()->domain_lawyer();

  // We return true for IsProxyMapped because when reconstructing
  // MAPPED_DOMAIN/file.pagespeed.ce.HASH.ext we won't be changing
  // the domain (WillDomainChange==false) but we want this function
  // to return true so that we can reconstruct the cache-extension and
  // serve the result with long public caching.  Without IsProxyMapped,
  // we'd serve the result with cache-control:private,max-age=300.
  return (lawyer->IsProxyMapped(origin_gurl) ||
          lawyer->WillDomainChange(origin_gurl));
}

void CacheExtender::StartElementImpl(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver()->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    bool may_load = false;
    RewriteDriver::InputRole input_role = RewriteDriver::InputRole::kUnknown;
    switch (attributes[i].category) {
      case semantic_type::kStylesheet:
        may_load = driver()->MayCacheExtendCss();
        input_role = RewriteDriver::InputRole::kStyle;
        break;
      case semantic_type::kImage:
        may_load = driver()->MayCacheExtendImages();
        input_role = RewriteDriver::InputRole::kImg;
        break;
      case semantic_type::kScript:
        may_load = driver()->MayCacheExtendScripts();
        input_role = RewriteDriver::InputRole::kScript;
        break;
      default:
        // Does the url in the attribute end in .pdf, ignoring query params?
        if (attributes[i].url->DecodedValueOrNull() != NULL
            && driver()->MayCacheExtendPdfs()) {
        GoogleUrl url(driver()->base_url(),
                      attributes[i].url->DecodedValueOrNull());
        if (url.IsWebValid() && StringCaseEndsWith(
                url.LeafSansQuery(), kContentTypePdf.file_extension())) {
          may_load = true;
        }
      }
      break;
    }
    if (!may_load) {
      continue;
    }

    // TODO(jmarantz): We ought to be able to domain-shard even if the
    // resources are non-cacheable or privately cacheable.
    if (driver()->IsRewritable(element)) {
      ResourcePtr input_resource(CreateInputResourceOrInsertDebugComment(
          attributes[i].url->DecodedValueOrNull(), input_role, element));
      if (input_resource.get() == NULL) {
        continue;
      }

      GoogleUrl input_gurl(input_resource->url());
      if (server_context()->IsPagespeedResource(input_gurl)) {
        continue;
      }

      ResourceSlotPtr slot(driver()->GetSlot(
          input_resource, element, attributes[i].url));
      Context* context = new Context(input_role, this, driver(),
                                     nullptr /* not nested */);
      context->AddSlot(slot);
      driver()->InitiateRewrite(context);
    }
  }

  if (element->keyword() == HtmlName::kImg &&
      driver()->MayCacheExtendImages()) {
    HtmlElement::Attribute* srcset = element->FindAttribute(HtmlName::kSrcset);
    if (srcset != nullptr) {
      SrcSetSlotCollectionPtr slot_collection(
          driver()->GetSrcSetSlotCollection(this, element, srcset));
      for (int i = 0; i < slot_collection->num_image_candidates(); ++i) {
        SrcSetSlot* slot = slot_collection->slot(i);
        // slot will be null if resource could not be created due to URL parsing
        // or being against our policy (not authorized domain, etc).
        if (slot == nullptr) {
          continue;
        }
        Context* context = new Context(
              RewriteDriver::InputRole::kImg, this,
              driver(), nullptr /* !nested */);
        context->AddSlot(RefCountedPtr<ResourceSlot>(slot));
        driver()->InitiateRewrite(context);
      }
    }
  }
}

bool CacheExtender::ComputeOnTheFly() const {
  return true;
}

void CacheExtender::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  if (ShouldAddCanonical(input_resource)) {
    AddLinkRelCanonical(input_resource, output_resource->response_headers());
  }
  RewriteDone(
      extender_->RewriteLoadedResource(
          input_resource, output_resource, mutable_output_partition(0)), 0);
}

bool CacheExtender::Context::PolicyPermitsRendering() const {
  if (num_output_partitions() == 1 && output(0).get() != nullptr
      && output(0)->has_hash()) {
    // This uses the InputRole rather than CspDirective variant to
    // handle kUnknown (and to get bonus handling of kReconstruction,
    // which wouldn't actually call this, but for which we still need to
    // override).
    return Driver()->IsLoadPermittedByCsp(
        GoogleUrl(output(0)->url()), input_role_);
  }
  return true;  // e.g. failure cases -> still want to permit error to render.
}

void CacheExtender::Context::Render() {
  if (num_output_partitions() == 1 && output_partition(0)->optimizable()) {
    extender_->extension_count_->Add(1);
    // Log applied rewriter id. Here, we care only about non-nested
    // cache extensions, and that too, those occurring in synchronous
    // flows only.
    if (Driver() != NULL) {
      ResourceSlotPtr the_slot = slot(0);
      if (the_slot->resource().get() != NULL &&
          the_slot->resource()->type() != NULL) {
        const char* filter_id = id();
        const ContentType* type = the_slot->resource()->type();
        if (type->type() == ContentType::kCss) {
          filter_id = RewriteOptions::FilterId(
              RewriteOptions::kExtendCacheCss);
        } else if (type->type() == ContentType::kJavascript) {
          filter_id = RewriteOptions::FilterId(
              RewriteOptions::kExtendCacheScripts);
        } else if (type->IsImage()) {
          filter_id = RewriteOptions::FilterId(
              RewriteOptions::kExtendCacheImages);
        }
        // TODO(anupama): Log cache extension for pdfs etc.
        Driver()->log_record()->SetRewriterLoggingStatus(
            filter_id,
            the_slot->resource()->url(),
            RewriterApplication::APPLIED_OK);
      }
    }
  }
}

RewriteResult CacheExtender::RewriteLoadedResource(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource,
    // TODO(jmaessen): does this belong in CacheExtender::Context? to this
    // method and ShouldRewriteResource.
    CachedResult* result) {
  CHECK(input_resource->loaded());

  MessageHandler* message_handler = driver()->message_handler();
  const ResponseHeaders* headers = input_resource->response_headers();
  GoogleString url = input_resource->url();
  int64 now_ms = server_context()->timer()->NowMs();

  // See if the resource is cacheable; and if so whether there is any need
  // to cache extend it.
  bool ok = false;
  // Assume that it may have cookies; see comment in
  // CacheableResourceBase::IsValidAndCacheableImpl.
  RequestHeaders::Properties req_properties;
  const ContentType* output_type = NULL;
  if (!server_context()->http_cache()->force_caching() &&
      !headers->IsProxyCacheable(
          req_properties,
          ResponseHeaders::GetVaryOption(driver()->options()->respect_vary()),
          ResponseHeaders::kNoValidator)) {
    // Note: RewriteContextTest.PreserveNoCacheWithFailedRewrites
    // relies on CacheExtender failing rewrites in this case.
    // If you change this behavior that test MUST be updated as it covers
    // security.
    not_cacheable_count_->Add(1);
  } else if (ShouldRewriteResource(
      headers, now_ms, input_resource, url, result)) {
    // We must be careful what Content-Types we allow to be cache extended.
    // Specifically, we do not want to cache extend any Content-Types that
    // could execute scripts when loaded in a browser because that could
    // open XSS vectors in case of system misconfiguration.
    //
    // In particular, if somehow a.com/b.com (incorrectly) authorize each other
    // as trusted in the DomainLawyer an external fetch of
    // a.com/,hb.com,_evil.html.pagespeed.ce.html, would run b.com's content
    // inside a.com's domain, getting access to a.com frames.
    //
    // We whitelist a set of safe Content-Types here.
    //
    // TODO(sligocki): Should we whitelist more Content-Types as well?
    // We would also have to find and rewrite the URLs to these resources
    // if we want to cache extend them.
    const ContentType* input_type = input_resource->type();
    if (input_type->IsImage() ||  // images get sniffed only to other images
        (input_type->type() == ContentType::kPdf &&
         driver()->MayCacheExtendPdfs()) ||  // Don't accept PDFs by default.
        input_type->type() == ContentType::kCss ||  // CSS + JS left as-is.
        input_type->type() == ContentType::kJavascript) {
      output_type = input_type;
      ok = true;
    } else {
      // Fail to cache extend a file that isn't an approved type.
      ok = false;

      // If we decide not to fail to cache extend unapproved types, we
      // should convert their Content-Type to text/plain because as per
      // http://mimesniff.spec.whatwg.org/ it will never get turned into
      // anything dangerous.
      output_type = &kContentTypeText;
    }
  }

  if (!ok) {
    return kRewriteFailed;
  }

  StringPiece contents(input_resource->ExtractUncompressedContents());
  GoogleString transformed_contents;
  StringWriter writer(&transformed_contents);
  GoogleUrl input_resource_gurl(input_resource->url());
  if (output_type->type() == ContentType::kCss) {
    switch (driver()->ResolveCssUrls(input_resource_gurl,
                                     output_resource->resolved_base(),
                                     contents, &writer, message_handler)) {
      case RewriteDriver::kNoResolutionNeeded:
        break;
      case RewriteDriver::kWriteFailed:
        return kRewriteFailed;
      case RewriteDriver::kSuccess:
        // TODO(jmarantz): find a mechanism to write this directly into
        // the HTTPValue so we can reduce the number of times that we
        // copy entire resources.
        contents = transformed_contents;
        break;
    }
  }

  server_context()->MergeNonCachingResponseHeaders(
      input_resource, output_resource);
  if (driver()->Write(ResourceVector(1, input_resource),
                      contents,
                      output_type,
                      input_resource->charset(),
                      output_resource.get())) {
    return kRewriteOk;
  } else {
    return kRewriteFailed;
  }
}

RewriteContext* CacheExtender::MakeRewriteContext() {
  return new Context(RewriteDriver::InputRole::kReconstruction, this,
                     driver(), nullptr /*not nested*/);
}

RewriteContext* CacheExtender::MakeNestedContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  Context* context = new Context(
      RewriteDriver::InputRole::kUnknown, this, nullptr /* driver*/, parent);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
