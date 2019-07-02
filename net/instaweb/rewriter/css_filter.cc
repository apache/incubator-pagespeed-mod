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


#include "net/instaweb/rewriter/public/css_filter.h"

#include <algorithm>                    // for std::merge.
#include <map>
#include <utility>                      // for pair
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/association_transformer.h"
#include "net/instaweb/rewriter/public/css_absolutify.h"
#include "net/instaweb/rewriter/public/css_flatten_imports_context.h"
#include "net/instaweb/rewriter/public/css_hierarchy.h"
#include "net/instaweb/rewriter/public/css_image_rewriter.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_url_counter.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/inline_attribute_slot.h"
#include "net/instaweb/rewriter/public/inline_output_resource.h"
#include "net/instaweb/rewriter/public/inline_resource_slot.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/usage_data_reporter.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/util/simple_random.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/log_record.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

class CacheExtender;
class ImageCombineFilter;

namespace {

const char kInlineCspMessage[] =
    "Avoiding modifying inline style with CSP present";

// A simple transformer that resolves URLs against a base. Unlike
// RewriteDomainTransformer, does not do any mapping or trimming.
class SimpleAbsolutifyTransformer : public CssTagScanner::Transformer {
 public:
  explicit SimpleAbsolutifyTransformer(const GoogleUrl* base_url)
      : base_url_(base_url) {}
  virtual ~SimpleAbsolutifyTransformer() {}

  virtual TransformStatus Transform(GoogleString* str) {
    GoogleUrl abs(*base_url_, *str);
    if (abs.IsWebValid()) {
      abs.Spec().CopyToString(str);
      return kSuccess;
    } else {
      return kNoChange;
    }
  }

 private:
  const GoogleUrl* base_url_;
  DISALLOW_COPY_AND_ASSIGN(SimpleAbsolutifyTransformer);
};

// All of the options that can affect image optimization can also affect
// CSS rewriting, due to embedded images.  We will merge those in during
// Initialize.  There are additional options that affect CSS files.  Notably,
// image inlining does not affect the http* URLs of images, but it does affect
// the URLs of CSS files because images inlined into CSS changes the hash.
const RewriteOptions::Filter kRelatedFilters[] = {
  RewriteOptions::kExtendCacheCss,
  RewriteOptions::kExtendCacheImages,
  RewriteOptions::kFallbackRewriteCssUrls,
  RewriteOptions::kFlattenCssImports,
  RewriteOptions::kInlineImages,
  RewriteOptions::kLeftTrimUrls,
  RewriteOptions::kRewriteDomains,
  RewriteOptions::kSpriteImages,
};
const int kRelatedFiltersSize = arraysize(kRelatedFilters);

const char* const kRelatedOptions[] = {
  RewriteOptions::kCssFlattenMaxBytes,
  RewriteOptions::kCssImageInlineMaxBytes,
  RewriteOptions::kCssPreserveURLs,
  RewriteOptions::kImagePreserveURLs,
  RewriteOptions::kMaxUrlSegmentSize,
  RewriteOptions::kMaxUrlSize,
};

bool IsInlineResource(const ResourcePtr& resource) {
  // InlineOutputResources have no URL, but original inline resources are
  // stored as DataUrlInputResources, thus have data url()
  // TODO(sligocki): Harmonize these all to use the same method.
  return (!resource->has_url() || IsDataUrl(resource->url()));
}

}  // namespace

const RewriteOptions::Filter* CssFilter::merged_filters_ = NULL;
int CssFilter::merged_filters_size_ = 0;

StringPieceVector* CssFilter::related_options_ = NULL;

// Statistics variable names.
const char CssFilter::kBlocksRewritten[] = "css_filter_blocks_rewritten";
const char CssFilter::kParseFailures[] = "css_filter_parse_failures";
const char CssFilter::kFallbackRewrites[] = "css_filter_fallback_rewrites";
const char CssFilter::kFallbackFailures[] = "css_filter_fallback_failures";
const char CssFilter::kRewritesDropped[] = "css_filter_rewrites_dropped";
const char CssFilter::kTotalBytesSaved[] = "css_filter_total_bytes_saved";
const char CssFilter::kTotalOriginalBytes[] = "css_filter_total_original_bytes";
const char CssFilter::kUses[] = "css_filter_uses";
const char CssFilter::kCharsetMismatch[] = "flatten_imports_charset_mismatch";
const char CssFilter::kInvalidUrl[]      = "flatten_imports_invalid_url";
const char CssFilter::kLimitExceeded[]   = "flatten_imports_limit_exceeded";
const char CssFilter::kMinifyFailed[]    = "flatten_imports_minify_failed";
const char CssFilter::kRecursion[]       = "flatten_imports_recursion";
const char CssFilter::kComplexQueries[]  = "flatten_imports_complex_queries";

CssFilter::Context::Context(CssFilter* filter, RewriteDriver* driver,
                            RewriteContext* parent,
                            CacheExtender* cache_extender,
                            ImageRewriteFilter* image_rewriter,
                            ImageCombineFilter* image_combiner,
                            ResourceContext* context)
    : SingleRewriteContext(driver, parent, context),
      filter_(filter),
      css_image_rewriter_(
          new CssImageRewriter(this, filter,
                               cache_extender, image_rewriter,
                               image_combiner)),
      image_rewrite_filter_(image_rewriter),
      hierarchy_(filter),
      css_rewritten_(false),
      has_utf8_bom_(false),
      fallback_mode_(false),
      rewrite_element_(NULL),
      rewrite_inline_element_(NULL),
      rewrite_inline_char_node_(NULL),
      rewrite_inline_attribute_(NULL),
      rewrite_inline_css_kind_(kInsideStyleTag),
      in_text_size_(-1) {
  initial_css_base_gurl_.Reset(filter_->decoded_base_url());
  DCHECK(initial_css_base_gurl_.IsWebValid());
  initial_css_trim_gurl_.Reset(initial_css_base_gurl_);
}

CssFilter::Context::~Context() {
}

