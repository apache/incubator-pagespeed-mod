// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>

#include "net/instaweb/http/public/request_headers.h"
#include "net/instaweb/http/public/user_agent_matcher.h"
#include "net/instaweb/http/public/user_agent_matcher_test.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace UserAgentStrings {
const char kTestingWebp[] = "webp";
const char kTestingWebpLosslessAlpha[] = "webp-la";
}

class UserAgentMatcherTest : public testing::Test {
 protected:
  bool IsMobileUserAgent(const StringPiece& user_agent) {
    return user_agent_matcher_.GetDeviceTypeForUA(user_agent) ==
        UserAgentMatcher::kMobile;
  }

  bool IsDesktopUserAgent(const StringPiece& user_agent) {
    return user_agent_matcher_.GetDeviceTypeForUA(user_agent) ==
        UserAgentMatcher::kDesktop;
  }
  bool IsTabletUserAgent(const StringPiece& user_agent) {
    return user_agent_matcher_.GetDeviceTypeForUA(user_agent) ==
        UserAgentMatcher::kTablet;
  }

  UserAgentMatcher user_agent_matcher_;
};

TEST_F(UserAgentMatcherTest, IsIeTest) {
  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe6UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe7UserAgent));

  EXPECT_TRUE(user_agent_matcher_.IsIe(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe7(UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(UserAgentStrings::kIe8UserAgent));
}

TEST_F(UserAgentMatcherTest, IsNotIeTest) {
  EXPECT_FALSE(user_agent_matcher_.IsIe(UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6(UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe6or7(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsIe(UserAgentStrings::kChromeUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsImageInlining) {
  for (int i = 0;
       i < arraysize(UserAgentStrings::kImageInliningSupportedUserAgents);
       ++i) {
    EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
                    UserAgentStrings::kImageInliningSupportedUserAgents[i]))
        << "\"" << UserAgentStrings::kImageInliningSupportedUserAgents[i]
        << "\"" << " not detected as a user agent that supports image inlining";
  }
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining("random user agent"));
}

TEST_F(UserAgentMatcherTest, SupportsLazyloadImages) {
  EXPECT_TRUE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kBlackBerryOS6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kBlackBerryOS5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsLazyloadImages(
      UserAgentStrings::kGooglePlusUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsImageInlining) {
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kGooglePlusUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsImageInlining(
      UserAgentStrings::kAndroidChrome18UserAgent));
}

TEST_F(UserAgentMatcherTest, BlinkWhitelistForDesktop) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kFirefoxUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe9UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kChromeUserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkWhiteListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kSafariUserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, BlinkBlackListForDesktop) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe6UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kIe8UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kBlinkBlackListForDesktop,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kFirefox1UserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, DoesNotSupportBlink) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kOpera5UserAgent, &headers));
  EXPECT_EQ(UserAgentMatcher::kDoesNotSupportBlink,
            user_agent_matcher_.GetBlinkRequestType(
                UserAgentStrings::kPSPUserAgent, &headers));
}

TEST_F(UserAgentMatcherTest, PrefetchMechanism) {
  const RequestHeaders headers;
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_image_tag"));
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kChromeUserAgent));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkScriptTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIe9UserAgent));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkRelSubresource,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_link_rel_subresource"));
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kSafariUserAgent));
  EXPECT_EQ(UserAgentMatcher::kPrefetchLinkScriptTag,
            user_agent_matcher_.GetPrefetchMechanism(
                "prefetch_link_script_tag"));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                NULL));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(""));
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kAndroidChrome21UserAgent));
  EXPECT_EQ(UserAgentMatcher::kPrefetchNotSupported,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIPhoneUserAgent));
  EXPECT_EQ(UserAgentMatcher::kPrefetchImageTag,
            user_agent_matcher_.GetPrefetchMechanism(
                UserAgentStrings::kIPadUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsJsDefer) {
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe9UserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kChromeUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefoxUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kSafariUserAgent, false));
}

TEST_F(UserAgentMatcherTest, SupportsJsDeferAllowMobile) {
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kAndroidHCUserAgent, true));
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIPhone4Safari, true));
  // Desktop is also supported.
  EXPECT_TRUE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kChromeUserAgent, true));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDefer) {
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe6UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIe8UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kFirefox1UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kNokiaUserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kOpera5UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kPSPUserAgent, false));
  // Mobile is not supported too.
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kIPhone4Safari, false));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDeferAllowMobile) {
  EXPECT_FALSE(user_agent_matcher_.SupportsJsDefer(
      UserAgentStrings::kOperaMobi9, true));
}

