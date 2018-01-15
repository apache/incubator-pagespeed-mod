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


#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

class UserAgentMatcherTest : public UserAgentMatcherTestBase {
};

TEST_F(UserAgentMatcherTest, IsIeTest) {
  EXPECT_TRUE(user_agent_matcher_->IsIe(kIe6UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsIe(kIe7UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsIe(kIe8UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsIe(kIe9UserAgent));
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    EXPECT_TRUE(user_agent_matcher_->IsIe(kIe11UserAgents[i]));
  }
  EXPECT_FALSE(user_agent_matcher_->IsIe(kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_->IsIe(kChromeUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsImageInlining) {
  VerifyImageInliningSupport();
}

TEST_F(UserAgentMatcherTest, SupportsLazyloadImages) {
  EXPECT_TRUE(user_agent_matcher_->SupportsLazyloadImages(
      kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsLazyloadImages(
      kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsLazyloadImages(
      kIPhoneUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsLazyloadImages(
      kBlackBerryOS6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsLazyloadImages(
      kBlackBerryOS5UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsLazyloadImages(
      kGooglePlusUserAgent));
}

TEST_F(UserAgentMatcherTest, NotSupportsImageInlining) {
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      kGooglePlusUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsImageInlining(
      kAndroidChrome18UserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsJsDefer) {
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kIe9UserAgent, false));
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
        kIe11UserAgents[i], false)) << i << ": " << kIe11UserAgents[i];
  }
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kChromeUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kFirefoxUserAgent, false));
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kSafariUserAgent, false));
}

TEST_F(UserAgentMatcherTest, SupportsJsDeferAllowMobile) {
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kAndroidHCUserAgent, true));
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kIPhone4Safari, true));
  // Desktop is also supported.
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(
      kChromeUserAgent, true));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDefer) {
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kIe6UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kIe8UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kFirefox1UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kFirefox3UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kNokiaUserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kOpera5UserAgent, false));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kPSPUserAgent, false));
  // Mobile is not supported too.
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kIPhone4Safari, false));
}

TEST_F(UserAgentMatcherTest, NotSupportsJsDeferAllowMobile) {
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(
      kOperaMobi9, true));
}

// Googlebot for mobile generally includes the UA for the mobile device
// being impersonated.
#define GOOGLEBOT_MOBILE \
  "(compatible; Googlebot-Mobile/2.1; +http://www.google.com/bot.html)"
TEST_F(UserAgentMatcherTest, MobileBotSupportsJsDefer) {
  // Reference: https://developers.google.com/webmasters/smartphone-sites/
  // detecting-user-agents
  const char kGoogleBotIphoneUA[] =
      "Mozilla/5.0 (iPhone; CPU iPhone OS 6_0 like Mac OS X) "
      "AppleWebKit/536.26 (KHTML, like Gecko) Version/6.0 Mobile/10A5376e "
      "Safari/8536.25 " GOOGLEBOT_MOBILE;
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGoogleBotIphoneUA, true));

  const char kGoogleBotAndroidUA[] =
      "Mozilla/5.0 (Linux; Android 4.3; Nexus 4 Build/JWR66Y) "
      "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/32.0.1666.0 Mobile "
      "Safari/537.36 " GOOGLEBOT_MOBILE;
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGoogleBotAndroidUA, true));

  // Feature-phones don't support JS Defer.
  const char kSamsungFeatureBot[] =
      "SAMSUNG-SGH-E250/1.0 Profile/MIDP-2.0 Configuration/CLDC-1.1 "
      "UP.Browser/6.2.3.3.c.1.101 (GUI) MMP/2.0 " GOOGLEBOT_MOBILE;
  const char kDoCoMoBot[] =
      "DoCoMo/2.0 N905i(c100;TB;W24H16) " GOOGLEBOT_MOBILE;
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(kSamsungFeatureBot, true));
  EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(kDoCoMoBot, true));
}

// Googlebot for desktop generally does not includes a specific browser UA.
#define GOOGLEBOT_DESKTOP \
  "(compatible; Googlebot/2.1; +http://www.google.com/bot.html)"