// The base URL used when absolutifying sub-resources must be the input
// URL of this rewrite.
//
// The only exception is the case of inline CSS, where we define the
// input URL to be a data: URL. In this case the base URL is the URL of
// the HTML page, which we save to initial_... in the constructor.
//
// When our input is the output of CssCombiner, the initial_css_base_gurl_ here
// is stale (it's the first input to the combination). It ought to be
// the URL of the output of the combination. Similarly css_trim_gurl
// needs to be set from the ultimate output resource and not just
// initial_css_trim_gurl_. This matters because for a cross-directory
// combine we can end up moving a few directories up, and further a UrlNamer
// might even end up moving some things to a separate cookieless domain.
//
// Note that we have to do it functionally and not in RewriteSingle since these
// may be invoked from Absolutify, which may be invoked from a
// different thread when doing fallback due to a deadline. This also means that
// initial_css_base_gurl_ and initial_css_trim_gurl_ must indeed just be
// initials and not be mutated.
void CssFilter::Context::GetCssBaseUrlToUse(
    const ResourcePtr& input_resource, GoogleUrl* css_base_gurl_to_use) {
  if (!IsInlineResource(input_resource)) {
    css_base_gurl_to_use->Reset(input_resource->url());
  } else {
    css_base_gurl_to_use->Reset(initial_css_base_gurl_);
  }
}

void CssFilter::Context::GetCssTrimUrlToUse(
    const ResourcePtr& input_resource,
    const StringPiece& output_url_base,
    GoogleUrl* css_trim_gurl_to_use) {
  if (!IsInlineResource(input_resource)) {
    css_trim_gurl_to_use->Reset(output_url_base);
  } else {
    css_trim_gurl_to_use->Reset(initial_css_trim_gurl_);
  }
}

void CssFilter::Context::GetCssTrimUrlToUse(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource,
    GoogleUrl* css_trim_gurl_to_use) {
  if (!IsInlineResource(input_resource)) {
    css_trim_gurl_to_use->Reset(output_resource->UrlEvenIfHashNotSet());
  } else {
    css_trim_gurl_to_use->Reset(initial_css_trim_gurl_);
  }
}

bool CssFilter::Context::SendFallbackResponse(
    StringPiece output_url_base,
    StringPiece input_contents,
    AsyncFetch* async_fetch,
    MessageHandler* handler) {
  // Do not set the content length, since we may need to mutate the
  // content as we stream out the bytes to correct for URL changes.
  async_fetch->HeadersComplete();

  DCHECK_EQ(1, num_slots());
  ResourcePtr input_resource(slot(0)->resource());
  DCHECK(input_resource.get() != NULL);

  GoogleUrl css_base_gurl_to_use;
  GetCssBaseUrlToUse(input_resource, &css_base_gurl_to_use);

  GoogleUrl css_trim_gurl_to_use;
  GetCssTrimUrlToUse(input_resource, output_url_base, &css_trim_gurl_to_use);

  bool ret = false;
  switch (Driver()->ResolveCssUrls(css_base_gurl_to_use,
                                   css_trim_gurl_to_use.Spec(),
                                   input_contents, async_fetch, handler)) {
    case RewriteDriver::kNoResolutionNeeded:
    case RewriteDriver::kWriteFailed:
      // If kNoResolutionNeeded, we just write out the input_contents, because
      // nothing needed to be changed.
      //
      // If kWriteFailed, this means that the URLs couldn't be transformed
      // (or that writer->Write() actually failed ... I think this shouldn't
      // generally happen). So, we just push out the unedited original,
      // figuring that must be better than nothing.
      //
      // TODO(sligocki): In the fetch path ResolveCssUrls should never fail
      // to transform URLs. We should just absolutify all the ones we can.
      ret = async_fetch->Write(input_contents, handler);
      break;
    case RewriteDriver::kSuccess:
      ret = true;
      break;
  }
  return ret;
}

bool CssFilter::Context::PolicyPermitsRendering() const {
  return AreOutputsAllowedByCsp(CspDirective::kStyleSrc);
}

void CssFilter::Context::Render() {
  if (num_output_partitions() == 0) {
    return;
  }

  DCHECK(has_parent() || (rewrite_element_ != NULL));

  const CachedResult& result = *output_partition(0);
  if (result.optimizable()) {
    // Note: All actual rendering is done inside ResourceSlot::Render() methods.
    if (rewrite_inline_char_node_ == NULL &&
        rewrite_inline_attribute_ == NULL) {
      // External css.
      Driver()->log_record()->SetRewriterLoggingStatus(
          id(), slot(0)->resource()->url(), RewriterApplication::APPLIED_OK);
    }
    filter_->num_uses_->Add(1);
  }

  if (Driver()->options()->Enabled(
          RewriteOptions::kExperimentCollectMobImageInfo) &&
      !has_parent() /* only report at top-level*/) {
    for (int i = 0; i < result.associated_image_info_size(); ++i) {
      image_rewrite_filter_->RegisterImageInfo(result.associated_image_info(i));
    }
  }
}

void CssFilter::Context::SetupInlineRewrite(HtmlElement* style_element,
                                            HtmlCharactersNode* text) {
  // To handle nested rewrites of inline CSS, we internally handle it
  // as a rewrite of a data: URL.
  rewrite_element_ = style_element;
  rewrite_inline_element_ = style_element;
  rewrite_inline_char_node_ = text;
  rewrite_inline_css_kind_ = kInsideStyleTag;
}

void CssFilter::Context::SetupAttributeRewrite(HtmlElement* element,
                                               HtmlElement::Attribute* src,
                                               InlineCssKind inline_css_kind) {
  DCHECK(inline_css_kind == kAttributeWithoutUrls ||
         inline_css_kind == kAttributeWithUrls);
  rewrite_element_ = element;
  rewrite_inline_element_ = element;
  rewrite_inline_attribute_ = src;
  rewrite_inline_css_kind_ = inline_css_kind;
}

void CssFilter::Context::SetupExternalRewrite(HtmlElement* element,
                                              const GoogleUrl& base_gurl,
                                              const GoogleUrl& trim_gurl) {
  rewrite_element_ = element;
  initial_css_base_gurl_.Reset(base_gurl);
  initial_css_trim_gurl_.Reset(trim_gurl);
}

void CssFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  int drop_percentage = Options()->rewrite_random_drop_percentage();
  if (drop_percentage > 0) {
    SimpleRandom* simple_random = FindServerContext()->simple_random();
    if (drop_percentage > static_cast<int>(simple_random->Next() % 100)) {
      return RewriteDone(kTooBusy, 0);
    }
  }

  bool is_ipro = IsNestedIn(RewriteOptions::kInPlaceRewriteId);
  AttachDependentRequestTrace(is_ipro ? "IproProcessCSS" : "ProcessCSS");
  input_resource_ = input_resource;
  output_resource_ = output_resource;
  StringPiece input_contents = input_resource_->ExtractUncompressedContents();
  in_text_size_ = input_contents.size();
  has_utf8_bom_ = StripUtf8Bom(&input_contents);

  GoogleUrl css_base_gurl_to_use;
  GetCssBaseUrlToUse(input_resource, &css_base_gurl_to_use);
  GoogleUrl css_trim_gurl_to_use;
  GetCssTrimUrlToUse(input_resource, output_resource_, &css_trim_gurl_to_use);
  bool parsed = RewriteCssText(
      css_base_gurl_to_use, css_trim_gurl_to_use, input_contents, in_text_size_,
      IsInlineAttribute() /* text_is_declarations */,
      Driver()->message_handler());

  if (parsed) {
    if (num_nested() > 0) {
      StartNestedTasks();
    } else {
      // We call Harvest() ourselves so we can centralize all the output there.
      Harvest();
    }
  } else {
    RewriteDone(kRewriteFailed, 0);
  }
}

// Return value answers the question: May we rewrite?
// css_base_gurl is the URL used to resolve relative URLs in the CSS.
// css_trim_gurl is the URL used to trim absolute URLs to relative URLs.
// Specifically, it should be the address of the CSS document itself for
// external CSS or the HTML document that the CSS is in for inline CSS.
bool CssFilter::Context::RewriteCssText(const GoogleUrl& css_base_gurl,
                                        const GoogleUrl& css_trim_gurl,
                                        const StringPiece& in_text,
                                        int64 in_text_size,
                                        bool text_is_declarations,
                                        MessageHandler* handler) {
  // Load stylesheet w/o expanding background attributes and preserving as
  // much content as possible from the original document.
  Css::Parser parser(in_text);
  parser.set_preservation_mode(true);
  // We avoid quirks-mode so that we do not "fix" something we shouldn't have.
  parser.set_quirks_mode(false);
  // Create a stylesheet even if given declarations so that we don't need
  // two versions of everything, though they do need to handle a stylesheet
  // with no selectors in it, which they currently do.
  scoped_ptr<Css::Stylesheet> stylesheet;
  if (text_is_declarations) {
    Css::Declarations* declarations = parser.ParseRawDeclarations();
    if (declarations != NULL) {
      stylesheet.reset(new Css::Stylesheet());
      Css::Ruleset* ruleset = new Css::Ruleset();
      stylesheet->mutable_rulesets().push_back(ruleset);
      ruleset->set_declarations(declarations);
    }
  } else {
    stylesheet.reset(parser.ParseRawStylesheet());
  }

  bool parsed = true;
  if (stylesheet.get() == NULL ||
      parser.errors_seen_mask() != Css::Parser::kNoError) {
    parsed = false;
    Driver()->message_handler()->Message(
        kWarning, "CSS parsing error in %s", css_base_gurl.spec_c_str());
    filter_->num_parse_failures_->Add(1);

    // Report all parse errors (Note: Some of these are errors we recovered
    // from by passing through unparsed sections of text).
    for (int i = 0, n = parser.errors_seen().size(); i < n; ++i) {
      Css::Parser::ErrorInfo error = parser.errors_seen()[i];
      Driver()->server_context()->usage_data_reporter()->ReportWarning(
          css_base_gurl, error.error_num, error.message);
    }

    // TODO(sligocki): Do we want to add the actual parse errors to this
    // comment? There are often a lot and they can be quite long, so I'm not
    // sure it's the best idea. Perhaps better to ask users to use the command
    // line utility? Or is it better to give them all the info in one place?
    mutable_output_partition(0)->add_debug_message(StrCat(
        "CSS rewrite failed: Parse error in ", css_base_gurl.Spec()));
  } else {
    // Edit stylesheet.
    // Any problem with an @import results in the error mask bit kImportError
    // being set, so if we get here we know that any @import rules were parsed
    // successfully, thus, flattening is safe.
    bool has_unparseables = (parser.unparseable_sections_seen_mask() !=
                             Css::Parser::kNoError);
    RewriteCssFromRoot(css_base_gurl, css_trim_gurl, in_text, in_text_size,
                       has_unparseables, stylesheet.release());
  }

  if (!parsed &&
      Driver()->options()->Enabled(RewriteOptions::kFallbackRewriteCssUrls)) {
    parsed = FallbackRewriteUrls(css_base_gurl, css_trim_gurl, in_text);
  }

  return parsed;
}

void CssFilter::Context::RewriteCssFromRoot(const GoogleUrl& css_base_gurl,
                                            const GoogleUrl& css_trim_gurl,
                                            const StringPiece& contents,
                                            int64 in_text_size,
                                            bool has_unparseables,
                                            Css::Stylesheet* stylesheet) {
  DCHECK_EQ(in_text_size_, in_text_size);

  hierarchy_.InitializeRoot(css_base_gurl, css_trim_gurl,
                            contents, has_unparseables,
                            Driver()->options()->css_flatten_max_bytes(),
                            stylesheet, Driver()->message_handler());

  css_rewritten_ = css_image_rewriter_->RewriteCss(ImageInlineMaxBytes(),
                                                   this,
                                                   &hierarchy_,
                                                   Driver()->message_handler());
}