TEST_F(UserAgentMatcherTest, SupportsWebp) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kTestingWebp));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kTestingWebpLosslessAlpha));

  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome12UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome18UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera1110UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebp) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kSafariUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebp(
      UserAgentStrings::kIPhoneChrome21UserAgent));
}

TEST_F(UserAgentMatcherTest, IsAndroidUserAgentTest) {
  EXPECT_TRUE(user_agent_matcher_.IsAndroidUserAgent(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsAndroidUserAgent(
      UserAgentStrings::kIe6UserAgent));
}

TEST_F(UserAgentMatcherTest, IsiOSUserAgentTest) {
  EXPECT_TRUE(user_agent_matcher_.IsiOSUserAgent(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsiOSUserAgent(
      UserAgentStrings::kIPadUserAgent));
  EXPECT_TRUE(user_agent_matcher_.IsiOSUserAgent(
      UserAgentStrings::kIPodSafari));
  EXPECT_TRUE(user_agent_matcher_.IsiOSUserAgent(
      UserAgentStrings::kIPhoneChrome21UserAgent));
  EXPECT_FALSE(user_agent_matcher_.IsiOSUserAgent(
      UserAgentStrings::kIe6UserAgent));
}

TEST_F(UserAgentMatcherTest, ChromeBuildNumberTest) {
  int major = -1;
  int minor = -1;
  int build = -1;
  int patch = -1;
  EXPECT_TRUE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kChrome9UserAgent, &major, &minor, &build, &patch));
  EXPECT_EQ(major, 9);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(build, 597);
  EXPECT_EQ(patch, 19);

  // On iOS it's "CriOS", not "Chrome".
  EXPECT_TRUE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kIPhoneChrome21UserAgent, &major, &minor, &build,
      &patch));
  EXPECT_EQ(major, 21);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(build, 1180);
  EXPECT_EQ(patch, 82);

  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kAndroidHCUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      UserAgentStrings::kChromeUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      "Chrome/10.0", &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_.GetChromeBuildNumber(
      "Chrome/10.0.1.", &major, &minor, &build, &patch));
}

TEST_F(UserAgentMatcherTest, ExceedsChromeBuildAndPatchTest) {
  EXPECT_TRUE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1000, 0));
  EXPECT_TRUE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1000, 999));
  EXPECT_TRUE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1180, 82));
  EXPECT_FALSE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1180, 83));
  EXPECT_FALSE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1181, 0));
  EXPECT_FALSE(user_agent_matcher_.UserAgentExceedsChromeBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1181, 83));

  EXPECT_TRUE(user_agent_matcher_.UserAgentExceedsChromeAndroidBuildAndPatch(
      UserAgentStrings::kAndroidChrome21UserAgent, 1000, 0));
  EXPECT_FALSE(user_agent_matcher_.UserAgentExceedsChromeAndroidBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1000, 0));

  EXPECT_TRUE(user_agent_matcher_.UserAgentExceedsChromeiOSBuildAndPatch(
      UserAgentStrings::kIPhoneChrome21UserAgent, 1000, 0));
  EXPECT_FALSE(user_agent_matcher_.UserAgentExceedsChromeiOSBuildAndPatch(
      UserAgentStrings::kAndroidChrome21UserAgent, 1000, 0));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetch) {
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kFirefox5UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportDnsPrefetch) {
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetch(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsWebpLosslessAlpha) {
  EXPECT_TRUE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kTestingWebpLosslessAlpha));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebpLosslessAlpha) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kTestingWebp));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kAndroidICSUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome12UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera1110UserAgent));

  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsWebpLosslessAlpha(
      UserAgentStrings::kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetchUsingRelPrefetch) {
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe8UserAgent));
  EXPECT_TRUE(user_agent_matcher_.SupportsDnsPrefetchUsingRelPrefetch(
      UserAgentStrings::kIe9UserAgent));
}

