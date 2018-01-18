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


#include "net/instaweb/rewriter/public/responsive_image_filter.h"

#include <cstddef>                     // for size_t
#include <memory>
#include <utility>                      // for pair

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

const char ResponsiveImageFirstFilter::kOriginalImage[] = "original";
const char ResponsiveImageFirstFilter::kNonInlinableVirtualImage[] =
    "non-inlinable-virtual";
const char ResponsiveImageFirstFilter::kInlinableVirtualImage[] =
    "inlinable-virtual";
const char ResponsiveImageFirstFilter::kFullsizedVirtualImage[] =
    "fullsized-virtual";

ResponsiveImageFirstFilter::ResponsiveImageFirstFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      densities_(driver->options()->responsive_image_densities()) {
  CHECK(!densities_.empty());
}

ResponsiveImageFirstFilter::~ResponsiveImageFirstFilter() {
}

void ResponsiveImageFirstFilter::StartDocumentImpl() {
  candidate_map_.clear();
}

void ResponsiveImageFirstFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg) {
    return;
  }

  if (element->AttributeValue(HtmlName::kSrc) == nullptr) {
    driver()->InsertDebugComment("Responsive image URL not decodable", element);
  } else if (element->HasAttribute(HtmlName::kDataPagespeedNoTransform) ||
             element->HasAttribute(HtmlName::kPagespeedNoTransform)) {
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because of "
        "data-pagespeed-no-transform attribute.", element);
  } else if (element->HasAttribute(HtmlName::kSrcset)) {
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because image already "
        "has one.", element);
  } else if (!element->HasAttribute(HtmlName::kDataPagespeedResponsiveTemp)) {
    // On first run of this filter, split <img> element into multiple
    // elements.
    AddHiResImages(element);
  }
}

// Adds dummy images for 1.5x and 2x resolutions. Note: this converts:
//   <img src=foo.jpg width=w height=h>
// into:
//   <img src=foo.jpg width=1.5w height=1.5h pagespeed_responsive_temp>
//   <img src=foo.jpg width=2w height=2h pagespeed_responsive_temp>
//   <img src=foo.jpg width=w height=h>
// The order of these images doesn't really matter, but adding them before
// this image avoids some extra processing of the added dummy images by
// ResponsiveImageFirstFilter.
void ResponsiveImageFirstFilter::AddHiResImages(HtmlElement* element) {
  const HtmlElement::Attribute* src_attr =
    element->FindAttribute(HtmlName::kSrc);
  // TODO(sligocki): width and height attributes can lie. Perhaps we should
  // look at rendered image dimensions (via beaconing back from clients).
  const char* width_str = element->AttributeValue(HtmlName::kWidth);
  const char* height_str = element->AttributeValue(HtmlName::kHeight);
  if ((src_attr == NULL) || (width_str == NULL) || (height_str == NULL)) {
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because image does not "
        "have dimensions (or a src URL).", element);
    return;
  }

  int orig_width, orig_height;
  if (StringToInt(width_str, &orig_width) &&
      StringToInt(height_str, &orig_height)) {
    if (orig_width <= 1 || orig_height <= 1) {
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset to tracking pixel.",
          element);
      return;
    }

    // TODO(sligocki): Possibly use lower quality settings for 1.5x and 2x
    // because standard quality-85 are overkill for high density displays.
    // However, we might want high quality for zoom.
    ResponsiveVirtualImages virtual_images;
    virtual_images.width = orig_width;
    virtual_images.height = orig_height;

    for (size_t i = 0, n = densities_.size(); i < n; ++i) {
      virtual_images.non_inlinable_candidates.push_back(
          AddHiResVersion(element, *src_attr, orig_width, orig_height,
                          kNonInlinableVirtualImage, densities_[i]));
    }

    // Highest quality version.
    virtual_images.inlinable_candidate =
        AddHiResVersion(element, *src_attr, orig_width, orig_height,
                        kInlinableVirtualImage,
                        densities_[densities_.size() - 1]);

    virtual_images.fullsized_candidate =
        AddHiResVersion(element, *src_attr, orig_width, orig_height,
                        kFullsizedVirtualImage, -1);

    candidate_map_[element] = virtual_images;

    // Mark this element as responsive as well, so that ImageRewriteFilter will
    // add actual final dimensions to the tag.
    driver()->AddAttribute(element, HtmlName::kDataPagespeedResponsiveTemp,
                           kOriginalImage);
  }
}