void CssFilter::Context::RewriteCssFromNested(RewriteContext* parent,
                                              CssHierarchy* hierarchy) {
  css_image_rewriter_->RewriteCss(ImageInlineMaxBytes(), parent, hierarchy,
                                  Driver()->message_handler());
}

// Fallback to rewriting URLs using CssTagScanner because of failure to parse.
// Note: We do not flatten CSS during fallback processing.
// TODO(sligocki): Allow recursive rewriting of @imported CSS files.
bool CssFilter::Context::FallbackRewriteUrls(
    const GoogleUrl& css_base_gurl, const GoogleUrl& css_trim_gurl,
    const StringPiece& in_text) {
  fallback_mode_ = true;

  // We need permanent copies of these since fallback transformers
  // keep pointers.
  base_gurl_for_fallback_.reset(new GoogleUrl());
  base_gurl_for_fallback_->Reset(css_base_gurl);
  trim_gurl_for_fallback_.reset(new GoogleUrl());
  trim_gurl_for_fallback_->Reset(css_trim_gurl);

  bool ret = false;
  // In order to rewrite CSS using only the CssTagScanner, we run two scans.
  // Here we just record all URLs found with the CssUrlCounter.
  // The second run will be in Harvest() after all the subresources have been
  // rewritten.
  CssUrlCounter url_counter(&css_base_gurl, Driver()->message_handler());
  if (url_counter.Count(in_text)) {
    // TransformUrls will succeed only if all the URLs in the CSS file
    // were parseable. If we encounter any unparseable URLs, we will not
    // be able to absolutify them and so should not rewrite the CSS.
    ret = true;

    // Setup absolutifier used by fallback_transformer_. Only enable it if
    // we need to absolutify resources. Otherwise leave it as NULL.
    bool proxy_mode;
    RewriteDriver* driver = Driver();
    if (driver->ShouldAbsolutifyUrl(css_base_gurl, css_trim_gurl,
                                    &proxy_mode)) {
      absolutifier_.reset(new RewriteDomainTransformer(
          base_gurl_for_fallback_.get(), trim_gurl_for_fallback_.get(),
          driver->server_context(), driver->options(),
          driver->message_handler()));
      if (proxy_mode) {
        absolutifier_->set_trim_urls(false);
      }
    }
    // fallback_transformer_ will be used in the second pass (in Harvest())
    // to rewrite the URLs.
    // We instantiate it here so that all the slots below can be set to render
    // into it. When they are rendered they will set the map used by
    // AssociationTransformer.
    fallback_transformer_.reset(new AssociationTransformer(
        base_gurl_for_fallback_.get(), driver->options(), absolutifier_.get(),
        driver->message_handler()));

    const StringIntMap& url_counts = url_counter.url_counts();
    for (StringIntMap::const_iterator it = url_counts.begin();
         it != url_counts.end(); ++it) {
      const GoogleUrl url(it->first);
      // TODO(sligocki): Use count of occurrences to decide which URLs to
      // inline. it->second has the count of how many occurrences of this
      // URL there were.
      // This is guaranteed by CssUrlCounter.
      CHECK(url.IsAnyValid()) << it->first;
      // Add slot.
      bool is_authorized;
      // This can be both an image or CSS at very least, so have to be
      // conservative wrt to policy.
      ResourcePtr resource = Driver()->CreateInputResource(
          url, RewriteDriver::InputRole::kUnknown, &is_authorized);
      if (resource.get()) {
        ResourceSlotPtr slot(new AssociationSlot(
            resource, fallback_transformer_->map(), url.Spec()));
        css_image_rewriter_->RewriteSlot(slot, ImageInlineMaxBytes(), this);
      } else if (!is_authorized) {
        mutable_output_partition(0)->add_debug_message(
            StrCat("A resource was not rewritten because ", url.Host(),
                   " is not an authorized domain"));
      }
    }
  }
  return ret;
}

void CssFilter::Context::Harvest() {
  GoogleString out_text;
  bool ok = false;

  // Propagate any info on images from child rewrites.
  CssImageRewriter::InheritChildImageInfo(this);

  if (fallback_mode_) {
    // If CSS was not successfully parsed.
    if (fallback_transformer_.get() != NULL) {
      StringWriter out(&out_text);
      ok = CssTagScanner::TransformUrls(
          input_resource_->ExtractUncompressedContents(), &out,
          fallback_transformer_.get(), Driver()->message_handler());
    }
    if (ok) {
      filter_->num_fallback_rewrites_->Add(1);
    } else {
      filter_->num_fallback_failures_->Add(1);
      GoogleUrl css_base_gurl;
      GetCssBaseUrlToUse(input_resource_, &css_base_gurl);
      mutable_output_partition(0)->add_debug_message(StrCat(
          "CSS rewrite failed: Fallback transformer error in ",
          css_base_gurl.Spec()));
    }

  } else {
    // If we are limiting the size of the flattened result, work that out now;
    // simply rolling up the contents does that nicely.
    if (hierarchy_.flattening_succeeded() &&
      hierarchy_.flattened_result_limit() > 0) {
      hierarchy_.RollUpContents();
    }

    // If CSS was successfully parsed.
    hierarchy_.RollUpStylesheets();

    bool previously_optimized = false;
    for (int i = 0; !previously_optimized && i < num_nested(); ++i) {
      RewriteContext* nested_context = nested(i);
      for (int j = 0; j < nested_context->num_slots(); ++j) {
        if (nested_context->slot(j)->was_optimized()) {
          previously_optimized = true;
          break;
        }
      }
    }

    GoogleUrl css_base_gurl_to_use;
    GetCssBaseUrlToUse(input_resource_, &css_base_gurl_to_use);

    GoogleUrl css_trim_gurl_to_use;
    GetCssTrimUrlToUse(input_resource_, output_resource_,
                       &css_trim_gurl_to_use);

    // May need to absolutify @import and/or url() URLs. Note we must invoke
    // ShouldAbsolutifyUrl first because we need 'proxying' to be calculated.
    bool absolutified_urls = false;
    bool proxying = false;
    bool should_absolutify = Driver()->ShouldAbsolutifyUrl(css_base_gurl_to_use,
                                                           css_trim_gurl_to_use,
                                                           &proxying);
    if (should_absolutify) {
      absolutified_urls =
          CssAbsolutify::AbsolutifyImports(hierarchy_.mutable_stylesheet(),
                                           css_base_gurl_to_use);
    }

    // If we have determined that we need to absolutify URLs, or if we are
    // proxying (*), we need to absolutify all URLs. If we have already run the
    // CSS through the image rewriter then all parseable URLs have already been
    // done, and we only need to do unparseable URLs if any were detected.
    // (*) When proxying the root of the path can change so we need to
    // absolutify.
    if (should_absolutify || proxying) {
      absolutified_urls |= CssAbsolutify::AbsolutifyUrls(
          hierarchy_.mutable_stylesheet(),
          css_base_gurl_to_use,
          !css_rewritten_,             /* handle_parseable_ruleset_sections */
          hierarchy_.unparseable_detected(), /* handle_unparseable_sections */
          Driver(),
          Driver()->message_handler());
    }

    ok = SerializeCss(
        in_text_size_, hierarchy_.mutable_stylesheet(), css_base_gurl_to_use,
        css_trim_gurl_to_use, previously_optimized || absolutified_urls,
        IsInlineAttribute() /* stylesheet_is_declarations */, has_utf8_bom_,
        &out_text, Driver()->message_handler());
  }

  if (ok) {
    if (rewrite_inline_element_ == NULL) {
      ServerContext* server_context = FindServerContext();
      server_context->MergeNonCachingResponseHeaders(input_resource_,
                                                     output_resource_);
    } else {
      mutable_output_partition(0)->set_inlined_data(out_text);
      mutable_output_partition(0)->set_is_inline_output_resource(true);
    }
    ok = Driver()->Write(ResourceVector(1, input_resource_),
                         out_text,
                         &kContentTypeCss,
                         input_resource_->charset(),
                         output_resource_.get());
  }

  if (!hierarchy_.flattening_failure_reason().empty()) {
    mutable_output_partition(0)->add_debug_message(
        hierarchy_.flattening_failure_reason());
  }

  if (ok) {
    RewriteDone(kRewriteOk, 0);
  } else {
    RewriteDone(kRewriteFailed, 0);
  }
}

