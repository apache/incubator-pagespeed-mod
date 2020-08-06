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

#include "pagespeed/kernel/http/user_agent_matcher.h"

#include <map>
#include <memory>
#include <utility>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/re2.h"

namespace net_instaweb {

const char UserAgentMatcher::kTestUserAgentWebP[] = "test-user-agent-webp";
// Note that this must not contain the substring "webp".
const char UserAgentMatcher::kTestUserAgentNoWebP[] = "test-user-agent-no";

class RequestHeaders;

// These are the user-agents of browsers/mobile devices which support
// image-inlining. The data is from "Latest WURFL Repository"(mobile devices)
// and "Web Patch"(browsers) on http://wurfl.sourceforge.net
// The user-agent string for Opera could be in the form of "Opera 7" or
// "Opera/7", we use the wildcard pattern "Opera?7" for this case.
namespace {

const char kGooglePlusUserAgent[] =
    "*Google (+https://developers.google.com/+/web/snippet/)*";

const char* kImageInliningAllowlist[] = {
    "*Android*",
    "*Chrome/*",
    "*Firefox/*",
    "*iPad*",
    "*iPhone*",
    "*iPod*",
    "*itouch*",
    "*Opera*",
    "*Safari*",
    "*Wget*",
    // Allow in ads policy checks to match usual UA behavior.
    "AdsBot-Google*",
    // Plus IE, see use in the code.
    // The following user agents are used only for internal testing
    "google command line rewriter",
    "webp",
    "webp-la",
};
const char* kImageInliningBlockedlist[] = {
    "*Firefox/1.*", "*Firefox/2.*", "*MSIE 5.*", "*MSIE 6.*",
    "*MSIE 7.*",    "*Opera?5*",    "*Opera?6*", kGooglePlusUserAgent};

// Exclude BlackBerry OS 5.0 and older. See
// http://supportforums.blackberry.com/t5/Web-and-WebWorks-Development/How-to-detect-the-BlackBerry-Browser/ta-p/559862
// for details on BlackBerry UAs.
// Exclude all Opera Mini: see bug #1070.
// https://github.com/apache/incubator-pagespeed-mod/issues/1070
const char* kLazyloadImagesBlockedlist[] = {"BlackBerry*CLDC*", "*Opera Mini*",
                                            kGooglePlusUserAgent};

// For defer js we only allow Firefox4+, IE8+, safari and Chrome
// We'll be updating this as and when required.
// The blockedlist is checked first, then if not in there, the allowlist is
// checked.
// Do allow googlebot, since we run defer js for modern browsers.
// Note: None of the following should match a mobile UA.
const char* kDeferJSAllowlist[] = {"*Chrome/*", "*Firefox/*", "*Safari*",
                                   // Plus IE, see code below.
                                   "*Wget*", "*Googlebot*",
                                   "*Mediapartners-Google*"};
const char* kDeferJSBlockedlist[] = {
    "*Firefox/1.*", "*Firefox/2.*", "*Firefox/3.*", "*MSIE 5.*",
    "*MSIE 6.*",    "*MSIE 7.*",    "*MSIE 8.*",
};
const char* kDeferJSMobileAllowlist[] = {
    "*AppleWebKit/*",
};

// Webp support for most devices should be triggered on Accept:image/webp.
// However we special-case Android 4.0 browsers which are fairly commonly
// used, support webp, and don't send Accept:image/webp.  Very old versions
// of Chrome may support webp without Accept:image/webp, but it is safe to
// ignore them because they are extremely rare.
//
// For legacy webp rewriting, we allowlist Android, but blockedlist
// older versions and Firefox, which includes 'Android' in its UA.
// We do this in 2 stages in order to exclude the following category 1 but
// include category 2.
//  1. Firefox on Android does not support WebP, and it has "Android" and
//     "Firefox" in the user agent.
//  2. Recent Opera support WebP, and some Opera have both "Opera" and
//     "Firefox" in the user agent.
const char* kLegacyWebpAllowlist[] = {
    "*Android *",    "*Firefox/66.*", "*Firefox/67.*",
    "*Firefox/68.*", "*Firefox/69.*", "*Firefox/70.*",
    "*Firefox/71.*",  // These Firefox versions are webp capable but don´t send webp header
};

// Based on https://github.com/apache/incubator-pagespeed-mod/issues/978,
// Desktop IE11 will start masquerading as Chrome soon, and according to
// https://groups.google.com/forum/?utm_medium=email&utm_source=footer#!msg/mod-pagespeed-discuss/HYzzdOzJu_k/ftdV8koVgUEJ
// a browser called Midori might (at some point) masquerade as Chrome as well.
const char* kLegacyWebpBlockedlist[] = {
    "*Android 0.*",  "*Android 1.*",  "*Android 2.*",  "*Android 3.*",
    "*Firefox/*",    "*Edge/*",       "*Trident/*",    "*Windows Phone*",
    "*Chrome/*",  // Genuine Chrome always sends Accept: webp.
    "*CriOS/*",   // Paranoia: we should not see Android and CriOS together.
    "*Firefox/?.*",  "*Firefox/1?.*", "*Firefox/2?.*", "*Firefox/3?.*",
    "*Firefox/4?.*", "*Firefox/5?.*", "*Firefox/60.*", "*Firefox/61.*",
    "*Firefox/62.*", "*Firefox/63.*",
    "*Firefox/64.*",  // Firefox versions not webp capables
};

// To determine lossless webp support and animated webp support, we must
// examine the UA.
const char* kWebpLosslessAlphaAllowlist[] = {
    "*Chrome/??.*", "*Chrome/???.*", "*CriOS/??.*",
    // User agent used only for internal testing.
    "webp-la", "webp-animated",
    "*Firefox/*",  // Do this way to permit Firefox webcapable to convert png
};

const char* kWebpLosslessAlphaBlockedlist[] = {
    "*Chrome/?.*",   "*Chrome/1?.*",  "*Chrome/20.*",  "*Chrome/21.*",
    "*Chrome/22.*",  "*CriOS/1?.*",   "*CriOS/20.*",   "*CriOS/21.*",
    "*CriOS/22.*",   "*CriOS/23.*",   "*CriOS/24.*",   "*CriOS/25.*",
    "*CriOS/26.*",   "*CriOS/27.*",   "*CriOS/28.*",   "*Firefox/?.*",
    "*Firefox/1?.*", "*Firefox/2?.*", "*Firefox/3?.*", "*Firefox/4?.*",
    "*Firefox/5?.*", "*Firefox/60.*", "*Firefox/61.*", "*Firefox/62.*",
    "*Firefox/63.*",
    "*Firefox/64.*",  // Black list Firefox not webp capable
};

// Animated WebP is supported by browsers based on Chromium v32+, including
// Chrome 32+ and Opera 19+. Because since version 15, Opera has been including
// "Chrome/VERSION" in the user agent string [1], the test for Chrome 32+ will
// also cover Opera 19+.
// [1] https://dev.opera.com/blog/opera-user-agent-strings-opera-15-and-beyond/
const char* kWebpAnimatedAllowlist[] = {
    "*Chrome/??.*",
    "*CriOS/??.*",
    "webp-animated",  // User agent for internal testing.
    "*Firefox/*",
};

const char* kWebpAnimatedBlockedlist[] = {
    "*Chrome/?.*",   "*Chrome/1?.*",  "*Chrome/2?.*",  "*Chrome/30.*",
    "*Chrome/31.*",  "*CriOS/?.*",    "*CriOS/1?.*",   "*CriOS/2?.*",
    "*CriOS/30.*",   "*CriOS/31.*",   "*Firefox/?.*",  "*Firefox/1?.*",
    "*Firefox/2?.*", "*Firefox/3?.*", "*Firefox/4?.*", "*Firefox/5?.*",
    "*Firefox/60.*", "*Firefox/61.*", "*Firefox/62.*", "*Firefox/63.*",
    "*Firefox/64.*",
};

const char* kInsertDnsPrefetchAllowlist[] = {
    "*Chrome/*",
    "*Firefox/*",
    "*Safari/*",
    // Plus IE, see code below.
    "*Wget*",
};

const char* kInsertDnsPrefetchBlockedlist[] = {
    "*Firefox/1.*",
    "*Firefox/2.*",
    "*Firefox/3.*",
    // Safari indicates version with a separate Version/N.N.N token that appears
    // somewhere before the Safari/ token.  This only started with version 3,
    // but versions before 3 are 10+ years old at this point and won't run on
    // any supported OS.
    "*Version/3.*Safari/*",
    "*Version/4.*Safari/*",
    // 5.0.1+ actually did support it, but that's long obsolete, so don't bother
    // contorting the list to include it.
    "*Version/5.*Safari/*",
    "*MSIE 5.*",
    "*MSIE 6.*",
    "*MSIE 7.*",
    "*MSIE 8.*",
};

// Allowlist used for doing the tablet-user-agent check, which also feeds
// into the device type used for storing properties in the property cache.
const char* kTabletUserAgentAllowlist[] = {
    "*Android*",  // Android tablet has "Android" but not "Mobile". Regexp
                  // checks for UserAgents should first check the mobile
                  // allowlists and blockedlists and only then check the tablet
                  // allowlist for correct results.
    "*iPad*", "*TouchPad*", "*Silk-Accelerated*", "*Kindle Fire*"};

// Allowlist used for doing the mobile-user-agent check, which also feeds
// into the device type used for storing properties in the property cache.
const char* kMobileUserAgentAllowlist[] = {
    "*Mozilla*Android*Mobile*",
    "*iPhone*",
    "*BlackBerry*",
    "*Opera Mobi*",
    "*Opera Mini*",
    "*SymbianOS*",
    "*UP.Browser*",
    "*J-PHONE*",
    "*Profile/MIDP*",
    "*profile/MIDP*",
    "*portalmmm*",
    "*DoCoMo*",
    "*Obigo*",
    "AdsBot-Google-Mobile",
};

// Blockedlist used for doing the mobile-user-agent check.
const char* kMobileUserAgentBlockedlist[] = {
    "*Mozilla*Android*Silk*Mobile*", "*Mozilla*Android*Kindle Fire*Mobile*"};

// Allowlist used for mobilization.
const char* kMobilizationUserAgentAllowlist[] = {
    "*Android*",  "*Chrome/*",     "*Firefox/*", "*iPad*", "*iPhone*",
    "*iPod*",     "*Opera*",       "*Safari*",   "*Wget*",
    "*CriOS/*",    // Chrome for iOS.
    "*Android *",  // Native Android browser (see blockedlist below).
    "*iPhone*",   "AdsBot-Google*"};

// Blockedlist used for doing the mobilization UA check.
const char* kMobilizationUserAgentBlockedlist[] = {
    "*Android 0.*", "*Android 1.*", "*Android 2.*", "*BlackBerry*",
    "*Mozilla*Android*Silk*Mobile*", "*Mozilla*Android*Kindle Fire*Mobile*",
    "*Opera Mobi*", "*Opera Mini*", "*SymbianOS*", "*UP.Browser*", "*J-PHONE*",
    "*Profile/MIDP*", "*profile/MIDP*", "*portalmmm*", "*DoCoMo*", "*Obigo*",
    // TODO(jmaessen): Remove when there's a fix for scroll misbehavior on
    // CriOS.
    "*CriOS/*",      // Chrome for iOS.
    "*GSA*Safari*",  // Google Search Application for iOS.
    // TODO(jmaessen): Remove when there's a fix for page geometry on the native
    // Android browser (the old WebKit browser).
    "*U; Android 3.*", "*U; Android 4.*"};

// IE 11 and later user agent strings are deliberately difficult.  That would be
// great if random pages never put the browser into backward compatibility mode,
// and all the outstanding caching bugs were fixed, but neither is true and so
// we need to be able to spot IE 11 and treat it as IE even though we're not
// supposed to need to do so ever again.  See
// http://blogs.msdn.com/b/ieinternals/archive/2013/09/21/internet-explorer-11-user-agent-string-ua-string-sniffing-compatibility-with-gecko-webkit.aspx
const char* kIeUserAgents[] = {
    "*MSIE *",                // Should match any IE before 11.
    "*rv:11.?) like Gecko*",  // Other revisions (eg 12.0) are FireFox
    "*IE 1*",                 // Initial numeral avoids Samsung UA
    "*Trident/7*",            // Opera sometimes pretends to be earlier Trident
};
const int kIEBefore11Index = 0;

// Match either 'CriOS' (iOS Chrome) or 'Chrome'. ':?' marks a non-capturing
// group.
const char* kChromeVersionPattern =
    "(?:Chrome|CriOS)/(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)";

// Device strings must not include wildcards.
struct Dimension {
  const char* device_name;
  int width;
  int height;
};

const Dimension kKnownScreenDimensions[] = {
    {"Galaxy Nexus", 720, 1280}, {"GT-I9300", 720, 1280},
    {"GT-N7100", 720, 1280},     {"Nexus 4", 768, 1280},
    {"Nexus 10", 1600, 2560},    {"Nexus S", 480, 800},
    {"Xoom", 800, 1280},         {"XT907", 540, 960},
};

}  // namespace

UserAgentMatcher::UserAgentMatcher()
    : chrome_version_pattern_(kChromeVersionPattern) {
  // Initialize FastWildcardGroup for image inlining allowlist & blockedlist.
  for (int i = 0, n = arraysize(kImageInliningAllowlist); i < n; ++i) {
    supports_image_inlining_.Allow(kImageInliningAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    supports_image_inlining_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kImageInliningBlockedlist); i < n; ++i) {
    supports_image_inlining_.Disallow(kImageInliningBlockedlist[i]);
  }
  for (int i = 0, n = arraysize(kLazyloadImagesBlockedlist); i < n; ++i) {
    supports_lazyload_images_.Disallow(kLazyloadImagesBlockedlist[i]);
  }
  defer_js_allowlist_.Allow(kIeUserAgents[kIEBefore11Index]);
  for (int i = 0, n = arraysize(kDeferJSAllowlist); i < n; ++i) {
    defer_js_allowlist_.Allow(kDeferJSAllowlist[i]);
  }

  // https://github.com/apache/incubator-pagespeed-mod/issues/982
  defer_js_allowlist_.Disallow("* MSIE 9.*");

  for (int i = 0, n = arraysize(kDeferJSBlockedlist); i < n; ++i) {
    defer_js_allowlist_.Disallow(kDeferJSBlockedlist[i]);
  }

  for (int i = 0, n = arraysize(kDeferJSMobileAllowlist); i < n; ++i) {
    defer_js_mobile_allowlist_.Allow(kDeferJSMobileAllowlist[i]);
  }

  // Do the same for webp support.
  for (int i = 0, n = arraysize(kLegacyWebpAllowlist); i < n; ++i) {
    legacy_webp_.Allow(kLegacyWebpAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kLegacyWebpBlockedlist); i < n; ++i) {
    legacy_webp_.Disallow(kLegacyWebpBlockedlist[i]);
  }

  for (int i = 0, n = arraysize(kWebpLosslessAlphaAllowlist); i < n; ++i) {
    supports_webp_lossless_alpha_.Allow(kWebpLosslessAlphaAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kWebpLosslessAlphaBlockedlist); i < n; ++i) {
    supports_webp_lossless_alpha_.Disallow(kWebpLosslessAlphaBlockedlist[i]);
  }
  for (int i = 0, n = arraysize(kWebpAnimatedAllowlist); i < n; ++i) {
    supports_webp_animated_.Allow(kWebpAnimatedAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kWebpAnimatedBlockedlist); i < n; ++i) {
    supports_webp_animated_.Disallow(kWebpAnimatedBlockedlist[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchAllowlist); i < n; ++i) {
    supports_dns_prefetch_.Allow(kInsertDnsPrefetchAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    supports_dns_prefetch_.Allow(kIeUserAgents[i]);
  }
  for (int i = 0, n = arraysize(kInsertDnsPrefetchBlockedlist); i < n; ++i) {
    supports_dns_prefetch_.Disallow(kInsertDnsPrefetchBlockedlist[i]);
  }

  for (int i = 0, n = arraysize(kMobileUserAgentAllowlist); i < n; ++i) {
    mobile_user_agents_.Allow(kMobileUserAgentAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kMobileUserAgentBlockedlist); i < n; ++i) {
    mobile_user_agents_.Disallow(kMobileUserAgentBlockedlist[i]);
  }
  for (int i = 0, n = arraysize(kTabletUserAgentAllowlist); i < n; ++i) {
    tablet_user_agents_.Allow(kTabletUserAgentAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kMobilizationUserAgentAllowlist); i < n; ++i) {
    mobilization_user_agents_.Allow(kMobilizationUserAgentAllowlist[i]);
  }
  for (int i = 0, n = arraysize(kMobilizationUserAgentBlockedlist); i < n;
       ++i) {
    mobilization_user_agents_.Disallow(kMobilizationUserAgentBlockedlist[i]);
  }
  for (int i = 0, n = arraysize(kIeUserAgents); i < n; ++i) {
    ie_user_agents_.Allow(kIeUserAgents[i]);
  }
  GoogleString known_devices_pattern_string = "(";
  for (int i = 0, n = arraysize(kKnownScreenDimensions); i < n; ++i) {
    const Dimension& dim = kKnownScreenDimensions[i];
    screen_dimensions_map_[dim.device_name] = make_pair(dim.width, dim.height);
    if (i != 0) {
      StrAppend(&known_devices_pattern_string, "|");
    }
    StrAppend(&known_devices_pattern_string, dim.device_name);
  }
  StrAppend(&known_devices_pattern_string, ")");
  known_devices_pattern_ =
      std::make_unique<re2::RE2>(known_devices_pattern_string);
}

UserAgentMatcher::~UserAgentMatcher() {}

bool UserAgentMatcher::IsIe(const StringPiece& user_agent) const {
  return ie_user_agents_.Match(user_agent, false);
}

bool UserAgentMatcher::IsIe9(const StringPiece& user_agent) const {
  return user_agent.find(" MSIE 9.") != GoogleString::npos;
}

bool UserAgentMatcher::SupportsImageInlining(
    const StringPiece& user_agent) const {
  if (user_agent.empty()) {
    return true;
  }
  return supports_image_inlining_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsLazyloadImages(StringPiece user_agent) const {
  return supports_lazyload_images_.Match(user_agent, true);
}

bool UserAgentMatcher::SupportsDnsPrefetch(
    const StringPiece& user_agent) const {
  return supports_dns_prefetch_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsJsDefer(const StringPiece& user_agent,
                                       bool allow_mobile) const {
  // TODO(ksimbili): Use IsMobileRequest?
  if (GetDeviceTypeForUA(user_agent) != kDesktop) {
    // TODO(ksimbili): IsMobileUserAgent returns true for tablets too.
    // Fix it when we need to differentiate them.
    return allow_mobile && defer_js_mobile_allowlist_.Match(user_agent, false);
  }
  return user_agent.empty() || defer_js_allowlist_.Match(user_agent, false);
}

bool UserAgentMatcher::LegacyWebp(const StringPiece& user_agent) const {
  return legacy_webp_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsWebpLosslessAlpha(
    const StringPiece& user_agent) const {
  return supports_webp_lossless_alpha_.Match(user_agent, false);
}

bool UserAgentMatcher::SupportsWebpAnimated(
    const StringPiece& user_agent) const {
  return supports_webp_animated_.Match(user_agent, false);
}

UserAgentMatcher::DeviceType UserAgentMatcher::GetDeviceTypeForUAAndHeaders(
    const StringPiece& user_agent,
    const RequestHeaders* request_headers) const {
  return GetDeviceTypeForUA(user_agent);
}

bool UserAgentMatcher::IsAndroidUserAgent(const StringPiece& user_agent) const {
  return user_agent.find("Android") != GoogleString::npos;
}

bool UserAgentMatcher::IsiOSUserAgent(const StringPiece& user_agent) const {
  return user_agent.find("iPhone") != GoogleString::npos ||
         user_agent.find("iPad") != GoogleString::npos;
}

bool UserAgentMatcher::GetChromeBuildNumber(const StringPiece& user_agent,
                                            int* major, int* minor, int* build,
                                            int* patch) const {
  return RE2::PartialMatch(StringPieceToRe2(user_agent),
                           chrome_version_pattern_, major, minor, build, patch);
}

bool UserAgentMatcher::SupportsDnsPrefetchUsingRelPrefetch(
    const StringPiece& user_agent) const {
  return IsIe9(user_agent);
}

// TODO(bharathbhushan): Make sure GetDeviceTypeForUA is called only once per
// http request.
UserAgentMatcher::DeviceType UserAgentMatcher::GetDeviceTypeForUA(
    const StringPiece& user_agent) const {
  if (mobile_user_agents_.Match(user_agent, false)) {
    return kMobile;
  }
  if (tablet_user_agents_.Match(user_agent, false)) {
    return kTablet;
  }
  return kDesktop;
}

StringPiece UserAgentMatcher::DeviceTypeString(DeviceType device_type) {
  StringPiece device_type_suffix = "";
  switch (device_type) {
    case kMobile:
      device_type_suffix = "mobile";
      break;
    case kTablet:
      device_type_suffix = "tablet";
      break;
    case kDesktop:
    case kEndOfDeviceType:
    default:
      device_type_suffix = "desktop";
      break;
  }
  return device_type_suffix;
}

StringPiece UserAgentMatcher::DeviceTypeSuffix(DeviceType device_type) {
  StringPiece device_type_suffix = "";
  switch (device_type) {
    case kMobile:
      device_type_suffix = "@Mobile";
      break;
    case kTablet:
      device_type_suffix = "@Tablet";
      break;
    case kDesktop:
    case kEndOfDeviceType:
    default:
      device_type_suffix = "@Desktop";
      break;
  }
  return device_type_suffix;
}

bool UserAgentMatcher::UserAgentExceedsChromeiOSBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // Verify if this is an iOS user agent.
  if (!IsiOSUserAgent(user_agent)) {
    return false;
  }
  return UserAgentExceedsChromeBuildAndPatch(user_agent, required_build,
                                             required_patch);
}

bool UserAgentMatcher::UserAgentExceedsChromeAndroidBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // Verify if this is an Android user agent.
  if (!IsAndroidUserAgent(user_agent)) {
    return false;
  }
  return UserAgentExceedsChromeBuildAndPatch(user_agent, required_build,
                                             required_patch);
}

bool UserAgentMatcher::UserAgentExceedsChromeBuildAndPatch(
    const StringPiece& user_agent, int required_build,
    int required_patch) const {
  // By default user agent sniffing is disabled.
  if (required_build == -1 && required_patch == -1) {
    return false;
  }
  int major = -1;
  int minor = -1;
  int parsed_build = -1;
  int parsed_patch = -1;
  if (!GetChromeBuildNumber(user_agent, &major, &minor, &parsed_build,
                            &parsed_patch)) {
    return false;
  }

  if (parsed_build < required_build) {
    return false;
  } else if (parsed_build == required_build && parsed_patch < required_patch) {
    return false;
  }

  return true;
}

bool UserAgentMatcher::SupportsMobilization(StringPiece user_agent) const {
  return mobilization_user_agents_.Match(user_agent, false);
}

}  // namespace net_instaweb
