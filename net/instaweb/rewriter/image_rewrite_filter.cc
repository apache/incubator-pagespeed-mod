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


#include "net/instaweb/rewriter/public/image_rewrite_filter.h"

#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <algorithm>
#include <cstdarg>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/logging_proto.h"
#include "net/instaweb/http/public/logging_proto_impl.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/critical_images_beacon_filter.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/css_url_encoder.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/domain_rewrite_filter.h"
#include "net/instaweb/rewriter/public/image.h"
#include "net/instaweb/rewriter/public/local_storage_cache_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/responsive_image_filter.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/single_rewrite_context.h"
#include "net/instaweb/rewriter/public/srcset_slot.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/controller/central_controller.h"
#include "pagespeed/controller/expensive_operation_callback.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/util/simple_random.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

namespace {

void DetermineQualities(const RewriteOptions& options,
                        const ResourceContext& resource_context,
                        const RequestProperties& request_properties,
                        Image::CompressionOptions* image_options) {
  if (resource_context.may_use_save_data_quality()) {
    // Use Save-Data qualities.
    image_options->webp_quality = options.ImageWebpQualityForSaveData();
    image_options->webp_animated_quality =
      options.ImageWebpQualityForSaveData();
    image_options->jpeg_quality = options.ImageJpegQualityForSaveData();
    image_options->jpeg_num_progressive_scans =
      options.image_jpeg_num_progressive_scans();
  } else if (resource_context.may_use_small_screen_quality()) {
    // Use small screen qualities.
    image_options->webp_quality = options.ImageWebpQualityForSmallScreen();
    image_options->webp_animated_quality = options.ImageWebpAnimatedQuality();
    image_options->jpeg_quality = options.ImageJpegQualityForSmallScreen();
    image_options->jpeg_num_progressive_scans =
        options.ImageJpegNumProgressiveScansForSmallScreen();
  } else {
    // Use regular (desktop) qualities.
    image_options->webp_quality = options.ImageWebpQuality();
    image_options->webp_animated_quality = options.ImageWebpAnimatedQuality();
    image_options->jpeg_quality = options.ImageJpegQuality();
    image_options->jpeg_num_progressive_scans =
        options.image_jpeg_num_progressive_scans();
  }
}

int64 GetPageWidth(const int64 page_height,
                   const int64 image_width,
                   const int64 image_height) {
  if (image_height > 0) {
    return (page_height * image_width + image_height / 2) / image_height;
  } else {
    // The client should ensure that "image_height > 0". If this condition is
    // not met, we protect against division by 0 by returning 0 so that resize
    // attempts will fail.
    return 0;
  }
}

int64 GetPageHeight(const int64 page_width,
                    const int64 image_height,
                    const int64 image_width) {
  if (image_height > 0) {
    return (page_width * image_height + image_width / 2) / image_width;
  } else {
    // The client should ensure that "image_width > 0". If this condition is
    // not met, we protect against division by 0 by returning 0 so that resize
    // attempts will fail.
    return 0;
  }
}

void SetDesiredDimensionsIfRequired(ImageDim* desired_dim,
                                    const ImageDim& image_dim) {
  if (!ImageUrlEncoder::HasValidDimension(*desired_dim)) {
    return;
  }
  int32 page_width = desired_dim->width();  // Rendered width.
  int32 page_height = desired_dim->height();  // Rendered height.
  const int64 image_width = image_dim.width();
  const int64 image_height = image_dim.height();
  if (!desired_dim->has_width()) {
    // Fill in a missing page height:
    //   page_height * (image_width / image_height),
    // rounding the result.
    // To avoid fractions we instead group as
    //   (page_height * image_width) / image_height and do the
    // math in int64 to avoid overflow in the numerator.  The additional
    // image_height / 2 causes us to round rather than truncate.
    desired_dim->set_height(page_height);
    desired_dim->set_width(static_cast<int32>(GetPageWidth(
        page_height, image_width, image_height)));
  } else if (!desired_dim->has_height()) {
    desired_dim->set_width(page_width);
    desired_dim->set_height(static_cast<int32>(GetPageHeight(
        page_width, image_height, image_width)));
  }
}

// Returns true if the low-res image can be inline-previewed.
bool ShouldInlinePreview(const int64 low_res_size, const int64 full_res_size,
                         const RewriteOptions* options) {
  bool low_res_is_small = options->max_low_res_image_size_bytes() < 0 ||
      low_res_size <= options->max_low_res_image_size_bytes();
  bool low_res_smaller_than_full_res =
      low_res_size * 100 < full_res_size *
      options->max_low_res_to_full_res_image_size_percentage();
  return (low_res_is_small && low_res_smaller_than_full_res);
}

const char* const kRelatedOptions[] = {
  RewriteOptions::kImageJpegNumProgressiveScans,
  RewriteOptions::kImageJpegNumProgressiveScansForSmallScreens,
  RewriteOptions::kImageJpegRecompressionQuality,
  RewriteOptions::kImageJpegRecompressionQualityForSmallScreens,
  RewriteOptions::kImageJpegQualityForSaveData,
  RewriteOptions::kImageLimitOptimizedPercent,
  RewriteOptions::kImageLimitResizeAreaPercent,
  RewriteOptions::kImageMaxRewritesAtOnce,
  RewriteOptions::kImagePreserveURLs,
  RewriteOptions::kImageRecompressionQuality,
  RewriteOptions::kImageResolutionLimitBytes,
  RewriteOptions::kImageWebpRecompressionQuality,
  RewriteOptions::kImageWebpRecompressionQualityForSmallScreens,
  RewriteOptions::kImageWebpAnimatedRecompressionQuality,
  RewriteOptions::kImageWebpQualityForSaveData,
  RewriteOptions::kProgressiveJpegMinBytes
};

}  // namespace

// Expose kRelatedFilters as a class variable for the benefit of
// static-init-time merging in css_filter.cc.
const RewriteOptions::Filter ImageRewriteFilter::kRelatedFilters[] = {
  RewriteOptions::kConvertGifToPng,
  RewriteOptions::kConvertJpegToProgressive,
  RewriteOptions::kConvertJpegToWebp,
  RewriteOptions::kConvertPngToJpeg,
  RewriteOptions::kConvertToWebpAnimated,
  RewriteOptions::kConvertToWebpLossless,
  RewriteOptions::kJpegSubsampling,
  RewriteOptions::kRecompressJpeg,
  RewriteOptions::kRecompressPng,
  RewriteOptions::kRecompressWebp,
  RewriteOptions::kResizeImages,
  RewriteOptions::kResizeMobileImages,
  RewriteOptions::kStripImageColorProfile,
  RewriteOptions::kStripImageMetaData
};
const int ImageRewriteFilter::kRelatedFiltersSize = arraysize(kRelatedFilters);

StringPieceVector* ImageRewriteFilter::related_options_ = NULL;

// names for Statistics variables.
const char ImageRewriteFilter::kImageRewrites[] = "image_rewrites";
const char ImageRewriteFilter::kImageNoRewritesHighResolution[] =
    "image_norewrites_high_resolution";
const char kImageRewritesDroppedIntentionally[] =
    "image_rewrites_dropped_intentionally";
const char ImageRewriteFilter::kImageRewritesDroppedDecodeFailure[] =
    "image_rewrites_dropped_decode_failure";
const char ImageRewriteFilter::kImageRewritesDroppedServerWriteFail[] =
    "image_rewrites_dropped_server_write_fail";
const char ImageRewriteFilter::kImageRewritesDroppedMIMETypeUnknown[] =
    "image_rewrites_dropped_mime_type_unknown";
const char ImageRewriteFilter::kImageRewritesDroppedNoSavingResize[] =
    "image_rewrites_dropped_nosaving_resize";
const char ImageRewriteFilter::kImageRewritesDroppedNoSavingNoResize[] =
    "image_rewrites_dropped_nosaving_noresize";
const char ImageRewriteFilter::kImageRewritesDroppedDueToLoad[] =
    "image_rewrites_dropped_due_to_load";
const char ImageRewriteFilter::kImageRewritesSquashingForMobileScreen[] =
    "image_rewrites_squashing_for_mobile_screen";
const char kImageRewriteTotalBytesSaved[] = "image_rewrite_total_bytes_saved";
const char kImageRewriteTotalOriginalBytes[] =
    "image_rewrite_total_original_bytes";
const char kImageRewriteUses[] = "image_rewrite_uses";
const char kImageInline[] = "image_inline";
const char ImageRewriteFilter::kImageOngoingRewrites[] =
    "image_ongoing_rewrites";
const char ImageRewriteFilter::kImageResizedUsingRenderedDimensions[] =
    "image_resized_using_rendered_dimensions";
const char ImageRewriteFilter::kImageWebpRewrites[] = "image_webp_rewrites";
const char ImageRewriteFilter::kInlinableImageUrlsPropertyName[] =
    "ImageRewriter-inlinable-urls";
const char ImageRewriteFilter::kImageRewriteLatencyOkMs[] =
    "image_rewrite_latency_ok_ms";
const char ImageRewriteFilter::kImageRewriteLatencyFailedMs[] =
    "image_rewrite_latency_failed_ms";
const char ImageRewriteFilter::kImageRewriteLatencyTotalMs[] =
    "image_rewrite_latency_total_ms";

const char ImageRewriteFilter::kImageWebpFromGifTimeouts[] =
    "image_webp_conversion_gif_timeouts";
const char ImageRewriteFilter::kImageWebpFromPngTimeouts[] =
    "image_webp_conversion_png_timeouts";
const char ImageRewriteFilter::kImageWebpFromJpegTimeouts[] =
    "image_webp_conversion_jpeg_timeouts";
const char ImageRewriteFilter::kImageWebpFromGifAnimatedTimeouts[] =
    "image_webp_conversion_gif_animated_timeouts";

const char ImageRewriteFilter::kImageWebpFromGifSuccessMs[] =
    "image_webp_conversion_gif_success_ms";
const char ImageRewriteFilter::kImageWebpFromPngSuccessMs[] =
    "image_webp_conversion_png_success_ms";
const char ImageRewriteFilter::kImageWebpFromJpegSuccessMs[] =
    "image_webp_conversion_jpeg_success_ms";
const char ImageRewriteFilter::kImageWebpFromGifAnimatedSuccessMs[] =
    "image_webp_conversion_gif_animated_success_ms";

const char ImageRewriteFilter::kImageWebpFromGifFailureMs[] =
    "image_webp_conversion_gif_failure_ms";
const char ImageRewriteFilter::kImageWebpFromPngFailureMs[] =
    "image_webp_conversion_png_failure_ms";
const char ImageRewriteFilter::kImageWebpFromJpegFailureMs[] =
    "image_webp_conversion_jpeg_failure_ms";
const char ImageRewriteFilter::kImageWebpFromGifAnimatedFailureMs[] =
    "image_webp_conversion_gif_animated_failure_ms";

const char ImageRewriteFilter::kImageWebpWithAlphaTimeouts[] =
    "image_webp_alpha_timeouts";
const char ImageRewriteFilter::kImageWebpWithAlphaSuccessMs[] =
    "image_webp_alpha_success_ms";