TEST_F(UserAgentMatcherTest, DesktopBotSupportsJsDefer) {
  // https://support.google.com/webmasters/answer/1061943?hl=en
  const char kGooglebotNormal[] =
      "Mozilla/5.0 " GOOGLEBOT_DESKTOP;
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotNormal, true));

  const char kGooglebotRare[] =
      "Googlebot/2.1 (+http://www.google.com/bot.html)";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotRare, true));

  const char kGooglebotNews[] = "Googlebot-News";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotNews, true));

  const char kGooglebotImage[] = "Googlebot-Image/1.0";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotImage, true));

  const char kGooglebotVideo[] = "Googlebot-Video/1.0";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotVideo, true));

  const char kMediaPartners[] = "Mediapartners-Google";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kMediaPartners, true));

  const char kGooglebotAdsBot[] = "Googlebot-AdsBot/1.0";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGooglebotAdsBot, true));

  // This UA was extrapolated from the snippet when google-searching for
  // "what's my user agent" from Firefox.
  const char kGoogleBotFirefoxUA[] =
      "Mozilla/5.0 " GOOGLEBOT_DESKTOP " Mozilla/5.0 (Windows NT 6.1; WOW64; "
      "rv:24.0) Gecko/20100101 Firefox/24.0";
  EXPECT_TRUE(user_agent_matcher_->SupportsJsDefer(kGoogleBotFirefoxUA, true));
}

TEST_F(UserAgentMatcherTest, WebpCapableLackingAcceptHeader) {
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(kTestingWebp));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kTestingWebpLosslessAlpha));

  EXPECT_TRUE(user_agent_matcher_->LegacyWebp(
      kAndroidICSUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kChrome12UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kOpera1110UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPadChrome29UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPadChrome36UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPhoneChrome36UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kOperaWithFirefoxUserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebp) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kFirefox42AndroidUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIe9UserAgent));
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
        kIe11UserAgents[i]));
  }
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kSafariUserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPhoneChrome21UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kIPadChrome28UserAgent));
  EXPECT_FALSE(user_agent_matcher_->LegacyWebp(
      kWindowsPhoneUserAgent));
}

TEST_F(UserAgentMatcherTest, IsAndroidUserAgentTest) {
  EXPECT_TRUE(user_agent_matcher_->IsAndroidUserAgent(
      kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_->IsAndroidUserAgent(
      kIe6UserAgent));
}

TEST_F(UserAgentMatcherTest, IsiOSUserAgentTest) {
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPhoneUserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPadUserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPodSafari));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPhoneChrome21UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPadChrome28UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPadChrome29UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPadChrome36UserAgent));
  EXPECT_TRUE(user_agent_matcher_->IsiOSUserAgent(
      kIPhoneChrome36UserAgent));
  EXPECT_FALSE(user_agent_matcher_->IsiOSUserAgent(
      kIe6UserAgent));
}

TEST_F(UserAgentMatcherTest, ChromeBuildNumberTest) {
  int major = -1;
  int minor = -1;
  int build = -1;
  int patch = -1;
  EXPECT_TRUE(user_agent_matcher_->GetChromeBuildNumber(
      kChrome9UserAgent, &major, &minor, &build, &patch));
  EXPECT_EQ(major, 9);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(build, 597);
  EXPECT_EQ(patch, 19);

  // On iOS it's "CriOS", not "Chrome".
  EXPECT_TRUE(user_agent_matcher_->GetChromeBuildNumber(
      kIPhoneChrome21UserAgent, &major, &minor, &build,
      &patch));
  EXPECT_EQ(major, 21);
  EXPECT_EQ(minor, 0);
  EXPECT_EQ(build, 1180);
  EXPECT_EQ(patch, 82);

  EXPECT_FALSE(user_agent_matcher_->GetChromeBuildNumber(
      kAndroidHCUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_->GetChromeBuildNumber(
      kChromeUserAgent, &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_->GetChromeBuildNumber(
      "Chrome/10.0", &major, &minor, &build, &patch));
  EXPECT_FALSE(user_agent_matcher_->GetChromeBuildNumber(
      "Chrome/10.0.1.", &major, &minor, &build, &patch));
}

TEST_F(UserAgentMatcherTest, ExceedsChromeBuildAndPatchTest) {
  EXPECT_TRUE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1000, 0));
  EXPECT_TRUE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1000, 999));
  EXPECT_TRUE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1180, 82));
  EXPECT_FALSE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1180, 83));
  EXPECT_FALSE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1181, 0));
  EXPECT_FALSE(user_agent_matcher_->UserAgentExceedsChromeBuildAndPatch(
      kIPhoneChrome21UserAgent, 1181, 83));

  EXPECT_TRUE(user_agent_matcher_->UserAgentExceedsChromeAndroidBuildAndPatch(
      kAndroidChrome21UserAgent, 1000, 0));
  EXPECT_FALSE(user_agent_matcher_->UserAgentExceedsChromeAndroidBuildAndPatch(
      kIPhoneChrome21UserAgent, 1000, 0));

  EXPECT_TRUE(user_agent_matcher_->UserAgentExceedsChromeiOSBuildAndPatch(
      kIPhoneChrome21UserAgent, 1000, 0));
  EXPECT_FALSE(user_agent_matcher_->UserAgentExceedsChromeiOSBuildAndPatch(
      kAndroidChrome21UserAgent, 1000, 0));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetch) {
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
      kChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
      kIe9UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
      kFirefox5UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
      kSafari6UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
      kSafari9UserAgent));
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetch(
        kIe11UserAgents[i]));
  }
}