bool CssFilter::Context::SerializeCss(int64 in_text_size,
                                      const Css::Stylesheet* stylesheet,
                                      const GoogleUrl& css_base_gurl,
                                      const GoogleUrl& css_trim_gurl,
                                      bool previously_optimized,
                                      bool stylesheet_is_declarations,
                                      bool add_utf8_bom,
                                      GoogleString* out_text,
                                      MessageHandler* handler) {
  bool ret = true;

  // Re-serialize stylesheet.
  StringWriter writer(out_text);
  if (add_utf8_bom) {
    writer.Write(kUtf8Bom, handler);
  }
  if (stylesheet_is_declarations) {
    CHECK_EQ(Css::Ruleset::RULESET, stylesheet->ruleset(0).type());
    CssMinify::Declarations(stylesheet->ruleset(0).declarations(),
                            &writer, handler);
  } else {
    CssMinify::Stylesheet(*stylesheet, &writer, handler);
  }

  // Get signed versions so that we can subtract them.
  int64 out_text_size = static_cast<int64>(out_text->size());
  int64 bytes_saved = in_text_size - out_text_size;

  if (!Driver()->options()->always_rewrite_css()) {
    // Don't rewrite if we didn't edit it or make it any smaller.
    if (!previously_optimized && bytes_saved <= 0) {
      ret = false;
      if (bytes_saved != 0) {
        Driver()->InfoAt(this, "CSS parser increased size of CSS file %s by %s "
                         "bytes.", css_base_gurl.spec_c_str(),
                         Integer64ToString(-bytes_saved).c_str());
      }
      filter_->num_rewrites_dropped_->Add(1);
      mutable_output_partition(0)->add_debug_message(StrCat(
          "CSS rewrite failed: Cannot improve ", css_base_gurl.Spec()));
    }
  }

  // Statistics
  if (ret) {
    filter_->num_blocks_rewritten_->Add(1);
    filter_->total_bytes_saved_->Add(bytes_saved);
    // TODO(sligocki): Will this be misleading if we flatten @imports?
    filter_->total_original_bytes_->Add(in_text_size);
  }
  return ret;
}

int64 CssFilter::Context::ImageInlineMaxBytes() const {
    if (rewrite_inline_element_ != NULL) {
      // We're in an html context.
      return std::min(
          Driver()->options()->ImageInlineMaxBytes(),
          Driver()->options()->CssImageInlineMaxBytes());
    } else {
      // We're in a standalone CSS file.
      return Driver()->options()->CssImageInlineMaxBytes();
    }
  }

bool CssFilter::Context::Partition(OutputPartitions* partitions,
                                   OutputResourceVector* outputs) {
  if (rewrite_inline_element_ == NULL) {
    return SingleRewriteContext::Partition(partitions, outputs);
  } else {
    // We use kOmitInputHash here as this is for inline content.
    CachedResult* partition = partitions->add_partition();
    slot(0)->resource()->AddInputInfoToPartition(
        Resource::kOmitInputHash, 0, partition);
    OutputResourcePtr output_resource(
        InlineOutputResource::MakeInlineOutputResource(Driver()));
    output_resource->set_cached_result(partition);
    outputs->push_back(output_resource);
    return true;
  }
}

GoogleString CssFilter::Context::UserAgentCacheKey(
    const ResourceContext* resource_context) const {
  GoogleString key;
  if (resource_context != NULL) {
    // CSS cache-key is sensitive to whether the UA supports webp or not.
    key = ImageUrlEncoder::CacheKeyFromResourceContext(*resource_context);
  }
  // The cache key we get from the image codec is not sufficient, as
  // it does not produce different results if CSS image inlining is
  // on, but of course the css rewriter does.
  if ((Options()->CssImageInlineMaxBytes() != 0) &&
      Driver()->request_properties()->SupportsImageInlining()) {
    StrAppend(&key, "I");
  } else {
    StrAppend(&key, "A");
  }
  return key;
}