const char ImageRewriteFilter::kImageWebpWithAlphaFailureMs[] =
    "image_webp_alpha_failure_ms";

const char ImageRewriteFilter::kImageWebpOpaqueTimeouts[] =
    "image_webp_opaque_timeouts";
const char ImageRewriteFilter::kImageWebpOpaqueSuccessMs[] =
    "image_webp_opaque_success_ms";
const char ImageRewriteFilter::kImageWebpOpaqueFailureMs[] =
    "image_webp_opaque_failure_ms";

const int kNotCriticalIndex = INT_MAX;

// This is the resized placeholder image width for mobile.
const int kDelayImageWidthForMobile = 320;

namespace {

void LogImageBackgroundRewriteActivity(
    RewriteDriver* driver,
    RewriterApplication::Status status,
    const GoogleString& url,
    const char* id,
    int original_size,
    int optimized_size,
    bool is_recompressed,
    ImageType original_image_type,
    ImageType optimized_image_type,
    bool is_resized,
    int original_width,
    int original_height,
    bool is_resized_using_rendered_dimensions,
    int resized_width,
    int resized_height) {
  const RewriteOptions* options = driver->options();
  if (!options->log_background_rewrites()) {
    return;
  }

  AbstractLogRecord* log_record =
      driver->request_context()->GetBackgroundRewriteLog(
          driver->server_context()->thread_system(),
          options->allow_logging_urls_in_log_record(),
          options->log_url_indices(),
          options->max_rewrite_info_log_size());

  // Write log for background rewrites.
  log_record->LogImageBackgroundRewriteActivity(status, url, id, original_size,
      optimized_size, is_recompressed, original_image_type,
      optimized_image_type, is_resized, original_width, original_height,
      is_resized_using_rendered_dimensions, resized_width, resized_height);
}

const char* MessageForInlineResult(InlineResult inline_result) {
  const char* message = "";
  switch (inline_result) {
    case INLINE_SUCCESS:
      // No message will be displayed.
      break;
    case INLINE_UNSUPPORTED_DEVICE:
      message = "The image was not inlined because device does not support "
        "inlinling.";
      break;
    case INLINE_NOT_CRITICAL:
      message = "The image was not inlined because you have chosen to only "
        "inline the critical images but this image is not critical.";
      break;
    case INLINE_NO_DATA:
    case INLINE_TOO_LARGE:
      message = "The image was not inlined because it has too many bytes.";
      break;
    case INLINE_CACHE_SMALL_IMAGES_UNREWRITTEN:
      message = "The image was not inlined because CacheSmallImagesUnrewritten "
        "has been set.";
      break;
    case INLINE_RESPONSIVE:
      // Don't add any debug message for virtual responsive images. This virtual
      // image will be deleted before the user sees it, so message won't be
      // useful.
      break;
    case INLINE_SHORTCUT:
      message = "The image was not inlined because it is a shortcut icon.";
      break;
    case INLINE_INTERNAL_ERROR:
      message = "The image was not inlined because the internal data was "
        "corrupted.";
      break;
  }
  return message;
}

}  // namespace

class ImageRewriteFilter::Context : public SingleRewriteContext {
 public:
  enum class Place {
    kCss,
    kFetch,
    kHtmlAttr,
    kSrcset,
    kNonCssNested,
  };

  Context(int64 css_image_inline_max_bytes,
          ImageRewriteFilter* filter, RewriteDriver* driver,
          RewriteContext* parent, ResourceContext* resource_context,
          Place place, int html_index, bool in_noscript_element,
          bool is_resized_using_rendered_dimensions)
      : SingleRewriteContext(driver, parent, resource_context),
        css_image_inline_max_bytes_(css_image_inline_max_bytes),
        filter_(filter),
        place_(place),
        html_index_(html_index),
        in_noscript_element_(in_noscript_element),
        is_resized_using_rendered_dimensions_(
            is_resized_using_rendered_dimensions) {}
  virtual ~Context() {}

  bool PolicyPermitsRendering() const override;
  virtual void Render();
  virtual void RewriteSingle(const ResourcePtr& input,
                             const OutputResourcePtr& output);
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }
  virtual const UrlSegmentEncoder* encoder() const;

  // Implements UserAgentCacheKey method of RewriteContext.
  virtual GoogleString UserAgentCacheKey(
      const ResourceContext* resource_context) const;

  // Implements EncodeUserAgentIntoResourceContext of RewriteContext.
  virtual void EncodeUserAgentIntoResourceContext(
      ResourceContext* context);

  virtual void FixFetchFallbackHeaders(const CachedResult& cached_result,
                                       ResponseHeaders* headers) {
    AddLinkRelCanonicalForFallbackHeaders(headers);
    SingleRewriteContext::FixFetchFallbackHeaders(cached_result, headers);
  }

  using RewriteContext::Options;
  using RewriteContext::FindServerContext;

 private:
  class InvokeRewriteFunction;

  friend class ImageRewriteFilter;

  virtual bool ScheduleViaCentralController() { return true; }

  int64 css_image_inline_max_bytes_;
  ImageRewriteFilter* filter_;
  Place place_;
  const int html_index_;
  bool in_noscript_element_;
  bool is_resized_using_rendered_dimensions_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

class ImageRewriteFilter::Context::InvokeRewriteFunction
    : public ExpensiveOperationCallback {
 public:
  InvokeRewriteFunction(ImageRewriteFilter::Context* context,
                        ImageRewriteFilter* filter,
                        const ResourcePtr& input_resource,
                        const OutputResourcePtr& output_resource)
      : ExpensiveOperationCallback(
            context->Driver()->low_priority_rewrite_worker()),
        context_(context),
        filter_(filter),
        input_resource_(input_resource),
        output_resource_(output_resource) {}
  virtual ~InvokeRewriteFunction() { }

 protected:
  virtual void RunImpl(scoped_ptr<ExpensiveOperationContext>* context) {
    RewriteResult result = filter_->RewriteLoadedResourceImpl(
        context_, input_resource_, output_resource_);
    (*context)->Done();
    context_->RewriteDone(result, 0);
  }

  virtual void CancelImpl() {
    filter_->ReportDroppedRewrite();
    filter_->InfoAndTrace(context_, "%s: Too busy to rewrite image.",
                          input_resource_->url().c_str());
    context_->RewriteDone(kTooBusy, 0);
  }

 private:
  ImageRewriteFilter::Context* context_;
  ImageRewriteFilter* filter_;
  const ResourcePtr input_resource_;
  const OutputResourcePtr output_resource_;

  DISALLOW_COPY_AND_ASSIGN(InvokeRewriteFunction);
};

// TODO(huibao): Move the logic for determining output format to a centralized
// method which should consider all relevant factors.
void SetWebpCompressionOptions(
    const ResourceContext& resource_context,
    const RewriteOptions& options,
    const StringPiece& url,
    Image::ConversionVariables* webp_conversion_variables,
    Image::CompressionOptions* image_options) {
  switch (resource_context.libwebp_level()) {
      case ResourceContext::LIBWEBP_NONE:
        image_options->preferred_webp = pagespeed::image_compression::WEBP_NONE;
        image_options->allow_webp_alpha = false;
        VLOG(1) << "User agent is not webp capable";
        break;

      case ResourceContext::LIBWEBP_LOSSY_ONLY:
        image_options->preferred_webp =
            pagespeed::image_compression::WEBP_LOSSY;
        image_options->allow_webp_alpha = false;
        VLOG(1) << "User agent is webp lossy capable ";
        break;

      case ResourceContext::LIBWEBP_ANIMATED:
        if (options.Enabled(RewriteOptions::kConvertToWebpAnimated)) {
          image_options->preferred_webp =
              pagespeed::image_compression::WEBP_ANIMATED;
          image_options->allow_webp_animated = true;
          image_options->allow_webp_alpha = true;
          break;
        }
        VLOG(1) << "User agent is webp animated capable ";
        FALLTHROUGH_INTENDED;

      case ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA:
        image_options->allow_webp_alpha = true;
        if (options.Enabled(RewriteOptions::kConvertToWebpLossless)) {
          image_options->preferred_webp =
              pagespeed::image_compression::WEBP_LOSSLESS;
          VLOG(1) << "User agent is webp lossless+alpha capable "
                  << "and lossless images preferred";
        } else {
          image_options->preferred_webp =
              pagespeed::image_compression::WEBP_LOSSY;
          VLOG(1) << "User agent is webp lossless+alpha capable "
                  << "and lossy images preferred";
        }
        break;
      default:
        LOG(DFATAL) << "Unhandled libwebp_level";
  }
  image_options->webp_conversion_variables = webp_conversion_variables;
}

void ImageRewriteFilter::Context::RewriteSingle(
    const ResourcePtr& input_resource,
    const OutputResourcePtr& output_resource) {
  // If requested, drop random image rewrites. Eventually, frequently requested
  // images will get optimized but the long tail won't be optimized much. We're
  // not particularly concerned about the quality of the PRNG here as it's just
  // deciding if we should optimize an image or not.
  int drop_percentage = Options()->rewrite_random_drop_percentage();
  if (drop_percentage > 0 && !IsNestedIn(RewriteOptions::kCssFilterId)) {
    // Note that we don't randomly drop if this is a nested context of the CSS
    // filter as we don't want to partially rewrite a CSS file.
    SimpleRandom* simple_random = FindServerContext()->simple_random();
    if (drop_percentage > static_cast<int>(simple_random->Next() % 100)) {
      RewriteDone(kTooBusy, 0);
      return;
    }
  }
  bool is_ipro = IsNestedIn(RewriteOptions::kInPlaceRewriteId);
  AttachDependentRequestTrace(is_ipro ? "IproProcessImage" : "ProcessImage");
  AddLinkRelCanonical(input_resource, output_resource->response_headers());
  FindServerContext()->central_controller()->ScheduleExpensiveOperation(
      new InvokeRewriteFunction(this, filter_, input_resource,
                                output_resource));
}

bool ImageRewriteFilter::Context::PolicyPermitsRendering() const {
  return AreOutputsAllowedByCsp(CspDirective::kImgSrc);
}

