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


#include "net/instaweb/rewriter/public/request_properties.h"

#include "net/instaweb/rewriter/public/device_properties.h"
#include "net/instaweb/rewriter/public/downstream_caching_directives.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

RequestProperties::RequestProperties(UserAgentMatcher* matcher)
    : device_properties_(new DeviceProperties(matcher)),
      downstream_caching_directives_(new DownstreamCachingDirectives()),
      supports_image_inlining_(kNotSet),
      supports_js_defer_(kNotSet),
      supports_lazyload_images_(kNotSet),
      supports_webp_in_place_(kNotSet),
      supports_webp_rewritten_urls_(kNotSet),
      supports_webp_lossless_alpha_(kNotSet),
      supports_webp_animated_(kNotSet) {
}

RequestProperties::~RequestProperties() {
}

void RequestProperties::SetUserAgent(StringPiece user_agent_string) {
  device_properties_->SetUserAgent(user_agent_string);
}

void RequestProperties::ParseRequestHeaders(
    const RequestHeaders& request_headers) {
  device_properties_->ParseRequestHeaders(request_headers);
  downstream_caching_directives_->ParseCapabilityListFromRequestHeaders(
                                      request_headers);
}


bool RequestProperties::SupportsImageInlining() const {
  if (supports_image_inlining_ == kNotSet) {
    supports_image_inlining_ =
        (downstream_caching_directives_->SupportsImageInlining() &&
         device_properties_->SupportsImageInlining()) ?
        kTrue :
        kFalse;
  }
  return (supports_image_inlining_ == kTrue);
}

bool RequestProperties::SupportsLazyloadImages() const {
  if (supports_lazyload_images_ == kNotSet) {
    supports_lazyload_images_ =
        (downstream_caching_directives_->SupportsLazyloadImages() &&
         device_properties_->SupportsLazyloadImages()) ?
        kTrue :
        kFalse;
  }
  return (supports_lazyload_images_ == kTrue);
}

bool RequestProperties::SupportsCriticalCss() const {
  return device_properties_->SupportsCriticalCss();
}

bool RequestProperties::AcceptsGzip() const {
  return device_properties_->AcceptsGzip();
}

bool RequestProperties::SupportsCriticalCssBeacon() const {
  // For bots, we don't allow instrumentation, but we do allow bots to use
  // previous instrumentation results collected by non-bots to enable the
  // prioritize_critical_css rewriter.
  return SupportsCriticalCss() && !IsBot();
}

bool RequestProperties::SupportsCriticalImagesBeacon() const {
  // For now this script has the same user agent requirements as image inlining,
  // however that could change in the future if more advanced JS is used by the
  // beacon.
  return device_properties_->SupportsCriticalImagesBeacon();
}

// Note that the result of the function is cached as supports_js_defer_. This
// must be cleared before calling the function a second time with a different
// value for allow_mobile.
bool RequestProperties::SupportsJsDefer(bool allow_mobile) const {
  if (supports_js_defer_ == kNotSet) {
    supports_js_defer_ =
        (downstream_caching_directives_->SupportsJsDefer() &&
         device_properties_->SupportsJsDefer(allow_mobile)) ?
        kTrue :
        kFalse;
  }
  return (supports_js_defer_ == kTrue);
}

bool RequestProperties::SupportsWebpInPlace() const {
  if (supports_webp_in_place_ == kNotSet) {
    supports_webp_in_place_ =
        (downstream_caching_directives_->SupportsWebp() &&
         device_properties_->SupportsWebpInPlace()) ?
        kTrue :
        kFalse;
  }
  return (supports_webp_in_place_ == kTrue);
}

bool RequestProperties::SupportsWebpRewrittenUrls() const {
  if (supports_webp_rewritten_urls_ == kNotSet) {
    supports_webp_rewritten_urls_ =
        (downstream_caching_directives_->SupportsWebp() &&
         device_properties_->SupportsWebpRewrittenUrls()) ?
        kTrue :
        kFalse;
  }
  return (supports_webp_rewritten_urls_ == kTrue);
}

bool RequestProperties::SupportsWebpLosslessAlpha() const {
  if (supports_webp_lossless_alpha_ == kNotSet) {
    supports_webp_lossless_alpha_ =
        (downstream_caching_directives_->SupportsWebpLosslessAlpha() &&
         device_properties_->SupportsWebpLosslessAlpha()) ?
        kTrue :
        kFalse;
  }
  return (supports_webp_lossless_alpha_ == kTrue);
}

bool RequestProperties::SupportsWebpAnimated() const {
  if (supports_webp_animated_ == kNotSet) {
    supports_webp_animated_ =
        (downstream_caching_directives_->SupportsWebpAnimated() &&
         device_properties_->SupportsWebpAnimated()) ?
        kTrue :
        kFalse;
  }
  return (supports_webp_animated_ == kTrue);
}

bool RequestProperties::IsBot() const {
  return device_properties_->IsBot();
}

bool RequestProperties::IsMobile() const {
  return device_properties_->IsMobile();
}

bool RequestProperties::IsTablet() const {
  return device_properties_->IsTablet();
}

UserAgentMatcher::DeviceType RequestProperties::GetDeviceType() const {
  return device_properties_->GetDeviceType();
}

void RequestProperties::LogDeviceInfo(
    AbstractLogRecord* log_record,
    bool enable_aggressive_rewriters_for_mobile) {
  log_record->LogDeviceInfo(
      GetDeviceType(),
      SupportsImageInlining(),
      SupportsLazyloadImages(),
      SupportsCriticalImagesBeacon(),
      SupportsJsDefer(enable_aggressive_rewriters_for_mobile),
      SupportsWebpInPlace(),
      SupportsWebpRewrittenUrls(),
      SupportsWebpLosslessAlpha(),
      IsBot());
}

bool RequestProperties::ForbidWebpInlining() const {
  return device_properties_->ForbidWebpInlining();
}

bool RequestProperties::RequestsSaveData() const {
  return device_properties_->RequestsSaveData();
}

bool RequestProperties::HasViaHeader() const {
  return device_properties_->HasViaHeader();
}

}  // namespace net_instaweb