TEST_F(UserAgentMatcherTest, SplitHtmlRelated) {
  for (int i = 0;
       i < arraysize(UserAgentStrings::kSplitHtmlSupportedUserAgents);
       ++i) {
    EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
                    UserAgentStrings::kSplitHtmlSupportedUserAgents[i], false))
        << "\"" << UserAgentStrings::kSplitHtmlSupportedUserAgents[i]
        << "\"" << " not detected as a user agent that supports split-html";
  }
  // Allow-mobile case.
  EXPECT_TRUE(user_agent_matcher_.SupportsSplitHtml(
      UserAgentStrings::kAndroidChrome21UserAgent, true));
  for (int i = 0;
       i < arraysize(UserAgentStrings::kSplitHtmlUnSupportedUserAgents);
       ++i) {
    EXPECT_FALSE(user_agent_matcher_.SupportsSplitHtml(
                    UserAgentStrings::kSplitHtmlUnSupportedUserAgents[i],
                    false))
        << "\"" << UserAgentStrings::kSplitHtmlUnSupportedUserAgents[i] << "\""
        << " detected incorrectly as a user agent that supports split-html";
  }
}

TEST_F(UserAgentMatcherTest, IsMobileUserAgent) {
  for (int i = 0; i < arraysize(UserAgentStrings::kMobileUserAgents); ++i) {
    EXPECT_TRUE(IsMobileUserAgent(UserAgentStrings::kMobileUserAgents[i]))
        << "\"" << UserAgentStrings::kMobileUserAgents[i] << "\""
        << " not detected as mobile user agent.";
  }
}

TEST_F(UserAgentMatcherTest, IsDesktopUserAgent) {
  for (int i = 0; i < arraysize(UserAgentStrings::kDesktopUserAgents); ++i) {
    EXPECT_TRUE(IsDesktopUserAgent(UserAgentStrings::kDesktopUserAgents[i]))
        << "\"" << UserAgentStrings::kDesktopUserAgents[i] << "\""
        << " not detected as desktop user agent.";
  }
}

TEST_F(UserAgentMatcherTest, IsTabletUserAgent) {
  for (int i = 0; i < arraysize(UserAgentStrings::kTabletUserAgents); ++i) {
    EXPECT_TRUE(IsTabletUserAgent(UserAgentStrings::kTabletUserAgents[i]))
        << "\"" << UserAgentStrings::kTabletUserAgents[i] << "\""
        << " not detected as tablet user agent.";
  }
}

TEST_F(UserAgentMatcherTest, GetDeviceTypeForUA) {
  EXPECT_EQ(UserAgentMatcher::kDesktop, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kIe9UserAgent));
  EXPECT_EQ(UserAgentMatcher::kMobile, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kIPhone4Safari));
  EXPECT_EQ(UserAgentMatcher::kTablet, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kIPadTabletUserAgent));
  // Silk-Accelerated is recognized as a tablet UA, whereas Silk is treated as
  // a desktop UA.
  EXPECT_EQ(UserAgentMatcher::kDesktop, user_agent_matcher_.GetDeviceTypeForUA(
      UserAgentStrings::kSilkDesktopUserAgent));
  EXPECT_EQ(UserAgentMatcher::kDesktop, user_agent_matcher_.GetDeviceTypeForUA(
      NULL));
}

TEST_F(UserAgentMatcherTest, GetScreenResolution) {
  int width, height;

  // Unknown user agent.
  EXPECT_FALSE(user_agent_matcher_.GetScreenResolution(
      UserAgentStrings::kIPhoneChrome21UserAgent, &width, &height));

  // Galaxy Nexus, first in list
  EXPECT_TRUE(user_agent_matcher_.GetScreenResolution(
      UserAgentStrings::kAndroidICSUserAgent, &width, &height));
  EXPECT_EQ(720, width);
  EXPECT_EQ(1280, height);

  // Nexus S, middle of list.
  EXPECT_TRUE(user_agent_matcher_.GetScreenResolution(
      UserAgentStrings::kAndroidNexusSUserAgent, &width, &height));
  EXPECT_EQ(480, width);
  EXPECT_EQ(800, height);

  // XT907, last in list.
  EXPECT_TRUE(user_agent_matcher_.GetScreenResolution(
      UserAgentStrings::XT907UserAgent, &width, &height));
  EXPECT_EQ(540, width);
  EXPECT_EQ(960, height);
}

}  // namespace net_instaweb