void ImageRewriteFilter::Context::Render() {
  if (num_output_partitions() != 1) {
    // Partition failed since one of the inputs was unavailable; nothing to do.
    return;
  }

  CHECK_EQ(1, num_slots());

  const CachedResult* result = output_partition(0);
  bool rewrote_url = false;
  ResourceSlot* resource_slot = slot(0).get();
  if (place_ == Place::kCss || !has_parent()) {
    InlineResult inline_result;
    if (place_ == Place::kCss) {
      rewrote_url = filter_->FinishRewriteCssImageUrl(
          css_image_inline_max_bytes_, result, resource_slot, &inline_result);
      if (Driver()->options()->Enabled(RewriteOptions::kInlineImages)) {
        const char* message = MessageForInlineResult(inline_result);
        if (message != nullptr) {
          RewriteContext* context = parent();
          if (context != nullptr) {
            CachedResult* css_result = context->mutable_output_partition(0);
            // We want to have the CSS context retain this, so we can
            // annotate the HTML that references the CSS with the image.
            // So we write the debug message into the CSS context.
            filter_->SaveDebugMessageToCache(message, css_result);
          }
        }
      }
    } else if (place_ == Place::kHtmlAttr) {
      // We use manual rendering for HTML, as we have to consider whether to
      // inline, and may also pass in width and height attributes.
      HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
          resource_slot);
      rewrote_url = filter_->FinishRewriteImageUrl(
          result, resource_context(), html_slot->element(),
          html_slot->attribute(), html_index_, html_slot, &inline_result);

      // Register image metrics for images inside HTML here. We don't deal with
      // images inside CSS here since we might not even run --- our work may get
      // cached at CSS filter level.
      if (Driver()->options()->Enabled(
              RewriteOptions::kExperimentCollectMobImageInfo)) {
        AssociatedImageInfo aii;
        if (ExtractAssociatedImageInfo(result, this, &aii)) {
          filter_->RegisterImageInfo(aii);
        }
      }
      if (Driver()->options()->Enabled(RewriteOptions::kInlineImages)) {
        const char* message = MessageForInlineResult(inline_result);
        if (message != nullptr) {
          Driver()->InsertDebugComment(message, html_slot->element());
        }
      }
    }
    // Use standard rendering in case the rewrite is nested and not inside CSS.
  }
  if (rewrote_url) {
    // We wrote out the URL ourselves; don't let the default handling mess it up
    // (in particular replacing data: with out-of-line version)
    resource_slot->set_disable_rendering(true);
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::Context::encoder() const {
  return filter_->encoder();
}

GoogleString ImageRewriteFilter::Context::UserAgentCacheKey(
    const ResourceContext* resource_context) const {
  if (resource_context != NULL) {
    // cache-key is sensitive to whether the UA supports webp or not.
    return ImageUrlEncoder::CacheKeyFromResourceContext(*resource_context);
  }
  return "";
}

void ImageRewriteFilter::Context::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) {
  filter_->EncodeUserAgentIntoResourceContext(context);
}