ResponsiveImageCandidate ResponsiveImageFirstFilter::AddHiResVersion(
    HtmlElement* img, const HtmlElement::Attribute& src_attr,
    int orig_width, int orig_height, StringPiece responsive_attribute_value,
    double resolution) {
  HtmlElement* new_img = driver()->NewElement(img->parent(), HtmlName::kImg);
  new_img->AddAttribute(src_attr);
  driver()->AddAttribute(new_img, HtmlName::kDataPagespeedResponsiveTemp,
                         responsive_attribute_value);
  if (resolution > 0) {
    // Note: We truncate width and height to integers here.
    driver()->AddAttribute(new_img, HtmlName::kWidth,
                           IntegerToString(orig_width * resolution));
    driver()->AddAttribute(new_img, HtmlName::kHeight,
                           IntegerToString(orig_height * resolution));
  }
  driver()->InsertNodeBeforeNode(img, new_img);
  ResponsiveImageCandidate candidate(new_img, resolution);
  return candidate;
}

ResponsiveImageSecondFilter::ResponsiveImageSecondFilter(
    RewriteDriver* driver, const ResponsiveImageFirstFilter* first_filter)
  : CommonFilter(driver),
    responsive_js_url_(
        driver->server_context()->static_asset_manager()->GetAssetUrl(
            StaticAssetEnum::RESPONSIVE_JS, driver->options())),
    first_filter_(first_filter),
    zoom_filter_enabled_(driver->options()->Enabled(
        RewriteOptions::kResponsiveImagesZoom)),
    srcsets_added_(false) {
}

ResponsiveImageSecondFilter::~ResponsiveImageSecondFilter() {
}

void ResponsiveImageSecondFilter::StartDocumentImpl() {
  srcsets_added_ = false;
}

void ResponsiveImageSecondFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() != HtmlName::kImg) {
    return;
  }

  ResponsiveImageCandidateMap::const_iterator p =
      first_filter_->candidate_map_.find(element);
  if (p != first_filter_->candidate_map_.end()) {
    // On second run of the filter, combine the elements back together.
    const ResponsiveVirtualImages& virtual_images = p->second;
    CombineHiResImages(element, virtual_images);
    Cleanup(element, virtual_images);
  }
}

namespace {

// Get actual dimensions. These are inserted by ImageRewriteFilter as
// attributes on all images involved in the responsive flow.
ImageDim ActualDims(const HtmlElement* element) {
  ImageDim dims;

  int height;
  const char* height_str = element->AttributeValue(HtmlName::kDataActualHeight);
  if (height_str != NULL && StringToInt(height_str, &height)) {
    dims.set_height(height);
  }

  int width;
  const char* width_str = element->AttributeValue(HtmlName::kDataActualWidth);
  if (width_str != NULL && StringToInt(width_str, &width)) {
    dims.set_width(width);
  }

  return dims;
}

GoogleString ResolutionToString(double resolution) {
  // Max 4 digits of precission.
  return StringPrintf("%.4g", resolution);
}

}  // namespace