GoogleString CssFilter::Context::CacheKeySuffix() const {
  GoogleString suffix;
  if (rewrite_inline_element_ != NULL) {
    // Incorporate the base path of the HTML as part of the key --- it
    // matters for inline CSS since resources are resolved against
    // that (while it doesn't for external CSS, since that uses the
    // stylesheet as the base).
    switch (rewrite_inline_css_kind_) {
      case kInsideStyleTag: {
        const Hasher* hasher = FindServerContext()->lock_hasher();
        StrAppend(&suffix, "_@",
                  hasher->Hash(initial_css_base_gurl_.AllExceptLeaf()));
        break;
      }

      case kAttributeWithUrls: {
        // For attributes, we take a somewhat different strategy. There are
        // a lot of them, and they can be repeated in many directories,
        // so just appending the directory causes the metadata cache usage
        // to balloon. Fortunately, they are also usually very short,
        // so instead, we use the absolutified version of the data: URLs
        // as a disambiguator, so that paths that resolve URLs the same way
        // get the same keys.
        GoogleString absolutified_version;
        SimpleAbsolutifyTransformer transformer(&Driver()->decoded_base_url());
        StringWriter writer(&absolutified_version);
        CssTagScanner::TransformUrls(
            slot(0)->resource()->ExtractUncompressedContents(), &writer,
            &transformer, Driver()->message_handler());

        const Hasher* hasher = FindServerContext()->lock_hasher();
        StrAppend(&suffix, "_@", hasher->Hash(absolutified_version));
        break;
      }

      case kAttributeWithoutUrls: {
        // If there are no URLs, then there is no dependence on the
        // path, either.
        break;
      }
    }
  }

  return suffix;
}

CssFilter::CssFilter(RewriteDriver* driver,
                     CacheExtender* cache_extender,
                     ImageRewriteFilter* image_rewriter,
                     ImageCombineFilter* image_combiner)
    : RewriteFilter(driver),
      in_style_element_(false),
      style_element_(NULL),
      cache_extender_(cache_extender),
      image_rewrite_filter_(image_rewriter),
      image_combiner_(image_combiner) {
  Statistics* stats = server_context()->statistics();
  num_blocks_rewritten_ = stats->GetVariable(CssFilter::kBlocksRewritten);
  num_parse_failures_ = stats->GetVariable(CssFilter::kParseFailures);
  num_fallback_rewrites_ = stats->GetVariable(CssFilter::kFallbackRewrites);
  num_fallback_failures_ = stats->GetVariable(CssFilter::kFallbackFailures);
  num_rewrites_dropped_ = stats->GetVariable(CssFilter::kRewritesDropped);
  total_bytes_saved_ = stats->GetUpDownCounter(CssFilter::kTotalBytesSaved);
  total_original_bytes_ = stats->GetVariable(CssFilter::kTotalOriginalBytes);
  num_uses_ = stats->GetVariable(CssFilter::kUses);
  num_flatten_imports_charset_mismatch_ = stats->GetVariable(kCharsetMismatch);
  num_flatten_imports_invalid_url_ = stats->GetVariable(kInvalidUrl);
  num_flatten_imports_limit_exceeded_ = stats->GetVariable(kLimitExceeded);
  num_flatten_imports_minify_failed_ = stats->GetVariable(kMinifyFailed);
  num_flatten_imports_recursion_ = stats->GetVariable(kRecursion);
  num_flatten_imports_complex_queries_ = stats->GetVariable(kComplexQueries);
}

CssFilter::~CssFilter() {}

void CssFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(CssFilter::kBlocksRewritten);
  statistics->AddVariable(CssFilter::kParseFailures);
  statistics->AddVariable(CssFilter::kFallbackRewrites);
  statistics->AddVariable(CssFilter::kFallbackFailures);
  statistics->AddVariable(CssFilter::kRewritesDropped);
  statistics->AddUpDownCounter(CssFilter::kTotalBytesSaved);
  statistics->AddVariable(CssFilter::kTotalOriginalBytes);
  statistics->AddVariable(CssFilter::kUses);
  statistics->AddVariable(CssFilter::kCharsetMismatch);
  statistics->AddVariable(CssFilter::kInvalidUrl);
  statistics->AddVariable(CssFilter::kLimitExceeded);
  statistics->AddVariable(CssFilter::kMinifyFailed);
  statistics->AddVariable(CssFilter::kRecursion);
  statistics->AddVariable(CssFilter::kComplexQueries);
}

namespace {

// Merges arrays a & b and returns the result, allocated with new[].  Checks
// that the arrays were non-overlapping by verifying the size of the output
// array.
template<typename T>
T* MergeArrays(const T* a, int a_size, const T* b, int b_size, int* out_size) {
  *out_size = a_size + b_size;
  T* out = new T[*out_size];
  T* out_end = std::merge(a, a + a_size, b, b + b_size, out);
  CHECK_EQ(*out_size, out_end - out);
  return out;
}

}  // namespace

void CssFilter::Initialize() {
  CHECK(merged_filters_ == NULL);
#ifndef NDEBUG
  for (int i = 1; i < kRelatedFiltersSize; ++i) {
    CHECK_LT(kRelatedFilters[i - 1], kRelatedFilters[i])
        << "kRelatedFilters not in enum-value order";
  }
#endif

  merged_filters_ = MergeArrays(ImageRewriteFilter::kRelatedFilters,
                                ImageRewriteFilter::kRelatedFiltersSize,
                                kRelatedFilters, kRelatedFiltersSize,
                                &merged_filters_size_);

  CHECK(related_options_ == NULL);
  related_options_ = new StringPieceVector;
  ImageRewriteFilter::AddRelatedOptions(related_options_);
  CssFilter::AddRelatedOptions(related_options_);
  std::sort(related_options_->begin(), related_options_->end());
}

void CssFilter::Terminate() {
  CHECK(merged_filters_ != NULL);
  delete [] merged_filters_;
  merged_filters_ = NULL;
  CHECK(related_options_ != NULL);
  delete related_options_;
  related_options_ = NULL;
}