ImageRewriteFilter::ImageRewriteFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      image_counter_(0),
      saw_end_document_(false) {
  Statistics* stats = server_context()->statistics();
  image_rewrites_ = stats->GetVariable(kImageRewrites);
  image_resized_using_rendered_dimensions_ =
      stats->GetVariable(kImageResizedUsingRenderedDimensions);
  image_norewrites_high_resolution_ = stats->GetVariable(
      kImageNoRewritesHighResolution);
  image_rewrites_dropped_intentionally_ =
      stats->GetVariable(kImageRewritesDroppedIntentionally);
  image_rewrites_dropped_decode_failure_ =
      stats->GetVariable(kImageRewritesDroppedDecodeFailure);
  image_rewrites_dropped_server_write_fail_ =
      stats->GetVariable(kImageRewritesDroppedServerWriteFail);
  image_rewrites_dropped_mime_type_unknown_ =
      stats->GetVariable(kImageRewritesDroppedMIMETypeUnknown);
  image_rewrites_dropped_nosaving_resize_ =
      stats->GetVariable(kImageRewritesDroppedNoSavingResize);
  image_rewrites_dropped_nosaving_noresize_ =
      stats->GetVariable(kImageRewritesDroppedNoSavingNoResize);
  image_rewrites_dropped_due_to_load_ =
      stats->GetTimedVariable(kImageRewritesDroppedDueToLoad);
  image_rewrites_squashing_for_mobile_screen_ =
      stats->GetTimedVariable(kImageRewritesSquashingForMobileScreen);
  image_rewrite_total_bytes_saved_ =
      stats->GetVariable(kImageRewriteTotalBytesSaved);
  image_rewrite_total_original_bytes_ =
      stats->GetVariable(kImageRewriteTotalOriginalBytes);
  image_rewrite_uses_ = stats->GetVariable(kImageRewriteUses);
  image_inline_count_ = stats->GetVariable(kImageInline);
  image_webp_rewrites_ = stats->GetVariable(kImageWebpRewrites);
  image_rewrite_latency_total_ms_ =
      stats->GetVariable(kImageRewriteLatencyTotalMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->timeout_count =
      stats->GetVariable(kImageWebpFromGifTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->timeout_count =
      stats->GetVariable(kImageWebpFromPngTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->timeout_count =
      stats->GetVariable(kImageWebpFromJpegTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF_ANIMATED)->timeout_count =
      stats->GetVariable(kImageWebpFromGifAnimatedTimeouts);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->success_ms =
      stats->GetHistogram(kImageWebpFromGifSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->success_ms =
      stats->GetHistogram(kImageWebpFromPngSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->success_ms =
      stats->GetHistogram(kImageWebpFromJpegSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF_ANIMATED)->success_ms =
      stats->GetHistogram(kImageWebpFromGifAnimatedSuccessMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF)->failure_ms =
      stats->GetHistogram(kImageWebpFromGifFailureMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_PNG)->failure_ms =
      stats->GetHistogram(kImageWebpFromPngFailureMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_JPEG)->failure_ms =
      stats->GetHistogram(kImageWebpFromJpegFailureMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::FROM_GIF_ANIMATED)->failure_ms =
      stats->GetHistogram(kImageWebpFromGifAnimatedFailureMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->timeout_count =
      stats->GetVariable(kImageWebpWithAlphaTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->success_ms =
      stats->GetHistogram(kImageWebpWithAlphaSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::NONOPAQUE)->failure_ms =
      stats->GetHistogram(kImageWebpWithAlphaFailureMs);

  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->timeout_count =
      stats->GetVariable(kImageWebpOpaqueTimeouts);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->success_ms =
      stats->GetHistogram(kImageWebpOpaqueSuccessMs);
  webp_conversion_variables_.Get(
      Image::ConversionVariables::OPAQUE)->failure_ms =
      stats->GetHistogram(kImageWebpOpaqueFailureMs);

  image_rewrite_latency_ok_ms_ = stats->GetHistogram(kImageRewriteLatencyOkMs);
  image_rewrite_latency_failed_ms_ =
      stats->GetHistogram(kImageRewriteLatencyFailedMs);

  image_ongoing_rewrites_ = stats->GetUpDownCounter(kImageOngoingRewrites);
}

ImageRewriteFilter::~ImageRewriteFilter() {}

void ImageRewriteFilter::InitStats(Statistics* statistics) {
#ifndef NDEBUG
  for (int i = 1; i < kRelatedFiltersSize; ++i) {
    CHECK_LT(kRelatedFilters[i - 1], kRelatedFilters[i])
        << "kRelatedFilters not in enum-value order";
  }
#endif

  statistics->AddVariable(kImageRewrites);
  statistics->AddVariable(kImageResizedUsingRenderedDimensions);
  statistics->AddVariable(kImageNoRewritesHighResolution);
  statistics->AddVariable(kImageRewritesDroppedIntentionally);
  statistics->AddVariable(kImageRewritesDroppedDecodeFailure);
  statistics->AddVariable(kImageRewritesDroppedMIMETypeUnknown);
  statistics->AddVariable(kImageRewritesDroppedServerWriteFail);
  statistics->AddVariable(kImageRewritesDroppedNoSavingResize);
  statistics->AddVariable(kImageRewritesDroppedNoSavingNoResize);
  statistics->AddTimedVariable(kImageRewritesDroppedDueToLoad,
                               Statistics::kDefaultGroup);
  statistics->AddTimedVariable(kImageRewritesSquashingForMobileScreen,
                               Statistics::kDefaultGroup);
  statistics->AddVariable(kImageRewriteTotalBytesSaved);
  statistics->AddVariable(kImageRewriteTotalOriginalBytes);
  statistics->AddVariable(kImageRewriteUses);
  statistics->AddVariable(kImageInline);
  statistics->AddVariable(kImageWebpRewrites);
  statistics->AddVariable(kImageRewriteLatencyTotalMs);
  statistics->AddUpDownCounter(kImageOngoingRewrites);
  statistics->AddHistogram(kImageRewriteLatencyOkMs);
  statistics->AddHistogram(kImageRewriteLatencyFailedMs);

  statistics->AddVariable(kImageWebpFromGifTimeouts);
  statistics->AddVariable(kImageWebpFromPngTimeouts);
  statistics->AddVariable(kImageWebpFromJpegTimeouts);
  statistics->AddVariable(kImageWebpFromGifAnimatedTimeouts);

  statistics->AddHistogram(kImageWebpFromGifSuccessMs);
  statistics->AddHistogram(kImageWebpFromPngSuccessMs);
  statistics->AddHistogram(kImageWebpFromJpegSuccessMs);
  statistics->AddHistogram(kImageWebpFromGifAnimatedSuccessMs);

  statistics->AddHistogram(kImageWebpFromGifFailureMs);
  statistics->AddHistogram(kImageWebpFromPngFailureMs);
  statistics->AddHistogram(kImageWebpFromJpegFailureMs);
  statistics->AddHistogram(kImageWebpFromGifAnimatedFailureMs);

  statistics->AddVariable(kImageWebpWithAlphaTimeouts);
  statistics->AddHistogram(kImageWebpWithAlphaSuccessMs);
  statistics->AddHistogram(kImageWebpWithAlphaFailureMs);

  statistics->AddVariable(kImageWebpOpaqueTimeouts);
  statistics->AddHistogram(kImageWebpOpaqueSuccessMs);
  statistics->AddHistogram(kImageWebpOpaqueFailureMs);
}

void ImageRewriteFilter::Initialize() {
  CHECK(related_options_ == NULL);
  related_options_ = new StringPieceVector;
  ImageRewriteFilter::AddRelatedOptions(ImageRewriteFilter::related_options_);
  std::sort(related_options_->begin(), related_options_->end());
}

void ImageRewriteFilter::Terminate() {
  CHECK(related_options_ != NULL);
  delete related_options_;
  related_options_ = NULL;
}

void ImageRewriteFilter::AddRelatedOptions(StringPieceVector* target) {
  for (int i = 0, n = arraysize(kRelatedOptions); i < n; ++i) {
    target->push_back(kRelatedOptions[i]);
  }
}

void ImageRewriteFilter::StartDocumentImpl() {
  image_counter_ = 0;
  saw_end_document_ = false;
  inlinable_urls_.clear();
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::kImageCompressionId, RewriterHtmlApplication::ACTIVE);
}

void ImageRewriteFilter::EndDocument() {
  saw_end_document_  = true;
}

void ImageRewriteFilter::RenderDone() {
  // Only care about the very end, not every flush window; framework orders
  // EndDocument before the last RenderDone (and after previous ones) so we
  // use EndDocument() having been called to distinguish the last flush window
  // from previous ones.
  if (!saw_end_document_) {
    return;
  }
  if (!image_info_.empty()) {
    GoogleString code =
        "psMobStaticImageInfo = {";
    for (AssociatedImageInfoMap::iterator i = image_info_.begin(),
                                          e = image_info_.end();
         i != e; ++i) {
      const AssociatedImageInfo& image_info = i->second;
      EscapeToJsStringLiteral(image_info.url(), true /* want quotes */,
                              &code);
      StrAppend(&code, ":{");
      StrAppend(&code, "w:",
                IntegerToString(image_info.dimensions().width()), ",");
      StrAppend(&code, "h:",
                IntegerToString(image_info.dimensions().height()), "},");
    }
    StrAppend(&code, "}");
    HtmlElement* script = driver()->NewElement(NULL, HtmlName::kScript);
    HtmlCharactersNode* chars = driver()->NewCharactersNode(script, code);
    InsertNodeAtBodyEnd(script);
    driver()->AppendChild(script, chars);
  }
  image_info_.clear();
}

// Allocate and initialize CompressionOptions object based on RewriteOptions and
// ResourceContext.
Image::CompressionOptions* ImageRewriteFilter::ImageOptionsForLoadedResource(
    const ResourceContext& resource_context,
    const ResourcePtr& input_resource) {
  Image::CompressionOptions* image_options = new Image::CompressionOptions();
  int64 input_size =
      static_cast<int64>(input_resource->UncompressedContentsSize());
  // Disable webp conversion for images in CSS if the original image size is
  // greater than max_image_bytes_in_css_for_webp. This is because webp does not
  // support progressive which causes a perceptible delay in the loading of
  // large background images.
  const RewriteOptions* options = driver()->options();
  if (resource_context.libwebp_level() != ResourceContext::LIBWEBP_NONE) {
    SetWebpCompressionOptions(resource_context, *options, input_resource->url(),
                              &webp_conversion_variables_, image_options);
  }

  DetermineQualities(*options, resource_context,
                     *driver()->request_properties(), image_options);

  image_options->progressive_jpeg =
      options->Enabled(RewriteOptions::kConvertJpegToProgressive) &&
      input_size >= options->progressive_jpeg_min_bytes();
  image_options->progressive_jpeg_min_bytes =
      options->progressive_jpeg_min_bytes();
  image_options->convert_png_to_jpeg =
      options->Enabled(RewriteOptions::kConvertPngToJpeg);
  image_options->convert_gif_to_png =
      options->Enabled(RewriteOptions::kConvertGifToPng);
  image_options->convert_jpeg_to_webp =
      options->Enabled(RewriteOptions::kConvertJpegToWebp);
  image_options->recompress_jpeg =
      options->Enabled(RewriteOptions::kRecompressJpeg);
  image_options->recompress_png =
      options->Enabled(RewriteOptions::kRecompressPng);
  image_options->recompress_webp =
      options->Enabled(RewriteOptions::kRecompressWebp);
  image_options->retain_color_profile =
      !options->Enabled(RewriteOptions::kStripImageColorProfile);
  image_options->retain_exif_data =
      !options->Enabled(RewriteOptions::kStripImageMetaData);
  image_options->retain_color_sampling =
      !options->Enabled(RewriteOptions::kJpegSubsampling);
  image_options->webp_conversion_timeout_ms =
      options->image_webp_timeout_ms();

  return image_options;
}

// Resize image if necessary, returning true if this resizing succeeds and false
// if it's unnecessary or fails.
bool ImageRewriteFilter::ResizeImageIfNecessary(
    const Context* rewrite_context, const GoogleString& url,
    ResourceContext* resource_context, Image* image, CachedResult* cached) {
  bool resized = false;
  // Begin by resizing the image if necessary
  ImageDim image_dim;
  image->Dimensions(&image_dim);

  if (image_dim.width() <= 0 || image_dim.height() <= 0) {
    cached->add_debug_message(
        StringPrintf("Cannot resize %s: Image must be at least 1x1",
                     UrlForDebugMessages(rewrite_context).c_str()));
    return false;
  }

  // Here we are computing the size of the image as described by the html on the
  // page or as desired by mobile screen resolutions. If we succeed in doing so,
  // that will be the desired image size. Otherwise we may fill in
  // desired_image_dims later based on actual image size.
  ImageDim* desired_dim = resource_context->mutable_desired_image_dims();
  const ImageDim* post_resize_dim = &image_dim;
  if (ShouldResize(*resource_context, url, image, desired_dim)) {
    DCHECK_LT(0, desired_dim->width());
    DCHECK_LT(0, desired_dim->height());

    const char* message;  // Informational message for logging only.
    if (image->ResizeTo(*desired_dim)) {
      post_resize_dim = desired_dim;
      message = "Resized";
      resized = true;
    } else {
      message = "Couldn't resize";
    }

    driver()->InfoAt(rewrite_context, "%s image `%s' from %dx%d to %dx%d",
                     message, url.c_str(),
                     image_dim.width(), image_dim.height(),
                     desired_dim->width(), desired_dim->height());
    cached->add_debug_message(image->resize_debug_message());
  } else {
    cached->add_debug_message(
        StringPrintf("Image %s does not appear to need resizing.",
                     UrlForDebugMessages(rewrite_context).c_str()));
  }

  // Cache image dimensions, including any resizing we did.
  // This happens regardless of whether we rewrite the image contents.
  if (ImageUrlEncoder::HasValidDimensions(*post_resize_dim)) {
    ImageDim* dims = cached->mutable_image_file_dims();
    dims->set_width(post_resize_dim->width());
    dims->set_height(post_resize_dim->height());
  }
  return resized;
}

// Determines whether an image should be resized based on the current options.
//
// Returns the dimensions to resize to in *desired_dimensions.
bool ImageRewriteFilter::ShouldResize(const ResourceContext& resource_context,
                                      const GoogleString& url,
                                      Image* image,
                                      ImageDim* desired_dim) {
  const RewriteOptions* options = driver()->options();
  if (!options->Enabled(RewriteOptions::kResizeImages) &&
      !options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions)) {
    return false;
  }

  if (image->content_type()->type() != ContentType::kGif ||
      options->Enabled(RewriteOptions::kConvertGifToPng) ||
      options->Enabled(RewriteOptions::kDelayImages)) {
    *desired_dim = resource_context.desired_image_dims();
    ImageDim image_dim;
    image->Dimensions(&image_dim);
    if (options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions)) {
      // Respect the aspect ratio of the image when doing the resize.
      SetDesiredDimensionsIfRequired(desired_dim, image_dim);
    } else {
      UpdateDesiredImageDimsIfNecessary(
          image_dim, resource_context, desired_dim);
      if (options->Enabled(RewriteOptions::kResizeImages) &&
          ImageUrlEncoder::HasValidDimension(*desired_dim) &&
          ImageUrlEncoder::HasValidDimensions(image_dim)) {
        SetDesiredDimensionsIfRequired(desired_dim, image_dim);
      }
    }
    if (ImageUrlEncoder::HasValidDimension(*desired_dim) &&
        ImageUrlEncoder::HasValidDimensions(image_dim)) {
      const int64 page_area =
          static_cast<int64>(desired_dim->width()) *
          desired_dim->height();
      const int64 image_area =
          static_cast<int64>(image_dim.width()) * image_dim.height();
      if (page_area * 100 <
          image_area * options->image_limit_resize_area_percent()) {
        DCHECK_LT(0, desired_dim->width());
        DCHECK_LT(0, desired_dim->height());
        return true;
      }
    }
  }
  return false;
}

namespace {

int64 GetCurrentCpuTimeMs(Timer* timer) {
  // See http://linux.die.net/man/2/getrusage -- RUSAGE_THREAD is supported
  // on Linux since Linux 2.6.26, so fall back to wall-clock time.
#ifdef RUSAGE_THREAD
  struct rusage start_rusage;
  if (getrusage(RUSAGE_THREAD, &start_rusage) == 0) {
    return ((start_rusage.ru_utime.tv_sec * 1000) +
            (start_rusage.ru_utime.tv_usec / 1000));
  }
#endif
  return timer->NowMs();
}

}  // namespace

// Format as InfoAt and using TracePrintf.
// TODO(jmaessen): Avoid formatting if neither applies.
void ImageRewriteFilter::InfoAndTrace(
    Context* rewrite_context, const char* format, ...) {
  va_list args;
  va_start(args, format);
  GoogleString message;
  StringAppendV(&message, format, args);
  driver()->InfoAt(rewrite_context, "%s", message.c_str());
  driver()->TraceString(message);
  va_end(args);
}

RewriteResult ImageRewriteFilter::RewriteLoadedResourceImpl(
      Context* rewrite_context, const ResourcePtr& input_resource,
      const OutputResourcePtr& result) {
  rewrite_context->TracePrintf("Image rewrite: %s",
                               input_resource->url().c_str());
  MessageHandler* message_handler = driver()->message_handler();
  StringVector urls;
  ResourceContext resource_context;
  const RewriteOptions* options = driver()->options();

  resource_context = *rewrite_context->resource_context();

  if (!encoder_.Decode(result->name(),
                       &urls, &resource_context, message_handler)) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_rewrites_dropped_decode_failure_->Add(1);
    return kRewriteFailed;
  }

  Image::CompressionOptions* image_options =
      ImageOptionsForLoadedResource(resource_context, input_resource);
  scoped_ptr<Image> image(
      NewImage(input_resource->ExtractUncompressedContents(),
               input_resource->url(), server_context()->filename_prefix(),
               image_options, driver()->timer(), message_handler));

  // Initialize logging data.
  ImageType original_image_type = image->image_type();
  ImageType optimized_image_type = original_image_type;
  int original_size = image->input_size();
  int optimized_size = original_size;
  bool is_recompressed = false;
  bool is_resized = false;
  image->SetDebugMessageUrl(UrlForDebugMessages(rewrite_context));

  if (original_image_type == IMAGE_UNKNOWN) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_rewrites_dropped_mime_type_unknown_->Add(1);
    driver()->InfoAt(
        rewrite_context, "%s: Image MIME type could not be "
        "discovered from reading magic bytes; rewriting dropped.",
        input_resource->url().c_str());
    return kRewriteFailed;
  }
  // We used to reject beacon images based on their size (1x1 or less) here, but
  // now rely on caching headers instead as this was missing a lot of padding
  // images that were ripe for inlining.
  RewriteResult rewrite_result = kTooBusy;

  ImageDim image_dim;
  image->Dimensions(&image_dim);
  int64 image_width = image_dim.width(), image_height = image_dim.height();
  if ((image_width*image_height*4) > options->image_resolution_limit_bytes()) {
    image_rewrites_dropped_intentionally_->Add(1);
    image_norewrites_high_resolution_->Add(1);
    return kRewriteFailed;
  }

  image_ongoing_rewrites_->Add(1);

  rewrite_result = kRewriteFailed;
  Timer* timer = server_context()->timer();
  int64 rewrite_time_start_ms = GetCurrentCpuTimeMs(timer);
  CachedResult* cached = result->EnsureCachedResultCreated();
  is_resized = ResizeImageIfNecessary(
      rewrite_context, input_resource->url(),
      &resource_context, image.get(), cached);

  // When the "resize_images" filter has been turned on and the IMG tag has
  // width and/or height specified, we assume that the image will be resized so
  // the new dimension will be embedded into the rewritten image URL. However,
  // if reizing turns out to be a failure, we don't want the new dimension in
  // the rewritten URL. For the latter case, we will reset the "name" of the
  // output resource.
  if (!is_resized) {
    resource_context.clear_desired_image_dims();
    GoogleString name;
    GoogleUrl mapped_gurl;  // Not used
    GoogleString failure_reason;  // Not used
    if (driver()->GenerateOutputResourceNameAndUrl(
            encoder(), &resource_context, input_resource, &name, &mapped_gurl,
            &failure_reason)) {
      result->mutable_full_name()->set_name(name);
    } else {
      LOG(DFATAL) << "Failed to generate name and URL for the output resource.";
      return kRewriteFailed;
    }
  }

  // Now re-compress the (possibly resized) image, and decide if it's
  // saved us anything.
  if (is_resized || options->ImageOptimizationEnabled()) {
    // Call output_size() before image_type(). When output_size() is called,
    // the image will be recompressed and the image type may be changed
    // in order to get the smallest output.
    // TODO(huibao): rename output_size() to RecompressAndReturnOutputSize()
    optimized_size = image->output_size();
    optimized_image_type = image->image_type();
    is_recompressed = true;

    // The image has been recompressed (and potentially resized). However,
    // the recompressed image may not be used unless the file size is reduced.
    if (image->output_size() * 100 <
        image->input_size() * options->image_limit_optimized_percent()) {
      // Here output image type could potentially be different from input
      // type.
      const ContentType* output_type =
          ImageToContentType(input_resource->url(), image.get());

      // Consider inlining output image (no need to check input, it's bigger)
      // This needs to happen before Write to persist.
      SaveIfInlinable(image->Contents(), image->image_type(), cached);

      server_context()->MergeNonCachingResponseHeaders(
          input_resource, result);
      if (options->no_transform_optimized_images()) {
        result->set_cache_control_suffix(",no-transform");
      }
      if (driver()->Write(
              ResourceVector(1, input_resource), image->Contents(),
              output_type, StringPiece() /* no charset for images */,
              result.get())) {
        // Static cast to avoid format string trouble.
        driver()->InfoAt(
            rewrite_context,
            "Shrinking image `%s' (%u bytes) to `%s' (%u bytes)",
            input_resource->url().c_str(),
            static_cast<unsigned>(image->input_size()),
            result->url().c_str(),
            static_cast<unsigned>(image->output_size()));

        // Update stats.
        image_rewrites_->Add(1);
        image_rewrite_total_bytes_saved_->Add(
            image->input_size() - image->output_size());
        image_rewrite_total_original_bytes_->Add(image->input_size());
        if (result->type()->type() == ContentType::kWebp) {
          image_webp_rewrites_->Add(1);
        }

        rewrite_result = kRewriteOk;
      } else {
        // Server fails to write merged files.
        image_rewrites_dropped_server_write_fail_->Add(1);
        InfoAndTrace(
            rewrite_context,
            "Server fails writing image content for `%s'; rewriting dropped.",
            input_resource->url().c_str());
      }
    } else if (is_resized) {
      // Eliminate any image dimensions from a resize operation that
      // succeeded, but yielded overly-large output.
      image_rewrites_dropped_nosaving_resize_->Add(1);
      InfoAndTrace(
          rewrite_context,
          "Shrink of image `%s' (%u -> %u bytes) doesn't save space; "
          "dropped.",
          input_resource->url().c_str(),
          static_cast<unsigned>(image->input_size()),
          static_cast<unsigned>(image->output_size()));
      ImageDim* dims = cached->mutable_image_file_dims();
      dims->clear_width();
      dims->clear_height();
    } else if (options->ImageOptimizationEnabled()) {
      // Fails due to overly-large output without resize.
      image_rewrites_dropped_nosaving_noresize_->Add(1);
      InfoAndTrace(
          rewrite_context,
          "Recompressing image `%s' (%u -> %u bytes) doesn't save space; "
          "dropped.",
          input_resource->url().c_str(),
          static_cast<unsigned>(image->input_size()),
          static_cast<unsigned>(image->output_size()));
    }
  }

  cached->set_optimized_image_type(optimized_image_type);
  cached->set_size(rewrite_result == kRewriteOk ? image->output_size() :
                   image->input_size());
  SaveDebugMessageToCache(image->debug_message(), cached);

  // Try inlining input image if output hasn't been inlined already.
  if (!cached->has_inlined_data()) {
    SaveIfInlinable(input_resource->ExtractUncompressedContents(),
                    original_image_type, cached);
  }

  int64 image_size = static_cast<int64>(image->output_size());
  if (options->Enabled(RewriteOptions::kDelayImages) &&
      !rewrite_context->in_noscript_element_ &&
      !cached->has_low_resolution_inlined_data() &&
      image_size >= options->min_image_size_low_resolution_bytes() &&
      image_size <= options->max_image_size_low_resolution_bytes()) {
    Image::CompressionOptions* image_options =
        new Image::CompressionOptions();
    SetWebpCompressionOptions(resource_context, *options,
                              input_resource->url(),
                              &webp_conversion_variables_,
                              image_options);

    image_options->jpeg_quality = options->ImageJpegQuality();
    image_options->webp_quality = options->ImageWebpQuality();
    image_options->webp_animated_quality = options->ImageWebpAnimatedQuality();
    image_options->progressive_jpeg = false;
    image_options->convert_png_to_jpeg =
        options->Enabled(RewriteOptions::kConvertPngToJpeg);

    // Set to true since we optimize a gif to png before resize.
    image_options->convert_gif_to_png = true;
    image_options->recompress_jpeg = true;
    image_options->recompress_png = true;
    image_options->recompress_webp = true;

    // Since these are replaced with their high res versions, stripping
    // them off for low res images will further reduce bytes.
    image_options->retain_color_profile = false;
    image_options->retain_exif_data = false;
    image_options->retain_color_sampling = false;
    image_options->jpeg_num_progressive_scans =
        options->image_jpeg_num_progressive_scans();

    scoped_ptr<Image> low_image;
    if (driver()->options()->use_blank_image_for_inline_preview()) {
      image_options->use_transparent_for_blank_image = true;
      low_image.reset(BlankImageWithOptions(image_width, image_height,
          IMAGE_PNG, server_context()->filename_prefix(),
          timer, message_handler, image_options));
      low_image->EnsureLoaded(true);
    } else {
      low_image.reset(NewImage(image->Contents(), input_resource->url(),
          server_context()->filename_prefix(), image_options,
          timer, message_handler));
      low_image->SetDebugMessageUrl(UrlForDebugMessages(rewrite_context));
    }
    low_image->SetTransformToLowRes();
    if (ShouldInlinePreview(low_image->Contents().size(),
                            image->Contents().size(), options)) {
      if (resource_context.mobile_user_agent()) {
        ResizeLowQualityImage(low_image.get(), input_resource, cached);
      } else {
        cached->set_low_resolution_inlined_data(low_image->Contents().data(),
                                                low_image->Contents().size());
      }
      cached->set_low_resolution_inlined_image_type(
          static_cast<int>(low_image->image_type()));
    }
  }
  image_ongoing_rewrites_->Add(-1);

  int64 latency_ms = GetCurrentCpuTimeMs(timer) - rewrite_time_start_ms;
  if (rewrite_result == kRewriteOk) {
    image_rewrite_latency_ok_ms_->Add(latency_ms);
  } else {
    image_rewrite_latency_failed_ms_->Add(latency_ms);
  }

  // We track the total latency (including failed & OK) in its own
  // variable so it can be easily scraped with wget.  The ok/failed
  // versions above are histograms and thus harder to scrape.
  image_rewrite_latency_total_ms_->Add(latency_ms);

  // All other conditions were updated in other code paths above.
  if (rewrite_result == kRewriteFailed) {
    image_rewrites_dropped_intentionally_->Add(1);
  } else if (rewrite_result == kRewriteOk) {
    rewrite_context->TracePrintf("Image rewrite success (%u -> %u)",
                                 static_cast<unsigned>(image->input_size()),
                                 static_cast<unsigned>(image->output_size()));
  }

  const ImageDim& post_resize_dim =
      resource_context.desired_image_dims();
  LogImageBackgroundRewriteActivity(driver(),
      rewrite_result == kRewriteOk ?
          RewriterApplication::APPLIED_OK : RewriterApplication::NOT_APPLIED,
      input_resource->url(), LoggingId(), original_size, optimized_size,
      is_recompressed, original_image_type, optimized_image_type, is_resized,
      image_width, image_height,
      rewrite_context->is_resized_using_rendered_dimensions_,
      post_resize_dim.width(), post_resize_dim.height());

  return rewrite_result;
}

// Generate resized low quality image if the image width is not smaller than
// kDelayImageWidthForMobile. If image width is smaller than
// kDelayImageWidthForMobile, "delay_images" optimization is not very useful
// and no low quality image will be generated.
void ImageRewriteFilter::ResizeLowQualityImage(
    Image* low_image, const ResourcePtr& input_resource, CachedResult* cached) {
  ImageDim image_dim;
  low_image->Dimensions(&image_dim);
  if (image_dim.width() >= kDelayImageWidthForMobile) {
    const RewriteOptions* options = driver()->options();
    Image::CompressionOptions* image_options =
        new Image::CompressionOptions();
    image_options->jpeg_quality = options->ImageJpegQuality();
    image_options->webp_quality = options->ImageWebpQuality();
    image_options->webp_animated_quality = options->ImageWebpAnimatedQuality();
    image_options->progressive_jpeg = false;
    image_options->convert_png_to_jpeg =
        options->Enabled(RewriteOptions::kConvertPngToJpeg);
    image_options->convert_gif_to_png =
        options->Enabled(RewriteOptions::kConvertGifToPng);
    image_options->recompress_jpeg =
        options->Enabled(RewriteOptions::kRecompressJpeg);
    image_options->recompress_png =
        options->Enabled(RewriteOptions::kRecompressPng);
    image_options->recompress_webp =
        options->Enabled(RewriteOptions::kRecompressWebp);
    scoped_ptr<Image> image(
        NewImage(low_image->Contents(), input_resource->url(),
                 server_context()->filename_prefix(), image_options,
                 driver()->timer(), driver()->message_handler()));
    image->SetTransformToLowRes();
    ImageDim resized_dim;
    resized_dim.set_width(kDelayImageWidthForMobile);
    resized_dim.set_height((static_cast<int64>(resized_dim.width()) *
                            image_dim.height()) / image_dim.width());
    MessageHandler* message_handler = driver()->message_handler();
    bool resized = image->ResizeTo(resized_dim);
    StringPiece contents = image->Contents();
    StringPiece old_contents = low_image->Contents();
    if (resized && contents.size() < old_contents.size()) {
      cached->set_low_resolution_inlined_data(contents.data(), contents.size());
      message_handler->Message(
          kInfo,
          "Resized low quality image (%s) from "
          "%dx%d(%d bytes) to %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.width(),
          static_cast<int>(contents.size()));
    } else {
      message_handler->Message(
          kInfo,
          "Couldn't resize low quality image (%s) or resized image file is "
          "not smaller: "
          "%dx%d(%d bytes) => %dx%d(%d bytes)",
          input_resource->url().c_str(),
          image_dim.width(), image_dim.height(),
          static_cast<int>(old_contents.size()),
          resized_dim.width(), resized_dim.height(),
          static_cast<int>(contents.size()));
    }
  }
}

void ImageRewriteFilter::SaveIfInlinable(const StringPiece& contents,
                                         const ImageType image_type,
                                         CachedResult* cached) {
  // We retain inlining information if the image size is < the largest possible
  // inlining threshold, as an image might be used in both html and css and we
  // may see it first from the one with a smaller threshold. Note that this can
  // cause us to save inline information for an image that won't ever actually
  // be inlined (because it's too big to inline in html, say, and doesn't occur
  // in css).
  int64 image_inline_max_bytes =
      driver()->options()->MaxImageInlineMaxBytes();
  if (static_cast<int64>(contents.size()) < image_inline_max_bytes) {
    cached->set_inlined_data(contents.data(), contents.size());
    cached->set_inlined_image_type(static_cast<int>(image_type));
  }
}

// Convert (possibly NULL) Image* to corresponding (possibly NULL) ContentType*
const ContentType* ImageRewriteFilter::ImageToContentType(
    const GoogleString& origin_url, Image* image) {
  const ContentType* content_type = NULL;
  if (image != NULL) {
    // Even if we know the content type from the extension coming
    // in, the content-type can change as a result of compression,
    // e.g. gif to png, or jpeg to webp.
    return image->content_type();
  }
  return content_type;
}

void ImageRewriteFilter::ComputePreserveUrls(
    const RewriteOptions* options, ResourceSlot* slot) {
  // Note that in RewriteOptions::Merge we turn off image_preserve_urls
  // when merging into a configuration that has explicitly
  // enabled cache_extend_images.
  //
  // Consider a hosting provider that turns on "optimize for
  // bandwidth" mode, and then a site enables resize_images
  // explicitly.  That should override the image-url-preservation
  // default that was set at root.  Note that explicitly turning on
  // RecompressImages doesn't mean we'll want to override
  // image_preserve_urls rewrite URLs here, since we can still get
  // the benefit of recompression via IPRO.  But we make an
  // exception for inlining and image-resizing directives since
  // those can only be done via url-rewriting.
  if (options->image_preserve_urls() &&
      !options->Enabled(RewriteOptions::kResizeImages) &&
      !options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions) &&
      !options->Enabled(RewriteOptions::kInlineImages)) {
    slot->set_preserve_urls(true);
  }
}

void ImageRewriteFilter::BeginRewriteImageUrl(HtmlElement* element,
                                              HtmlElement::Attribute* src) {
  scoped_ptr<ResourceContext> resource_context(new ResourceContext);
  const RewriteOptions* options = driver()->options();
  bool is_resized_using_rendered_dimensions = false;

  // In case of RewriteOptions::image_preserve_urls() we do not want to use
  // image dimension information from HTML/CSS.

  if (options->Enabled(RewriteOptions::kResizeImages) ||
      options->Enabled(RewriteOptions::kResizeToRenderedImageDimensions)) {
    ImageDim* desired_dim = resource_context->mutable_desired_image_dims();
    GetDimensions(element, desired_dim, src,
                  &is_resized_using_rendered_dimensions);
    if ((desired_dim->width() == 0 || desired_dim->height() == 0 ||
         (desired_dim->width() == 1 && desired_dim->height() == 1))) {
      // This is either a beacon image, or an attempt to prefetch.  Drop the
      // desired dimensions so that the image is not resized.
      resource_context->clear_desired_image_dims();
    }
  }
  StringPiece url(src->DecodedValueOrNull());

  EncodeUserAgentIntoResourceContext(resource_context.get());

  ResourcePtr input_resource(CreateInputResourceOrInsertDebugComment(
      src->DecodedValueOrNull(), RewriteDriver::InputRole::kImg, element));
  if (input_resource.get() == NULL) {
    return;
  }

  // If the image will be inlined and the local storage cache is enabled, add
  // the LSC marker attribute to this element so that the LSC filter knows to
  // insert the relevant javascript functions.
  if (driver()->request_properties()->SupportsImageInlining()) {
    LocalStorageCacheFilter::InlineState state;
    LocalStorageCacheFilter::AddStorableResource(src->DecodedValueOrNull(),
                                                 driver(),
                                                 true /* ignore cookie */,
                                                 element, &state);
  }
  Context* context = new Context(0 /* No CSS inlining, it's html */,
                                 this, driver(), NULL /*not nested */,
                                 resource_context.release(),
                                 Context::Place::kHtmlAttr, image_counter_++,
                                 noscript_element() != NULL,
                                 is_resized_using_rendered_dimensions);
  ResourceSlotPtr slot(driver()->GetSlot(input_resource, element, src));
  context->AddSlot(slot);

  ComputePreserveUrls(options, slot.get());
  driver()->InitiateRewrite(context);
}

void ImageRewriteFilter::BeginRewriteSrcSet(HtmlElement* element,
                                            HtmlElement::Attribute* srcset) {
  RefCountedPtr<SrcSetSlotCollection> slot_collection(
      driver()->GetSrcSetSlotCollection(this, element, srcset));

  for (int i = 0; i < slot_collection->num_image_candidates(); ++i) {
    SrcSetSlot* slot = slot_collection->slot(i);
    if (slot == nullptr) {
      continue;
    }

    scoped_ptr<ResourceContext> resource_context(new ResourceContext);
    EncodeUserAgentIntoResourceContext(resource_context.get());

    Context* context = new Context(0 /* No CSS inlining, it's html */,
                                   this, driver(), nullptr /*not nested */,
                                   resource_context.release(),
                                   Context::Place::kSrcset, image_counter_++,
                                   noscript_element() != nullptr,
                                   false /*not resizing with rendered dim */);
    context->AddSlot(RefCountedPtr<ResourceSlot>(slot));
    ComputePreserveUrls(driver()->options(), slot);
    driver()->InitiateRewrite(context);
  }
}

bool ImageRewriteFilter::FinishRewriteCssImageUrl(
    int64 css_image_inline_max_bytes, const CachedResult* cached,
    ResourceSlot* slot, InlineResult* inline_result) {
  GoogleString data_url;
  *inline_result = TryInline(false /*not html*/, false /*not critical*/,
                             css_image_inline_max_bytes, cached, slot,
                             &data_url);

  if (*inline_result == INLINE_SUCCESS) {
    // TODO(jmaessen): Can we make output URL reflect actual *usage*
    // of image inlining and/or webp images?
    const RewriteOptions* options = driver()->options();
    DCHECK(!options->cache_small_images_unrewritten())
        << "Modifying a URL slot despite "
        << "image_inlining_identify_and_cache_without_rewriting set.";
    if (slot->DirectSetUrl(data_url)) {
      image_inline_count_->Add(1);
      return true;
    }
  } else if (cached->optimizable()) {
    image_rewrite_uses_->Add(1);
  }
  // Fall back to nested rewriting, which will also left trim the url if that
  // is required.
  return false;
}

namespace {

// Skip ascii whitespace, returning pointer to first non-whitespace character in
// accordance with:
//   http://www.whatwg.org/specs/web-apps/current-work/multipage/
//                  common-microsyntaxes.html#space-character
const char* SkipAsciiWhitespace(const char* position) {
  while (*position <= ' ' &&  // Quickly skip if no leading whitespace
         (*position == ' ' || *position == '\x09' || *position == '\x0A' ||
          *position == '\x0C' || *position == '\x0D')) {
    ++position;
  }
  return position;
}

bool GetDimensionAttribute(
    const HtmlElement* element, HtmlName::Keyword name, int* value) {
  const HtmlElement::Attribute* attribute = element->FindAttribute(name);
  if (attribute == NULL) {
    return false;
  }
  const char* position = attribute->DecodedValueOrNull();
  return ImageRewriteFilter::ParseDimensionAttribute(position, value);
}

// If the element has a width attribute, set it in page_dim.
void SetWidthFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 width;
  if (GetDimensionAttribute(element, HtmlName::kWidth, &width)) {
    page_dim->set_width(width);
  }
}

// If the element has a height attribute, set it in page_dim.
void SetHeightFromAttribute(const HtmlElement* element, ImageDim* page_dim) {
  int32 height;
  if (GetDimensionAttribute(element, HtmlName::kHeight, &height)) {
    page_dim->set_height(height);
  }
}

void DeleteMatchingImageDimsAfterInline(
    const CachedResult* cached, HtmlElement* element) {
  // Never strip width= or height= attributes from non-img elements.
  if (element->keyword() != HtmlName::kImg) {
    return;
  }
  // We used to take the absence of desired_image_dims here as license to delete
  // dimensions.  That was incorrect, as sometimes there were dimensions in the
  // page but the image was being enlarged on page and we can't strip the
  // enlargement out safely.  Now we also strip desired_image_dims when the
  // image is 1x1 or less.  As a result, we go back to the html to determine
  // whether it's safe to strip the width and height attributes, doing so only
  // if all dimensions that are present match the actual post-optimization image
  // dimensions.
  if (cached->has_image_file_dims()) {
    int attribute_width, attribute_height = -1;
    if (GetDimensionAttribute(element, HtmlName::kWidth, &attribute_width)) {
      if (cached->image_file_dims().width() == attribute_width) {
        // Width matches, height must either be absent or match.
        if (!element->FindAttribute(HtmlName::kHeight)) {
          // No height, just delete width.
          element->DeleteAttribute(HtmlName::kWidth);
        } else if (GetDimensionAttribute(
                element, HtmlName::kHeight, &attribute_height) &&
            cached->image_file_dims().height() == attribute_height) {
          // Both dimensions match, delete both.
          element->DeleteAttribute(HtmlName::kWidth);
          element->DeleteAttribute(HtmlName::kHeight);
        }
      }
    } else if (!element->FindAttribute(HtmlName::kWidth) &&
        GetDimensionAttribute(element, HtmlName::kHeight, &attribute_height) &&
        cached->image_file_dims().height() == attribute_height) {
      // No width, matching height
      element->DeleteAttribute(HtmlName::kHeight);
    }
  }
}

}  // namespace