// Combines information from dummy 1.5x and 2x images into the 1x srcset.
void ResponsiveImageSecondFilter::CombineHiResImages(
    HtmlElement* orig_element,
    const ResponsiveVirtualImages& virtual_images) {
  // If the highest resolution image was inlinable, use that as the only
  // version of the image (no srcset).
  const char* inlinable_src =
      virtual_images.inlinable_candidate.element->AttributeValue(
          HtmlName::kSrc);
  if (IsDataUrl(inlinable_src)) {
    // Note: This throws away any Local Storage attributes associated with this
    // inlined image. Maybe we should copy those over as well?
    orig_element->DeleteAttribute(HtmlName::kSrc);
    driver()->AddAttribute(orig_element, HtmlName::kSrc, inlinable_src);
    return;
  }

  ResponsiveImageCandidateVector candidates =
      virtual_images.non_inlinable_candidates;

  // Find out what resolution fullsize image is and add it to candidates list.
  ResponsiveImageCandidate fullsized = virtual_images.fullsized_candidate;
  ImageDim full_dims = ActualDims(fullsized.element);
  if (full_dims.width() > 0) {
    fullsized.resolution =
        static_cast<double>(full_dims.width()) / virtual_images.width;
    candidates.push_back(fullsized);
  }

  const char* x1_src = orig_element->AttributeValue(HtmlName::kSrc);

  if (x1_src == NULL) {
    // Should not happen. We explicitly checked that <img> had a decodeable
    // src= attribute in ResponsiveImageFirstFilter::AddHiResImages().
    LOG(DFATAL) << "Original responsive image has no decodeable URL: "
                << orig_element->ToString();
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because original image has "
        "no src URL.", orig_element);
    return;
  } else if (IsDataUrl(x1_src)) {
    // Should not happen. ImageRewriteFilter should never inline the original
    // image. Instead, if the image is small enough it will be inlined via the
    // inlinable virtual image.
    driver()->InsertDebugComment(
        "ResponsiveImageFilter: Not adding srcset because original image was "
        "inlined.", orig_element);
    return;
  }

  GoogleString srcset_value;
  // Keep track of last candidate's URL. If next candidate has same URL,
  // don't include it in the srcset.
  GoogleString last_src = x1_src;
  // Keep track of actual final dimensions of last candidate. If next candidate
  // has same actual dimensions, we don't include it in the srcset.
  ImageDim last_dims = ActualDims(orig_element);
  bool added_hi_res = false;

  for (int i = 0, n = candidates.size(); i < n; ++i) {
    const char* src = candidates[i].element->AttributeValue(HtmlName::kSrc);

    if (src == NULL) {
      // Should not happen. We explicitly created a src= attribute in
      // ResponsiveImageFirstFilter::AddHiResVersion().
      LOG(DFATAL) << "Virtual responsive image has no URL.";
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset because virtual image has "
          "no src URL.", orig_element);
      return;
    } else if (IsDataUrl(src)) {
      // Should not happen. ImageRewriteFilter should never inline these
      // non-inlinable virtual images.
      LOG(DFATAL) << "Non-inlinable image was inlined.";
      driver()->InsertDebugComment(
          "ResponsiveImageFilter: Not adding srcset because virtual image was "
          "unexpectedly inlined.", orig_element);
      return;
    }

    ImageDim dims = ActualDims(candidates[i].element);
    if (src == last_src) {
      if (driver()->DebugMode()) {
        driver()->InsertDebugComment(StrCat(
            "ResponsiveImageFilter: Not adding ",
            ResolutionToString(candidates[i].resolution), "x candidate to "
            "srcset because it is the same as previous candidate."),
                                     orig_element);
      }
    // TODO(sligocki): Remove last candidate if dimensions are too close to
    // this candidate. Ex: if 1.5x is 99x99 and 2x is 100x100, obviously we
    // should remove 1.5x version.
    } else if (dims.height() == last_dims.height() &&
               dims.width() == last_dims.width()) {
      if (driver()->DebugMode()) {
        driver()->InsertDebugComment(StrCat(
            "ResponsiveImageFilter: Not adding ",
            ResolutionToString(candidates[i].resolution), "x candidate to "
            "srcset because native image was not high enough resolution."),
                                     orig_element);
      }
    } else {
      if (added_hi_res) {
        StrAppend(&srcset_value, ",");
      }
      // Note: Escaping and parsing rules for srcsets are very strange.
      // Specifically, URLs in srcsets are not allowed to start nor end
      // with a comma. Commas are allowed in the middle of a URL and do not
      // need to be escaped. In fact, they are reserved chars in the URL spec
      // (rfc 3986 2.2) and so escaping them as %2C would potentially change
      // the meaning of the URL.
      // See: http://www.w3.org/html/wg/drafts/html/master/semantics.html#attr-img-srcset
      //
      // Note: PageSpeed resized images will never begin nor end with a comma.
      StringPiece src_sp(src);
      if (src_sp.ends_with(",") || src_sp.starts_with(",")) {
        driver()->InsertDebugComment(StrCat(
            "ResponsiveImageFilter: Not adding srcset because one of the "
            "candidate URLs starts or ends with a comma: ", src_sp),
                                     orig_element);
        return;
      }
      // However it appears that all spaces do need to be percent escaped.
      // Otherwise srcset parsing would be ambiguous.
      GoogleString src_escaped = GoogleUrl::Sanitize(src);

      GoogleString resolution_string =
          ResolutionToString(candidates[i].resolution);
      StrAppend(&srcset_value, src_escaped, " ", resolution_string, "x");

      last_src = src;
      last_dims = dims;
      added_hi_res = true;
    }
  }

  if (added_hi_res) {
    driver()->AddAttribute(orig_element, HtmlName::kSrcset, srcset_value);
    srcsets_added_ = true;
  }
}