void CssFilter::AddRelatedOptions(StringPieceVector* target) {
  for (int i = 0, n = arraysize(kRelatedOptions); i < n; ++i) {
    target->push_back(kRelatedOptions[i]);
  }
}

void CssFilter::StartDocumentImpl() {
  in_style_element_ = false;
  meta_tag_charset_.clear();
}

void CssFilter::StartElementImpl(HtmlElement* element) {
  // HtmlParse should not pass us elements inside a style element.
  CHECK(!in_style_element_);
  if (element->keyword() == HtmlName::kStyle) {
    in_style_element_ = true;
    style_element_ = element;
  } else if (driver()->can_rewrite_resources()) {
    bool do_rewrite = false;
    bool check_for_url = false;
    if (driver()->options()->Enabled(RewriteOptions::kRewriteStyleAttributes)) {
      do_rewrite = true;
    } else if (driver()->options()->Enabled(
        RewriteOptions::kRewriteStyleAttributesWithUrl)) {
      check_for_url = true;
    }

    // Rewrite style attribute, if any, and iff enabled.
    if (do_rewrite || check_for_url) {
      // Per http://www.w3.org/TR/CSS21/syndata.html#uri s4.3.4 URLs and URIs:
      // "The format of a URI value is 'url(' followed by ..."
      HtmlElement::Attribute* element_style = element->FindAttribute(
          HtmlName::kStyle);
      if (element_style != NULL) {
        bool has_url =
            CssTagScanner::HasUrl(element_style->DecodedValueOrNull());
        if (!check_for_url || has_url) {
          StartAttributeRewrite(
              element, element_style,
              has_url ? kAttributeWithUrls : kAttributeWithoutUrls);
        }
      }
    }
  }
  // We deal with <link> elements in EndElement.
}

void CssFilter::Characters(HtmlCharactersNode* characters_node) {
  if (in_style_element_ && driver()->can_rewrite_resources()) {
    // Note: HtmlParse should guarantee that we only get one CharactersNode
    // per <style> block even if it is split by a flush. However, this code
    // will still mostly work if we somehow got multiple CharacterNodes.
    StartInlineRewrite(characters_node, style_element_);
  }
}

void CssFilter::EndElementImpl(HtmlElement* element) {
  // Rewrite an inline style.
  if (in_style_element_) {
    CHECK(style_element_ == element);  // HtmlParse should not pass unmatching.
    in_style_element_ = false;
  }
  if (driver()->IsRewritable(element)) {
    resource_tag_scanner::UrlCategoryVector attributes;
    resource_tag_scanner::ScanElement(
        element, driver()->options(), &attributes);
    for (resource_tag_scanner::UrlCategoryPair uc : attributes) {
      if (uc.category == semantic_type::kStylesheet) {
        StartExternalRewrite(element, uc.url);
      }
    }
  }
  if (meta_tag_charset_.empty() && element->keyword() == HtmlName::kMeta) {
    // Note any meta tag charset specifier.
    GoogleString content, mime_type, charset;
    if (ExtractMetaTagDetails(*element, NULL, &content, &mime_type, &charset)) {
      meta_tag_charset_ = charset;
    }
  }
}

void CssFilter::StartInlineRewrite(HtmlCharactersNode* char_node,
                                   HtmlElement* parent_element) {
  if (driver()->content_security_policy().HasDirectiveOrDefaultSrc(
        CspDirective::kStyleSrc)) {
    driver()->InsertDebugComment(kInlineCspMessage, parent_element);
    return;
  }

  ResourcePtr input_resource(MakeInlineResource(char_node->contents()));
  ResourceSlotPtr slot(driver()->GetInlineSlot(input_resource, char_node));

  CssFilter::Context* rewriter = StartRewriting(slot);
  if (rewriter == NULL) {
    return;
  }
  HtmlElement* element = char_node->parent();
  rewriter->SetupInlineRewrite(element, char_node);

  // Get the applicable media and charset. As style elements can't have a
  // charset attribute pass NULL to GetApplicableCharset instead of 'element'.
  // If the resulting charset for the style element doesn't agree with that of
  // the source page, we can't flatten (though that should be impossible since
  // we only look at meta elements and headers in this case).
  CssHierarchy* hierarchy = rewriter->mutable_hierarchy();
  GetApplicableMedia(element, hierarchy->mutable_media());
  GoogleString failure_reason;
  hierarchy->set_flattening_succeeded(
      GetApplicableCharset(NULL, hierarchy->mutable_charset(),
                           &failure_reason));
  if (!hierarchy->flattening_succeeded()) {
    num_flatten_imports_charset_mismatch_->Add(1);
    hierarchy->AddFlatteningFailureReason(failure_reason);
  }
}

void CssFilter::StartAttributeRewrite(HtmlElement* element,
                                      HtmlElement::Attribute* style,
                                      InlineCssKind inline_css_kind) {
  if (driver()->content_security_policy().HasDirectiveOrDefaultSrc(
        CspDirective::kStyleSrc)) {
    driver()->InsertDebugComment(kInlineCspMessage, element);
    return;
  }
  ResourcePtr input_resource(MakeInlineResource(style->DecodedValueOrNull()));
  ResourceSlotPtr slot(
      driver()->GetInlineAttributeSlot(input_resource, element, style));

  CssFilter::Context* rewriter = StartRewriting(slot);
  if (rewriter == NULL) {
    return;
  }
  rewriter->SetupAttributeRewrite(element, style, inline_css_kind);

  // @import is not allowed (nor handled) in attribute CSS, which must be
  // declarations only, so disable flattening from the get-go. Since this
  // is not a failure to flatten as such, don't update the statistics.
  // Not setting the failure reason suppresses +debug from emitting it.
  rewriter->mutable_hierarchy()->set_flattening_succeeded(false);
}