bool ImageRewriteFilter::FinishRewriteImageUrl(
    const CachedResult* cached, const ResourceContext* resource_context,
    HtmlElement* element, HtmlElement::Attribute* src, int image_index,
    HtmlResourceSlot* slot, InlineResult* inline_result) {
  GoogleString src_value(src->DecodedValueOrNull());
  if (src_value.empty()) {
    return false;
  }

  const RewriteOptions* options = driver()->options();
  bool rewrote_url = false;
  bool image_inlined = false;
  const bool is_critical_image = IsHtmlCriticalImage(src_value);

  // Don't inline images used by responsive filter (except for the ones
  // explicitly marked as inlinable).
  const char* responsive_attr =
      element->AttributeValue(HtmlName::kDataPagespeedResponsiveTemp);
  if (responsive_attr != NULL &&
      StringPiece(responsive_attr) !=
      ResponsiveImageFirstFilter::kInlinableVirtualImage) {
    *inline_result = INLINE_RESPONSIVE;
  } else if (element->keyword() == HtmlName::kLink) {
    // Don't inline shortcut images.  All shortcut images are on link tags, and
    // no non-shortcut images are on link tags, so we can just check if this is
    // a link tag.  This is to exclude inlining on:
    // * <link rel=icon ...>
    // * <link rel=apple-touch-icon ...>
    // * <link rel=apple-touch-icon-precomposed ...>
    // * <link rel=apple-touch-startup-image ...>
    *inline_result = INLINE_SHORTCUT;
  } else {
    // See if we have a data URL, and if so use it if the browser can handle it
    // TODO(jmaessen): get rid of a string copy here. Tricky because
    // src->SetValue() copies implicitly.
    GoogleString data_url;
    // TODO(sligocki): Use different threshold for responsive images?
    *inline_result = TryInline(true /*in html*/, is_critical_image,
                               options->ImageInlineMaxBytes(),
                               cached, slot, &data_url);

    if (*inline_result == INLINE_SUCCESS) {
      DCHECK(!options->cache_small_images_unrewritten())
          << "Modifying a URL slot despite "
          << "image_inlining_identify_and_cache_without_rewriting set.";
      src->SetValue(data_url);
      // Note the use of the ORIGINAL url not the data url.
      LocalStorageCacheFilter::AddLscAttributes(src_value, *cached,
                                                driver(), element);
      // AddLscAttributes uses the width and height attributes so must be called
      // before we delete them with:
      DeleteMatchingImageDimsAfterInline(cached, element);
      image_inline_count_->Add(1);
      rewrote_url = true;
      image_inlined = true;
    }
  }

  // Rewrite URL in case this image was not inlined (and URL rewriting allowed).
  if (!image_inlined && !slot->preserve_urls()) {
    // Not inlined means we cannot store it in local storage.
    LocalStorageCacheFilter::RemoveLscAttributes(element, driver());
    if (cached->optimizable()) {
      // Rewritten HTTP url
      src->SetValue(ResourceSlot::RelativizeOrPassthrough(
          options, cached->url(), slot->url_relativity(),
          driver()->base_url()));
      image_rewrite_uses_->Add(1);
      rewrote_url = true;
    }
    if (options->Enabled(RewriteOptions::kInsertImageDimensions) &&
        (element->keyword() == HtmlName::kImg ||
         element->keyword() == HtmlName::kInput) &&
        !HasAnyDimensions(element) &&
        cached->has_image_file_dims() &&
        ImageUrlEncoder::HasValidDimensions(cached->image_file_dims())) {
      // Add image dimensions. We don't bother to resize if either dimension is
      // specified with units (em, %) rather than as absolute pixels. But note
      // that we DO attempt to include image dimensions even if we otherwise
      // choose not to optimize an image.
      const ImageDim& file_dims = cached->image_file_dims();
      driver()->AddAttribute(element, HtmlName::kWidth,
                             IntegerToString(file_dims.width()));
      driver()->AddAttribute(element, HtmlName::kHeight,
                             IntegerToString(file_dims.height()));
    }
    if (element->FindAttribute(HtmlName::kDataPagespeedResponsiveTemp) != NULL
        && cached->has_image_file_dims()
        && ImageUrlEncoder::HasValidDimensions(cached->image_file_dims())) {
      // If this is an image used by ResponsiveImageFilter, add information
      // on actual final dimensions used. That way we can decide which to use
      // in srcset and which to discard (because they are the same size as a
      // lower density image).
      const ImageDim& file_dims = cached->image_file_dims();
      driver()->AddAttribute(element, HtmlName::kDataActualWidth,
                             IntegerToString(file_dims.width()));
      driver()->AddAttribute(element, HtmlName::kDataActualHeight,
                             IntegerToString(file_dims.height()));
    }
  }

  bool low_res_src_inserted = false;
  bool try_low_res_src_insertion = false;
  ImageType low_res_image_type = IMAGE_UNKNOWN;
  if (options->Enabled(RewriteOptions::kDelayImages) &&
      src->keyword() == HtmlName::kSrc &&
      (element->keyword() == HtmlName::kImg ||
       element->keyword() == HtmlName::kInput)) {
    try_low_res_src_insertion = true;
    int max_preview_image_index = options->max_inlined_preview_images_index();
    if (!image_inlined &&
        !slot->preserve_urls() &&
        is_critical_image &&
        driver()->request_properties()->SupportsImageInlining() &&
        driver()->server_context()->critical_images_finder()->Available(
            driver()) != CriticalImagesFinder::kNoDataYet &&
        cached->has_low_resolution_inlined_data() &&
        (max_preview_image_index < 0 ||
         image_index < max_preview_image_index)) {
      low_res_image_type = static_cast<ImageType>(
          cached->low_resolution_inlined_image_type());

      const ContentType* content_type =
          Image::TypeToContentType(low_res_image_type);
      DCHECK(content_type != NULL) << "Invalid Image Type: "
                                   << low_res_image_type;
      if (content_type != NULL) {
        GoogleString data_url;
        DataUrl(*content_type, BASE64, cached->low_resolution_inlined_data(),
                &data_url);
        driver()->AddAttribute(
            element, HtmlName::kDataPagespeedLowResSrc, data_url);
        driver()->increment_num_inline_preview_images();
        low_res_src_inserted = true;
      } else {
        driver()->message_handler()->Message(kError,
                                             "Invalid low res image type: %d",
                                             low_res_image_type);
      }
    }
  }

  // Absolutify the image url for logging.
  GoogleUrl image_gurl(driver()->base_url(), src_value);
  driver()->log_record()->LogImageRewriteActivity(
      LoggingId(),
      image_gurl.spec_c_str(),
      (rewrote_url ?
       RewriterApplication::APPLIED_OK :
       RewriterApplication::NOT_APPLIED),
      image_inlined,
      is_critical_image,
      cached->optimizable(),
      cached->size(),
      try_low_res_src_insertion,
      low_res_src_inserted,
      low_res_image_type,
      cached->low_resolution_inlined_data().size());
  return rewrote_url;
}