namespace {

// Helper function which never returns NULL (and is thus safe to use directly
// in printf, etc.).
const char* AttributeValueOrEmpty(const HtmlElement* element,
                                  const HtmlName::Keyword attr_name) {
  const char* ret = element->AttributeValue(attr_name);
  if (ret == NULL) {
    return "";
  } else {
    return ret;
  }
}

}  // namespace

void ResponsiveImageSecondFilter::InsertPlaceholderDebugComment(
    const ResponsiveImageCandidate& candidate, const char* qualifier) {
  if (driver()->DebugMode()) {
    GoogleString resolution_str;
    if (candidate.resolution > 0) {
      resolution_str =
          StrCat(" ", ResolutionToString(candidate.resolution), "x");
    }
    driver()->InsertDebugComment(StrCat(
        "ResponsiveImageFilter: Any debug messages after this refer to the "
        "virtual", qualifier, resolution_str, " image with "
        "src=", AttributeValueOrEmpty(candidate.element, HtmlName::kSrc),
        " width=", AttributeValueOrEmpty(candidate.element, HtmlName::kWidth),
        " height=", AttributeValueOrEmpty(candidate.element,
                                          HtmlName::kHeight)),
                                 candidate.element);
  }
}

void ResponsiveImageSecondFilter::Cleanup(
    HtmlElement* orig_element,
    const ResponsiveVirtualImages& virtual_images) {
  for (int i = 0, n = virtual_images.non_inlinable_candidates.size();
       i < n; ++i) {
    InsertPlaceholderDebugComment(virtual_images.non_inlinable_candidates[i],
                                  "");
    driver()->DeleteNode(virtual_images.non_inlinable_candidates[i].element);
  }

  InsertPlaceholderDebugComment(virtual_images.inlinable_candidate,
                                " inlinable");
  driver()->DeleteNode(virtual_images.inlinable_candidate.element);

  InsertPlaceholderDebugComment(virtual_images.fullsized_candidate,
                                " full-sized");
  driver()->DeleteNode(virtual_images.fullsized_candidate.element);

  orig_element->DeleteAttribute(HtmlName::kDataPagespeedResponsiveTemp);
  orig_element->DeleteAttribute(HtmlName::kDataActualHeight);
  orig_element->DeleteAttribute(HtmlName::kDataActualWidth);
}

void ResponsiveImageSecondFilter::EndDocument() {
  if (zoom_filter_enabled_ && srcsets_added_ && !driver()->is_amp_document()) {
    if (IsRelativeUrlLoadPermittedByCsp(
            responsive_js_url_, CspDirective::kScriptSrc)) {
      HtmlElement* script = driver()->NewElement(nullptr, HtmlName::kScript);
      driver()->AddAttribute(script, HtmlName::kSrc, responsive_js_url_);
      InsertNodeAtBodyEnd(script);
    } else if (DebugMode()) {
      HtmlNode* comment_node = driver()->NewCommentNode(
          nullptr, "ResponsiveImageFilter: cannot insert zoom JS as "
                   "Content-Security-Policy would disallow it");
       InsertNodeAtBodyEnd(comment_node);
    }
  }
}

}  // namespace net_instaweb