void CssFilter::StartExternalRewrite(HtmlElement* link,
                                     HtmlElement::Attribute* src) {
  if (!driver()->can_rewrite_resources()) {
    return;
  }
  // Create the input resource for the slot.
  ResourcePtr input_resource(CreateInputResourceOrInsertDebugComment(
      src->DecodedValueOrNull(), RewriteDriver::InputRole::kStyle, link));
  if (input_resource.get() == NULL) {
    return;
  }
  ResourceSlotPtr slot(driver()->GetSlot(input_resource, link, src));
  CssFilter::Context* rewriter = StartRewriting(slot);
  if (rewriter == NULL) {
    return;
  }
  GoogleUrl input_resource_gurl(input_resource->url());
  // TODO(sligocki): I don't think css_trim_gurl_ should be set to
  // decoded_base_url(). But I also think that the values passed in here
  // will always be overwritten later. This should be cleaned up.
  rewriter->SetupExternalRewrite(link, input_resource_gurl, decoded_base_url());

  // Get the applicable media and charset. If the charset on the link doesn't
  // agree with that of the source page, we can't flatten.
  CssHierarchy* hierarchy = rewriter->mutable_hierarchy();
  GetApplicableMedia(link, hierarchy->mutable_media());
  GoogleString failure_reason;
  hierarchy->set_flattening_succeeded(
      GetApplicableCharset(link, hierarchy->mutable_charset(),
                           &failure_reason));
  if (!hierarchy->flattening_succeeded()) {
    num_flatten_imports_charset_mismatch_->Add(1);
    hierarchy->AddFlatteningFailureReason(failure_reason);
  }
}

ResourcePtr CssFilter::MakeInlineResource(StringPiece content) {
  GoogleString data_url;
  // TODO(morlovich): This does a lot of useless conversions and
  // copying. Get rid of them.
  DataUrl(kContentTypeCss, PLAIN, content, &data_url);
  return DataUrlInputResource::Make(data_url, driver());
}

CssFilter::Context* CssFilter::StartRewriting(const ResourceSlotPtr& slot) {
  // Create the context add it to the slot, then kick everything off.
  DCHECK(driver()->can_rewrite_resources());
  CssFilter::Context* rewriter = MakeContext(driver(), NULL);
  rewriter->AddSlot(slot);
  if (driver()->options()->css_preserve_urls()) {
    slot->set_preserve_urls(true);
  }
  if (!driver()->InitiateRewrite(rewriter)) {
    rewriter = NULL;
  }
  return rewriter;
}

bool CssFilter::GetApplicableCharset(const HtmlElement* element,
                                     GoogleString* charset,
                                     GoogleString* failure_reason) const {
  // HTTP1.1 says the default charset is ISO-8859-1 but as the W3C says (in
  // http://www.w3.org/International/O-HTTP-charset.en.php) not many browsers
  // actually do this so a default of "" might be better. Starting from that
  // base, if the headers specify a charset that is used, otherwise if a meta
  // tag specifies a charset that is used.
  StringPiece our_charset("iso-8859-1");
  const char* our_charset_source = "the default";
  GoogleString headers_charset;
  const ResponseHeaders* headers = driver()->response_headers();
  if (headers != NULL) {
    headers_charset = headers->DetermineCharset();
    if (!headers_charset.empty()) {
      our_charset = headers_charset;
      our_charset_source = "from headers";
    }
  }
  if (headers_charset.empty() && !meta_tag_charset_.empty()) {
    our_charset = meta_tag_charset_;
    our_charset_source = "from a meta tag";
  }
  if (element != NULL) {
    const HtmlElement::Attribute* charset_attribute =
        element->FindAttribute(HtmlName::kCharset);
    if (charset_attribute != NULL) {
      const char* elements_charset = charset_attribute->DecodedValueOrNull();
      if (our_charset != elements_charset) {
        *failure_reason = StrCat(
            "The charset of the HTML (", our_charset, ", ", our_charset_source,
            ") is different from the charset attribute on the preceding "
            "element (",
            (elements_charset == NULL ? "not set" : elements_charset), ")");
        return false;  // early return!
      }
    }
  }
  our_charset.CopyToString(charset);
  return true;
}

bool CssFilter::GetApplicableMedia(const HtmlElement* element,
                                   StringVector* media) const {
  bool result = false;
  if (element != NULL) {
    const HtmlElement::Attribute* media_attribute =
        element->FindAttribute(HtmlName::kMedia);
    if (media_attribute != NULL) {
      css_util::VectorizeMediaAttribute(media_attribute->DecodedValueOrNull(),
                                        media);
      result = true;
    }
  }
  return result;
}

CssFilter::Context* CssFilter::MakeContext(RewriteDriver* driver,
                                           RewriteContext* parent) {
  ResourceContext* resource_context = new ResourceContext;
  if (parent != NULL && parent->resource_context() != NULL) {
    resource_context->CopyFrom(*(parent->resource_context()));
  } else {
    EncodeUserAgentIntoResourceContext(resource_context);
  }
  return new Context(this, driver, parent, cache_extender_,
                     image_rewrite_filter_, image_combiner_, resource_context);
}

RewriteContext* CssFilter::MakeRewriteContext() {
  return MakeContext(driver(), NULL);
}

const UrlSegmentEncoder* CssFilter::encoder() const {
  return &encoder_;
}

void CssFilter::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) const {
  // Use the same encoding as the image rewrite filter.
  image_rewrite_filter_->EncodeUserAgentIntoResourceContext(context);
}

const UrlSegmentEncoder* CssFilter::Context::encoder() const {
  return filter_->encoder();
}

RewriteContext* CssFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  RewriteContext* context = MakeContext(NULL, parent);
  context->AddSlot(slot);
  return context;
}

RewriteContext* CssFilter::MakeNestedFlatteningContextInNewSlot(
    const ResourcePtr& resource, const GoogleString& location,
    CssFilter::Context* rewriter, RewriteContext* parent,
    CssHierarchy* hierarchy) {
  // Slot represents the @import URL inside another CSS file. But rendering is
  // complicated, so we use a NullResourceSlot that has an empty Render method.
  ResourceSlotPtr slot(new NullResourceSlot(resource, location));
  RewriteContext* context = new CssFlattenImportsContext(parent, this,
                                                         rewriter, hierarchy);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