void ImageRewriteFilter::SaveDebugMessageToCache(const GoogleString& message,
                                                 CachedResult* cached_result) {
  if (!message.empty()) {
    // We always save our result to our cache entry, since it will be propagated
    // to the parent automatically, and we need to be replayable independently.
    cached_result->add_debug_message(message);
  }
}

GoogleString ImageRewriteFilter::UrlForDebugMessages(
    const Context* rc) const {
  GoogleString slot_url = rc->slot(0)->resource()->url();
  GoogleUrl url(slot_url);
  if (!url.IsWebValid()) {
    return slot_url;
  }

  // If we're adjusting all the URLs based on domain lawyer, we should do so
  // with comments we add. If unoptimized URLs are left as is, we can do so
  // with debug comments as well.
  if (!rc->Options()->Enabled(RewriteOptions::kRewriteDomains)) {
    return slot_url;
  }

  GoogleString mapped;
  if (DomainRewriteFilter::Rewrite(url.Spec(), url,
                                   rc->FindServerContext(),
                                   rc->Options(),
                                   true, /* apply sharding */
                                   true, /* apply_domain_suffix*/
                                   &mapped)
       == DomainRewriteFilter::kRewroteDomain) {
    return mapped;
  }

  return slot_url;
}

bool ImageRewriteFilter::IsHtmlCriticalImage(StringPiece image_url) const {
  CriticalImagesFinder* finder =
      driver()->server_context()->critical_images_finder();
  if (finder->Available(driver()) != CriticalImagesFinder::kAvailable) {
    // Default to all images being critical if we don't have meaningful critical
    // image information.
    return true;
  }
  GoogleUrl image_gurl(driver()->base_url(), image_url);
  return finder->IsHtmlCriticalImage(image_gurl.Spec(), driver());
}