TEST_F(UserAgentMatcherTest, DoesntSupportDnsPrefetch) {
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetch(
      kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetch(
      kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetch(
      kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetch(
      kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetch(
      kSafariUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsWebpLosslessAlpha) {
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kTestingWebpLosslessAlpha));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIPadChrome29UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIPadChrome36UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIPhoneChrome36UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kNexus10ChromeUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      XT907UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kPagespeedInsightsMobileUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kPagespeedInsightsDesktopUserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportWebpLosslessAlpha) {
  // The most interesting tests here are the recent but slightly older versions
  // of Chrome and Opera that can't display webp.
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kTestingWebp));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kAndroidICSUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kChrome12UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kOpera1110UserAgent));

  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kAndroidHCUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kChromeUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kChrome9UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kChrome15UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kOpera1101UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kFirefox1UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIe8UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIe9UserAgent));
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
        kIe11UserAgents[i]));
  }
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kNokiaUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kOpera5UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kOpera8UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kPSPUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kSafariUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kIPadChrome28UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpLosslessAlpha(
      kWindowsPhoneUserAgent));
}

TEST_F(UserAgentMatcherTest, SupportsDnsPrefetchUsingRelPrefetch) {
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetchUsingRelPrefetch(
      kIe6UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetchUsingRelPrefetch(
      kIe7UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsDnsPrefetchUsingRelPrefetch(
      kIe8UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsDnsPrefetchUsingRelPrefetch(
      kIe9UserAgent));
}

TEST_F(UserAgentMatcherTest, GetDeviceTypeForUA) {
  VerifyGetDeviceTypeForUA();
}

TEST_F(UserAgentMatcherTest, IE11NoDeferJs) {
  for (int i = 0; i < kIe11UserAgentsArraySize; ++i) {
    const char* user_agent = kIe11UserAgents[i];
    EXPECT_FALSE(user_agent_matcher_->SupportsJsDefer(user_agent, true));
  }
}

TEST_F(UserAgentMatcherTest, Mobilization) {
  VerifyMobilizationSupport();
}

TEST_F(UserAgentMatcherTest, SupportsAnimatedWebp) {
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpAnimated(
      kTestingWebpAnimated));

  EXPECT_TRUE(user_agent_matcher_->SupportsWebpAnimated(
      kChrome32UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpAnimated(
      kCriOS32UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpAnimated(
      kOpera19UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsWebpAnimated(
      kChrome37UserAgent));
}

TEST_F(UserAgentMatcherTest, DoesntSupportAnimatedWebp) {
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kChrome31UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kCriOS31UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kOpera18UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kOpera1110UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kAndroidICSUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kFirefoxUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kIe10UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kIPhone4Safari));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kWindowsPhoneUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kPagespeedInsightsMobileUserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsWebpAnimated(
      kPagespeedInsightsDesktopUserAgent));
}

}  // namespace net_instaweb