bool ImageRewriteFilter::StoreUrlInPropertyCache(const StringPiece& url) {
  if (url.length() == 0) {
    return true;
  }
  PropertyPage* property_page = driver()->property_page();
  if (property_page == NULL) {
    LOG(WARNING) << "image_inlining_identify_and_cache_without_rewriting "
                 << "without PropertyPage.";
    return false;
  }
  const PropertyCache::Cohort* cohort =
      driver()->server_context()->dom_cohort();
  if (cohort == NULL) {
    LOG(WARNING) << "image_inlining_identify_and_cache_without_rewriting "
                 << "without configured DOM cohort.";
    return false;
  }
  PropertyValue* value = property_page->GetProperty(
      cohort, kInlinableImageUrlsPropertyName);
  VLOG(3) << "image_inlining_identify_and_cache_without_rewriting value "
          << "inserted into pcache: " << url;
  GoogleString new_value(StrCat("\"", url, "\""));
  if (value->has_value()) {
    StrAppend(&new_value, ",", value->value());
  }
  property_page->UpdateValue(
      cohort, kInlinableImageUrlsPropertyName, new_value);
  return true;
}

bool ImageRewriteFilter::HasAnyDimensions(HtmlElement* element) {
  if (element->FindAttribute(HtmlName::kWidth)) {
    return true;
  }
  if (element->FindAttribute(HtmlName::kHeight)) {
    return true;
  }
  css_util::StyleExtractor extractor(element);
  return extractor.HasAnyDimensions();
}

bool ImageRewriteFilter::ParseDimensionAttribute(
    const char* position, int* value) {
  if (position == NULL) {
    return false;
  }
  // Note that we rely heavily on null-termination of char* here to cause our
  // control flow to fall through when we reach end of string.  Numbered steps
  // correspond to the steps in the spec.
  //   http://www.whatwg.org/specs/web-apps/current-work/multipage/
  //                  common-microsyntaxes.html#percentages-and-dimensions
  // 3) Skip ascii whitespace
  position = SkipAsciiWhitespace(position);
  // 5) Skip leading plus
  if (*position == '+') {
    ++position;
  }
  unsigned int result = 0;  // unsigned for consistent overflow behavior.
  // 6,7,9) Process digits
  while ('0' <= *position && *position <= '9') {
    unsigned int new_result = result * 10 + *position - '0';
    if (new_result < result) {
      // Integer overflow.  Reject.
      return false;
    }
    result = new_result;
    ++position;
  }
  // 6,7,8) Reject if no digits or only zeroes, or conversion to signed will
  // fail.
  if (result < 1 || INT_MAX < result) {
    return false;
  }
  // 11) Process fraction (including 45. with nothing after the . )
  if (*position == '.') {
    ++position;
    if ('5' <= *position && *position <= '9' && result < INT_MAX) {
      // Round based on leading fraction digit, avoiding overflow.
      ++result;
      ++position;
    }
    // Discard all fraction digits.
    while ('0' <= *position && *position <= '9') {
      ++position;
    }
  }
  // Skip whitespace before a possible trailing px.  The spec allows other junk,
  // or a trailing percent, but we can't resize percentages and older browsers
  // don't resize when they encounter junk.
  position = SkipAsciiWhitespace(position);
  if (position[0] == 'p' && position[1] == 'x') {
    position = SkipAsciiWhitespace(position + 2);
  }
  // Reject if there's trailing junk.
  if (*position != '\0') {
    return false;
  }
  // 14) return result as length.
  *value = static_cast<int>(result);
  return true;
}

void ImageRewriteFilter::GetDimensions(
    HtmlElement* element,
    ImageDim* page_dim,
    const HtmlElement::Attribute* src,
    bool* is_resized_using_rendered_dimensions) {
  css_util::StyleExtractor extractor(element);
  css_util::DimensionState state = extractor.state();
  int32 width = extractor.width();
  int32 height = extractor.height();
  int32 rendered_width = 0;
  int32 rendered_height = 0;
  // If the image has rendered dimensions stored in the property cache, update
  // the desired image dimensions. Don't use rendered image dimensions
  // when beaconing, since it would cause improper instrumentation.
  if (driver()->options()->Enabled(
          RewriteOptions::kResizeToRenderedImageDimensions) &&
      !CriticalImagesBeaconFilter::ShouldApply(driver())) {
    StringPiece src_value(src->DecodedValueOrNull());
    if (!src_value.empty()) {
      GoogleUrl src_gurl(driver()->base_url(), src_value);
      if (src_gurl.IsWebOrDataValid()) {
        std::pair<int32, int32> dimensions;
        CriticalImagesFinder* finder =
            driver()->server_context()->critical_images_finder();
        if (finder->GetRenderedImageDimensions(
                driver(), src_gurl, &dimensions)) {
          if (dimensions.first != 0 && dimensions.second != 0) {
            rendered_width = dimensions.first;
            rendered_height = dimensions.second;
          }
        }
      }
    }
  }
  // If we didn't get a height dimension above, but there is a height
  // value in the style attribute, that means there's a height value
  // we can't process. This height will trump the height attribute in the
  // image tag, so we need to avoid resizing.
  // The same is true of width.
  switch (state) {
    case css_util::kNotParsable:
      break;
    case css_util::kHasBothDimensions:
      page_dim->set_width(width);
      page_dim->set_height(height);
      break;
    case css_util::kHasHeightOnly:
      page_dim->set_height(height);
      SetWidthFromAttribute(element, page_dim);
      break;
    case css_util::kHasWidthOnly:
      page_dim->set_width(width);
      SetHeightFromAttribute(element, page_dim);
      break;
    case css_util::kNoDimensions:
      SetWidthFromAttribute(element, page_dim);
      SetHeightFromAttribute(element, page_dim);
      break;
  }

  // If the area of image using rendered dimensions is less than the dimensions
  // from the style or image tag attributes, then only resize using rendered
  // dimensions.
  int64 rendered_area = rendered_width * rendered_height;
  int64 image_attribute_area = page_dim->width() * page_dim->height();
  // Note: we check for image_attribute_area = 1 (-1 * -1 = 1) when we have
  // -1(unset) for both height and width from the image attributes.
  if (rendered_area != 0 && ((image_attribute_area != 1 &&
       rendered_area < image_attribute_area) ||
      (image_attribute_area == 1))) {
    page_dim->set_width(rendered_width);
    page_dim->set_height(rendered_height);
    *is_resized_using_rendered_dimensions = true;
    image_resized_using_rendered_dimensions_->Add(1);
  }
}

InlineResult ImageRewriteFilter::TryInline(bool is_html, bool is_critical,
    int64 image_inline_max_bytes, const CachedResult* cached_result,
    ResourceSlot* slot, GoogleString* data_url) {
  int32 image_type_value = cached_result->inlined_image_type();
  if (!ImageType_IsValid(image_type_value)) {
    LOG(DFATAL) << "Invalid inlined_image_type in cached_result";
    return INLINE_INTERNAL_ERROR;
  }
  ImageType image_type = static_cast<ImageType>(image_type_value);

  const RequestProperties* request_properties = driver()->request_properties();
  if (!request_properties->SupportsImageInlining() ||
      ((image_type == IMAGE_WEBP ||
        image_type == IMAGE_WEBP_LOSSLESS_OR_ALPHA) &&
       request_properties->ForbidWebpInlining())) {
    return INLINE_UNSUPPORTED_DEVICE;
  }
  if (is_html && driver()->options()->inline_only_critical_images() &&
      !is_critical) {
    return INLINE_NOT_CRITICAL;
  }
  if (!cached_result->has_inlined_data()) {
    return INLINE_NO_DATA;
  }
  StringPiece data = cached_result->inlined_data();
  if (static_cast<int64>(data.size()) >= image_inline_max_bytes) {
    return INLINE_TOO_LARGE;
  }

  // This is the decision point for whether or not an image is suitable for
  // inlining. After this point, we may skip inlining an image, but not
  // because of properties of the image.
  const RewriteOptions* options = driver()->options();
  if (options->cache_small_images_unrewritten()) {
    // Skip rewriting, record the URL for storage in the property cache,
    // suppress future rewrites to this slot, and return immediately.
    GoogleString url(slot->resource()->url());

    // Duplicate URLs are suppressed.
    if (inlinable_urls_.insert(url).second) {
      // This write to the property value allows downstream filters to observe
      // inlinable images within the same flush window. Note that this does not
      // induce a write to the underlying cache -- the value is written only
      // when the filter chain has finished execution.
      StoreUrlInPropertyCache(url);
    }
    // We disable rendering to prevent any rewriting of the URL that we'll
    // advertise in the property cache.
    slot->set_disable_rendering(true);
    return INLINE_CACHE_SMALL_IMAGES_UNREWRITTEN;
  }
  DataUrl(*Image::TypeToContentType(image_type), BASE64, data, data_url);
  return INLINE_SUCCESS;
}

void ImageRewriteFilter::StartElementImpl(HtmlElement* element) {

  // Don't rewrite if there is a pagespeed_no_transform or
  // data-pagespeed-no-transform attribute.
  if (element->FindAttribute(HtmlName::kDataPagespeedNoTransform)) {
    // Remove the attribute
    element->DeleteAttribute(HtmlName::kDataPagespeedNoTransform);
    return;
  }
  if (element->FindAttribute(HtmlName::kPagespeedNoTransform)) {
    // Remove the attribute
    element->DeleteAttribute(HtmlName::kPagespeedNoTransform);
    return;
  }
  // Rewrite any image-valued attributes we find.
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver()->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    if (attributes[i].category != semantic_type::kImage ||
        attributes[i].url->DecodedValueOrNull() == NULL) {
      continue;
    }

    // The LSC filter only knows how to handle the src attribute.
    if (attributes[i].url->keyword() == HtmlName::kSrc) {
      // Ask the LSC filter to work out how to handle this element. A return
      // value of true means we don't have to rewrite it so can skip that.
      // The state is carried forward to after we initiate rewriting since
      // we might still have to modify the element.
      LocalStorageCacheFilter::InlineState state;
      if (LocalStorageCacheFilter::AddStorableResource(
              attributes[i].url->DecodedValueOrNull(),
              driver(),
              false /* check cookie */,
              element, &state)) {
        continue;
      }
    }

    BeginRewriteImageUrl(element, attributes[i].url);
  }

  if (element->keyword() == HtmlName::kImg) {
    HtmlElement::Attribute* srcset = element->FindAttribute(HtmlName::kSrcset);
    if (srcset != nullptr) {
      BeginRewriteSrcSet(element, srcset);
    }
  }
}

const UrlSegmentEncoder* ImageRewriteFilter::encoder() const {
  return &encoder_;
}

void ImageRewriteFilter::EncodeUserAgentIntoResourceContext(
    ResourceContext* context) const {
  ImageUrlEncoder::SetWebpAndMobileUserAgent(*driver(), context);
  CssUrlEncoder::SetInliningImages(*driver()->request_properties(), context);
  ImageUrlEncoder::SetSmallScreen(*driver(), context);

  context->set_may_use_save_data_quality(
      driver()->options()->SupportSaveData() &&
      driver()->request_properties()->RequestsSaveData());
}

RewriteContext* ImageRewriteFilter::MakeRewriteContext() {
  ResourceContext* resource_context = new ResourceContext;
  EncodeUserAgentIntoResourceContext(resource_context);
  return new Context(0 /*No CSS inlining, it's html */,
                     this, driver(), NULL /*not nested */,
                     resource_context,
                     Context::Place::kFetch,
                     kNotCriticalIndex,
                     false /*not in noscript */,
                     false /*not resized by rendered dimensions*/);
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContextForCss(
    int64 css_image_inline_max_bytes, RewriteContext* parent,
    const ResourceSlotPtr& slot) {
  // Copy over the ResourceContext from the parent RewriteContext so that we
  // preserve request specific options, such as whether WebP rewriting is
  // allowed.
  ResourceContext* cloned_context = new ResourceContext;
  const ResourceContext* parent_context = parent->resource_context();
  if (parent_context != NULL) {
    *cloned_context = *parent_context;
  }

  if (cloned_context->libwebp_level() != ResourceContext::LIBWEBP_NONE) {
    // Assignment from parent_context is not sufficient because parent_context
    // checks only UserAgentSupportsWebp when creating the context, but while
    // rewriting the image, rewrite options should also be checked.
    ImageUrlEncoder::SetLibWebpLevel(
        *driver()->options(), *driver()->request_properties(),
        cloned_context);
  }
  Context* context = new Context(css_image_inline_max_bytes,
                                 this, NULL /* driver*/, parent,
                                 cloned_context,
                                 Context::Place::kCss,
                                 kNotCriticalIndex,
                                 false /*not in noscript */,
                                 false /*not resized by rendered dimensions*/);
  context->AddSlot(slot);
  return context;
}

RewriteContext* ImageRewriteFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  ResourceContext* resource_context = new ResourceContext;
  DCHECK(parent != NULL);
  DCHECK(parent->resource_context() != NULL);
  if (parent != NULL && parent->resource_context() != NULL) {
    *resource_context = *(parent->resource_context());
  }
  Context* context = new Context(
      0 /*No Css inling */, this, NULL /* driver */, parent, resource_context,
      Context::Place::kNonCssNested, kNotCriticalIndex,
      false /*not in noscript */, false /*not resized by rendered dimensions*/);
  context->AddSlot(slot);
  return context;
}

bool ImageRewriteFilter::UpdateDesiredImageDimsIfNecessary(
    const ImageDim& image_dim, const ResourceContext& resource_context,
    ImageDim* desired_dim) {
  return false;
}

const RewriteOptions::Filter* ImageRewriteFilter::RelatedFilters(
    int* num_filters) const {
  *num_filters = kRelatedFiltersSize;
  return kRelatedFilters;
}

void ImageRewriteFilter::DisableRelatedFilters(RewriteOptions* options) {
  for (int i = 0; i < kRelatedFiltersSize; ++i) {
    options->DisableFilter(kRelatedFilters[i]);
  }
}

void ImageRewriteFilter::RegisterImageInfo(
    const AssociatedImageInfo& image_info) {
  if (!driver()->options()->Enabled(
          RewriteOptions::kExperimentCollectMobImageInfo)) {
    return;
  }

  image_info_[image_info.url()] = image_info;
}

void ImageRewriteFilter::ReportDroppedRewrite() {
  image_rewrites_dropped_due_to_load_->IncBy(1);
}

bool ImageRewriteFilter::ExtractAssociatedImageInfo(
    const CachedResult* result, RewriteContext* context,
    AssociatedImageInfo* out) {
  bool ret = false;
  if (result->has_image_file_dims()) {
    if (result->url().empty()) {
      if (context->num_slots() == 1) {
        out->set_url(context->slot(0)->resource()->url());
        ret = true;
      }
    } else {
      out->set_url(result->url());
      ret = true;
    }
  }
  if (ret) {
    *out->mutable_dimensions() = result->image_file_dims();
  }
  return ret;
}

}  // namespace net_instaweb
