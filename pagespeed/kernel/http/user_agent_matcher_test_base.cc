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


#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

const char UserAgentMatcherTestBase::kAcceptHeaderValueMobile[] =
    "text/html,application/vnd.wap.xhtml+xml";
const char UserAgentMatcherTestBase::kAcceptHeaderValueNonMobile[] =
    "text/html";
const char UserAgentMatcherTestBase::kALCATELMobileUserAgent[] =
    "ALCATEL_one_touch_310A/1.0 Profile/MIDP-2.0 "
    "Configuration/CLDC-1.1 ObigoInternetBrowser/Q03C";
const char UserAgentMatcherTestBase::kAlcatelUserAgent[] =
    "Alcatel_one_touch_214/1.0 ObigoInternetBrowser/Q03C";
const char UserAgentMatcherTestBase::kAmoiUserAgent[] =
    "Amoi 8512/R18.0 NF-Browser/3.3";
const char UserAgentMatcherTestBase::kAndroidChrome18UserAgent[] =
    // webp broken
    "Mozilla/5.0 (Linux; Android 4.0.4; Galaxy Nexus Build/IMM76B) "
    "AppleWebKit/535.19 (KHTML, like Gecko) Chrome/18.0.1025.133 Mobile "
    "Safari/535.19";
const char UserAgentMatcherTestBase::kAndroidChrome21UserAgent[] =
    // webp fixed (string is a hack)
    "Mozilla/5.0 (Linux; Android 4.1.4; Galaxy Nexus Build/IMM76B) "
    "AppleWebKit/535.19 (KHTML, like Gecko) Chrome/21.0.1025.133 Mobile "
    "Safari/535.19";
const char UserAgentMatcherTestBase::kAndroidHCUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 3.2; en-us; Sony Tablet S Build/THMAS11000)"
    " AppleWebKit/534.13 (KHTML, like Gecko) Version/4.0 Safari/534.13";
const char UserAgentMatcherTestBase::kAndroidICSUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 4.0.1; en-us; Galaxy Nexus Build/ICL27) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30";
const char UserAgentMatcherTestBase::kPagespeedInsightsMobileUserAgent[] =
    "Mozilla/5.0 (Linux; Android 6.0.1; Nexus 5X Build/MMB29P) "
    "AppleWebKit/537.36 (KHTML, like Gecko; Google Page Speed Insights) "
    "Chrome/27.0.1453 Mobile Safari/537.36";
const char UserAgentMatcherTestBase::kPagespeedInsightsDesktopUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko; "
    "Google Page Speed Insights) Chrome/27.0.1453 Safari/537.36";
const char UserAgentMatcherTestBase::kAndroidNexusSUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 2.3.3; en-gb; Nexus S Build/GRI20)"
    "AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1";
const char UserAgentMatcherTestBase::kBenqUserAgent[] =
    "BENQ-A500";
const char UserAgentMatcherTestBase::kBlackBerryOS5UserAgent[] =
    "BlackBerry9000/5.0.0.93 Profile/MIDP-2.0 Configuration/CLDC-1.1 "
    "VendorID/179";
const char UserAgentMatcherTestBase::kBlackBerryOS6UserAgent[] =
    "Mozilla/5.0 (BlackBerry; U; BlackBerry 9800; en-US) AppleWebKit/534.11+ "
    "(KHTML, like Gecko) Version/6.0.0.141 Mobile Safari/534.11+";
const char UserAgentMatcherTestBase::kChrome12UserAgent[] =  // webp capable
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_4) "
    "AppleWebKit/534.30 (KHTML, like Gecko) Chrome/12.0.742.100 Safari/534.30";
const char UserAgentMatcherTestBase::kChrome15UserAgent[] =  // Not webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/15.0.597.19 Safari/534.13";
const char UserAgentMatcherTestBase::kChrome18UserAgent[] =  // webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/18.0.597.19 Safari/534.13";
const char UserAgentMatcherTestBase::kChrome9UserAgent[] =  // Not webp capable
    "Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) "
    "AppleWebKit/534.13 (KHTML, like Gecko) Chrome/9.0.597.19 Safari/534.13";
const char UserAgentMatcherTestBase::kChrome31UserAgent[] =  // No animated WebP
    "Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/31.0.1650.16 Safari/537.36";
const char UserAgentMatcherTestBase::kChrome32UserAgent[] =  // Animated WebP
    "Mozilla/5.0 (Windows NT 6.2; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1667.0 Safari/537.36";
const char UserAgentMatcherTestBase::kChrome37UserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_4) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2062.122 Safari/537.36";
const char UserAgentMatcherTestBase::kChrome41UserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)"
    " Chrome/41.0.2227.0 Safari/537.36";
const char UserAgentMatcherTestBase::kChrome42UserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)"
    " Chrome/42.0.2302.4 Safari/537.36";
const char UserAgentMatcherTestBase::kChromeUserAgent[] =
    "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US) "
    "AppleWebKit/525.13 (KHTML, like Gecko) Chrome/0.A.B.C Safari/525.13";
const char UserAgentMatcherTestBase::kCompalUserAgent[] =
    "Compal-A618";
const char UserAgentMatcherTestBase::kCriOS31UserAgent[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 7_0_3 like Mac OS X) "
    "AppleWebKit/537.51.1 (KHTML, like Gecko) CriOS/31.0.1650.18 Mobile/11B511 "
    "Safari/8536.25 (37014222-1F1D-4117-89B3-699D5C10628A)";
const char UserAgentMatcherTestBase::kCriOS32UserAgent[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 7_0_4 like Mac OS X) "
    "AppleWebKit/537.51.1 (KHTML, like Gecko) CriOS/32.0.1700.21 "
    "Mobile/11B554a Safari/9537.53 (A7BED55D-B09E-484F-B2FE-5E4952E9B87E)";
const char UserAgentMatcherTestBase::kCriOS48UserAgent[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 9_2 like Mac OS X) "
    "AppleWebKit/601.1 (KHTML, like Gecko) CriOS/48.0.2564.87 "
    "Mobile/13C75 Safari/601.1.46";
const char UserAgentMatcherTestBase::kDoCoMoMobileUserAgent[] =
    "DoCoMo/1.0/D505iS/c20/TB/W20H10";
const char UserAgentMatcherTestBase::kFirefox1UserAgent[] =
    "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.0.7) "
    "Gecko/20060909 Firefox/1.5.0.7 MG (Novarra-Vision/6.1)";
const char UserAgentMatcherTestBase::kFirefox3UserAgent[] =
    "Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
    "Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10";
const char UserAgentMatcherTestBase::kFirefox42AndroidUserAgent[] =
    "Mozilla/5.0 (Android 5.0.1; Mobile; rv:42.0) Gecko/42.0 Firefox/42.0";
const char UserAgentMatcherTestBase::kFirefox5UserAgent[] =
    "Mozilla/5.0 (X11; U; Linux i586; de; rv:5.0) Gecko/20100101 Firefox/5.0";
const char UserAgentMatcherTestBase::kFirefoxMobileUserAgent[] =
    "Mozilla/5.0 (Android; Mobile; rv:38.0) Gecko/38.0 Firefox/38.0";
const char UserAgentMatcherTestBase::kFirefoxNokiaN800[] =
    // This is a tablet.
    "Mozilla/5.0 (X11; U; Linux armv6l; en-US; rv:1.9a6pre) Gecko/20070810 "
    "Firefox/3.0a1 Tablet browser 0.1.16 RX-34_2007SE_4.2007.38-2";
const char UserAgentMatcherTestBase::kFirefoxUserAgent[] =
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:27.0) "
    "Gecko/20100101 Firefox/27.0";
const char UserAgentMatcherTestBase::kFLYUserAgent[] =
    "FLY-2040i/BSI AU.Browser/2.0 QO3C1 MMP/1.0";
const char UserAgentMatcherTestBase::kGenericAndroidUserAgent[] =
    "Android";
const char UserAgentMatcherTestBase::kGooglebotUserAgent[] =
    "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)";
// Google ads bot user agents, used to check ads policy compliance.
// See https://support.google.com/adwords/answer/2404197?hl=en#exclude
const char UserAgentMatcherTestBase::kGoogleAdsBotUserAgent[] =
    "AdsBot-Google";
const char UserAgentMatcherTestBase::kGoogleAdsBotMobileUserAgent[] =
    "AdsBot-Google-Mobile";
const char UserAgentMatcherTestBase::kGooglePlusUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.1; rv:6.0) Gecko/20110814 Firefox/6.0 Google "
    "(+https://developers.google.com/+/web/snippet/)";
const char UserAgentMatcherTestBase::kIe6UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 6.0; Windows NT 5.1; SV1;"
    " .NET CLR 2.0.50727)";
const char UserAgentMatcherTestBase::kIe7UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 7.0; Windows NT 6.0; en-US)";
const char UserAgentMatcherTestBase::kIe8UserAgent[] =
    "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64;"
    " Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729;"
    " .NET CLR 3.0.30729; Media Center PC 6.0; InfoPath.2;"
    " .NET4.0C; .NET4.0E; FDM)";
const char UserAgentMatcherTestBase::kIe9UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 9.0; WIndows NT 9.0; en-US))";
const char UserAgentMatcherTestBase::kIe10UserAgent[] =
    "Mozilla/5.0 (Windows; U; MSIE 10.0; WIndows NT 9.0; en-US))";
const char UserAgentMatcherTestBase::kIPadTabletUserAgent[] =
    "Mozilla/5.0 (iPad; CPU OS 6_1_3 like Mac OS X) AppleWebKit/536.26 (KHTML, "
    "like Gecko) Version/6.0 Mobile/10B329 Safari/8536.25";
const char UserAgentMatcherTestBase::kIPadUserAgent[] =
    "Mozilla/5.0 (iPad; U; CPU OS 3_2 like Mac OS X; en-us) "
    "AppleWebKit/531.21.10 (KHTML, like Gecko) Version/4.0.4 "
    "Mobile/7B334b Safari/531.21.10";
const char UserAgentMatcherTestBase::kIPadChrome28UserAgent[] =
    "Mozilla/5.0 (iPad; CPU OS 613 like Mac OS X) AppleWebKit/536.26 (KHTML "
    "like Gecko) CriOS/28.0.1500.12 Mobile/10B329 Safari/8536.25";
const char UserAgentMatcherTestBase::kIPadChrome29UserAgent[] =
    "Mozilla/5.0 (iPad; CPU OS 613 like Mac OS X) AppleWebKit/536.26 (KHTML "
    "like Gecko) CriOS/29.0.1547.11 Mobile/10B329 Safari/8536.25";
const char UserAgentMatcherTestBase::kIPadChrome36UserAgent[] =
    "Mozilla/5.0 (iPad; CPU OS 7_1_1 like Mac OS X) AppleWebKit/537.51.2 "
    "(KHTML, like Gecko) CriOS/36.0.1985.49 Mobile/11D201 Safari/9537.53";
const char UserAgentMatcherTestBase::kIPhone4Safari[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 5_0_1 like Mac OS X) AppleWebKit/534.46"
    " (KHTML, like Gecko) Version/5.1 Mobile/9A405 Safari/7534.48.3";
const char UserAgentMatcherTestBase::kIPhoneChrome21UserAgent[] =
    // no webp on iOS
    "Mozilla/5.0 (iPhone; CPU iPhone OS 6_0_1 like Mac OS X; en-us) "
    "AppleWebKit/534.46.0 (KHTML, like Gecko) CriOS/21.0.1180.82 "
    "Mobile/10A523 Safari/7534.48.3";
const char UserAgentMatcherTestBase::kIPhoneChrome36UserAgent[] =
    "Mozilla/5.0 (iPhone; CPU iPhone OS 7_1_2 like Mac OS X) "
    "AppleWebKit/537.51.2 (KHTML, like Gecko) CriOS/36.0.1985.49 "
    "Mobile/11D257 Safari/9537.53";
const char UserAgentMatcherTestBase::kIPhoneUserAgent[] =
    "Apple iPhone OS v2.1.1 CoreMedia v1.0.0.5F138";
const char UserAgentMatcherTestBase::kIPodSafari[] =
    "Mozilla/5.0 (iPod; U; CPU iPhone OS 4_3_3 like Mac OS X; en-us)"
    " AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8J2"
    " Safari/6533.18.5";
const char UserAgentMatcherTestBase::kiUserAgent[] =
    "i-mobile318";
const char UserAgentMatcherTestBase::kJMobileUserAgent[] =
    "J-PHONE/3.0/J-SA05";
const char UserAgentMatcherTestBase::kKDDIMobileUserAgent[] =
    "KDDI-CA31 UP.Browser/6.2.0.7.3.129 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kKindleTabletUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; Kindle Fire "
    "Build/GINGERBREAD) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 "
    "Mobile Safari/533.1";
const char UserAgentMatcherTestBase::kKWCMobileUserAgent[] =
    "KWC-E2000/1003 UP.Browser/7.2.6.1.475 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kLENOVOUserAgent[] =
    "LENOVO-E307_ENG_RUS_FLY/(2006.05.10)S276/WAP1.2.1";
const char UserAgentMatcherTestBase::kLGEMobileUserAgent[] =
    "LGE-AX300/1.0 UP.Browser/6.2.3.8 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kLGEUserAgent[] =
    "LGE-CU8188";
const char UserAgentMatcherTestBase::kLGMIDPMobileUserAgent[] =
    "LG-A225/V100 Obigo/WAP2.0 Profile/MIDP-2.1 Configuration/CLDC-1.1";
const char UserAgentMatcherTestBase::kLGUPBrowserMobileUserAgent[] =
    "LG8500/1.0 UP.Browser/6.2.3.9 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kLGUserAgent[] =
    "LG-B2000";
const char UserAgentMatcherTestBase::kMOTMobileUserAgent[] =
    "MOT-1.2.0/11.03 UP.Browser/4.1.27a";
const char UserAgentMatcherTestBase::kMozillaMobileUserAgent[] =
    "Mozilla/4.0 (compatible; MSIE 4.01; Windows CE; Smartphone; "
    "SDA/2.0 Profile/MIDP-2.0 Configuration/CLDC-1.1)";
const char UserAgentMatcherTestBase::kMozillaUserAgent[] =
    "Mozilla/1.22 (compatible; MMEF20; Cellphone; Sony CMD-Z5)";
const char UserAgentMatcherTestBase::kNECUserAgent[] =
    "NEC-E122/1.0 TMT-Mobile-Internet-Browser/1.1.14.20 (GUI)";
const char UserAgentMatcherTestBase::kNexus10ChromeUserAgent[] =
    "Mozilla/5.0 (Linux; Android 4.2.2; Nexus 10 Build/JDQ39) AppleWebKit/"
    "537.31 (KHTML, like Gecko) Chrome/26.0.1408.0 Safari/537.31";
const char UserAgentMatcherTestBase::kNexus6Chrome44UserAgent[] =
    "Mozilla/5.0 (Linux; Android 5.1.1; Nexus 6 Build/LVY48E) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.133 Mobile "
    "Safari/537.36";
const char UserAgentMatcherTestBase::kNexus7ChromeUserAgent[] =
    "Mozilla/5.0 (Linux; Android 4.2; Nexus 7 Build/JOP32C) AppleWebKit/535.19"
    "(KHTML, like Gecko) Chrome/18.0.1025.166 Safari/535.19";
const char UserAgentMatcherTestBase::kNokiaMobileUserAgent[] =
    "Nokia6600/1.0 (2.33.0) SymbianOS/7.0s Series60/2.0 "
    "Profile/MIDP-2.0 Configuration/CLDC-1.0";
const char UserAgentMatcherTestBase::kNokiaUserAgent[] =
    "Nokia2355/1.0 (JN100V0200.nep) UP.Browser/6.2.2.1.c.1.108 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kOpera1101UserAgent[] =
    // Not webp capable
    "Opera/9.80 (Windows NT 5.2; U; ru) Presto/2.7.62 Version/11.01";
const char UserAgentMatcherTestBase::kOpera1110UserAgent[] =  // webp capable
    "Opera/9.80 (Windows NT 6.0; U; en) Presto/2.8.99 Version/11.10";
const char UserAgentMatcherTestBase::kOpera5UserAgent[] =
    "Opera/5.0 (SunOS 5.8 sun4u; U) [en]";
const char UserAgentMatcherTestBase::kOpera8UserAgent[] =
    "Opera/8.01 (J2ME/MIDP; Opera Mini/1.1.2666/1724; en; U; ssr)";
const char UserAgentMatcherTestBase::kOpera18UserAgent[] =
    "Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/31.0.1650.63 Safari/537.36 OPR/18.0.1284.68";
const char UserAgentMatcherTestBase::kOpera19UserAgent[] =
    "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1700.76 Safari/537.36 OPR/19.0.1326.56";
const char UserAgentMatcherTestBase::kOperaMiniMobileUserAgent[] =
    "Opera/8.01 (J2ME/MIDP; Opera Mini/1.1.4821/hifi/tmobile/uk; "
    "Motorola V3; en; U; ssr)";
const char UserAgentMatcherTestBase::kOperaMobi9[] =
    "Opera/9.51 Beta (Microsoft Windows; PPC; Opera Mobi/1718; U; en)";
const char UserAgentMatcherTestBase::kOperaMobilMobileUserAgent[] =
    "Opera/9.80 (Android 4.0.4; Linux; Opera Mobi/ADR-1104201100; U; ru) "
    "Presto/2.7.81 Version/11.00";
const char UserAgentMatcherTestBase::kOperaWithFirefoxUserAgent[] =
    "Mozilla/5.0 (Windows NT 6.0; rv:2.0) Gecko/20100101 Firefox/4.0 Opera "
    "12.14";
const char UserAgentMatcherTestBase::kPanasonicMobileUserAgent[] =
    "Panasonic-G60/1.0 UP.Browser/6.1.0.6 "
    "MMP/1.0 UP.Browser/6.1.0.6 (GUI) MMP/1.0";
const char UserAgentMatcherTestBase::kPGUserAgent[] =
    "PG-1610/R01";
const char UserAgentMatcherTestBase::kPHILIPSUserAgent[] =
    "PHILIPS 330 / Obigo Internet Browser 2.0";
const char UserAgentMatcherTestBase::kportalmmmMobileUserAgent[] =
    "portalmmm/2.0 6120(c100;TB)";
const char UserAgentMatcherTestBase::kPSPUserAgent[] =
    "Mozilla/4.0 (PSP (PlayStation Portable); 2.00)";
const char UserAgentMatcherTestBase::kRoverUserAgent[] =
    "Rover 3.5 (RIM Handheld; Mobitex; OS v. 2.1)";
const char UserAgentMatcherTestBase::kSafariUserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_6_8) AppleWebKit/534.51.22 "
    "(KHTML, like Gecko) Version/5.1.1 Safari/534.51.22";
const char UserAgentMatcherTestBase::kSafari6UserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_7_4) AppleWebKit/536.25 "
    "(KHTML, like Gecko) Version/6.0 Safari/536.25";
const char UserAgentMatcherTestBase::kSafari9UserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_11_5) AppleWebKit/601.6.17 "
    "(KHTML, like Gecko) Version/9.1.1 Safari/601.6.17";
const char UserAgentMatcherTestBase::kSAGEMMobileUserAgent[] =
    "SAGEM-my202C/Orange1.0 UP.Browser/5.0.5.6 (GUI)";
const char UserAgentMatcherTestBase::kSAGEMUserAgent[] =
    "SAGEM-942";
const char UserAgentMatcherTestBase::kSAMSUNGMobileUserAgent[] =
    "SAMSUNG-B2700/SXIB1 SHP/VPP/R5 NetFront/3.4 SMM-MMS/1.2.0 "
    "profile/MIDP-2.0 configuration/CLDC-1.1";
const char UserAgentMatcherTestBase::kSCHMobileUserAgent[] =
    "SCH-A850 UP.Browser/6.2.3.2 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kSCHUserAgent[] =
    "SCH-U350/1.0 NetFront/3.0.22.2.18 (GUI) MMP/2.0";
const char UserAgentMatcherTestBase::kSECMobileUserAgent[] =
    "SEC-scha310 UP.Browser/4.1.26c3";
const char UserAgentMatcherTestBase::kSGHUserAgent[] =
    "SGH-Z230";
const char UserAgentMatcherTestBase::kSHARPMobileUserAgent[] =
    "SHARP-TQ-GX10/0.0 Profile/MIDP-1.0 Configuration/CLDC-1.0 "
    "UP.Browser/6.1.0.2.129 (GUI) MMP/1.0";
const char UserAgentMatcherTestBase::kSHARPUserAgent[] =
    "SHARP-TQ-GX15";
const char UserAgentMatcherTestBase::kSIEMobileUserAgent[] =
    "SIE-2128/24 UP.Browser/5.0.3.3 (GUI)";
const char UserAgentMatcherTestBase::kSIEUserAgent[] =
    "SIE-A40";
const char UserAgentMatcherTestBase::kSilkDesktopUserAgent[] =
    "Mozilla/5.0 (PlayStation Vita 2.10) AppleWebKit/536.26 (KHTML, "
    "like Gecko) Silk/3.2";
const char UserAgentMatcherTestBase::kSilkTabletUserAgent[] =
    "Mozilla/5.0 (Linux; U; Android 2.3.4; en-us; Silk/1.0.22.153_10033210) "
    "AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1 "
    "Silk-Accelerated=true";
const char UserAgentMatcherTestBase::kSoftBankMobileUserAgent[] =
    "SoftBank/1.0/705NK/NKJ001 Series60/3.0 NokiaN73/3.0650 "
    "Profile/MIDP-2.0 Configuration/CLDC-1.1";
const char UserAgentMatcherTestBase::kSpiceUserAgent[] =
    "Spice M6800  Opera/9.80 (MTK; Nucleus; U; en-US) Presto/2.4.18 "
    "Version/10.00";
const char UserAgentMatcherTestBase::kTIANYUUserAgent[] =
    "TIANYU-KTOUCH/B2012";
const char UserAgentMatcherTestBase::kVodafoneMobileUserAgent[] =
    "Vodafone/1.0/0Vodafone710/B616 Browser/Obigo-Browser/Q04A "
    "MMS/Obigo-MMS/Q04A SyncML/HW-SyncML/1.0 Java/QVM/4.1 Profile/MIDP-2.0 "
    "Configuration/CLDC-1.1";
// http://msdn.microsoft.com/en-us/library/ie/hh869301(v=vs.85).aspx
const char UserAgentMatcherTestBase::kWindowsPhoneUserAgent[] =
    "Mozilla/5.0 (Mobile; Windows Phone 8.1; Android 4.0; ARM; Trident/7.0; "
    "Touch; rv:11.0; IEMobile/11.0; NOKIA; Lumia 928) like iPhone OS 7_0_3 "
    "Mac OS X AppleWebKit/537 (KHTML, like Gecko) Mobile Safari/537";
const char UserAgentMatcherTestBase::kWinWAPUserAgent[] =
    "WinWAP/1.3 (1.3.0.0;WinCE;PPC2003)";
const char UserAgentMatcherTestBase::kXWapProfileHeaderValue[] =
    "http://foo.bar.xml";
const char UserAgentMatcherTestBase::kXWapProfile[] =
    "x-wap-profile";
const char UserAgentMatcherTestBase::kYourWapUserAgent[] =
    "YourWap Ericsson 380/2.63";
const char UserAgentMatcherTestBase::kZTEMobileUserAgent[] =
    "ZTE-C705/1.0 UP.Browser/4.1.27a2";
const char UserAgentMatcherTestBase::XT907UserAgent[] =
    "Mozilla/5.0 (Linux; Android 4.1.1; XT907 Build/9.8.1Q_27-2) AppleWebKit"
    "/537.25 (KHTML, like Gecko) Chrome/26.0.1376.1 Mobile Safari/537.25";
const char UserAgentMatcherTestBase::kTestingWebp[] =
    "webp";
const char UserAgentMatcherTestBase::kTestingWebpLosslessAlpha[] =
    "webp-la";
const char UserAgentMatcherTestBase::kTestingWebpAnimated[] =
    "webp-animated";

// The user-agent strings for IE 11 seen below were all actual UA strings
// observed in the real world.  Yes, the asterisks are actually in the UA.
const char* const
UserAgentMatcherTestBase::kIe11UserAgents[] = {
  "Windows-RSS-Platform/2.0 (IE 11.0; Windows NT 6.1)",
  "Mozilla/5.0 (compatible; IE 11.0; Win32; Trident/7.0)",
  "Mozilla/5.0 (Windows NT 6.1; WOW64; ***********; rv:11.0) like Gecko",
  "Mozilla/5.0 (Windows NT 6.1; Win64; x64; Trident/7.0; rv:11.0) like Gecko",
};

const char* const UserAgentMatcherTestBase::kMobileUserAgents[] = {
  UserAgentMatcherTestBase::kALCATELMobileUserAgent,
  UserAgentMatcherTestBase::kAlcatelUserAgent,
  UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
  UserAgentMatcherTestBase::kAndroidICSUserAgent,
  UserAgentMatcherTestBase::kAndroidICSUserAgent,
  UserAgentMatcherTestBase::kAndroidNexusSUserAgent,
  UserAgentMatcherTestBase::kDoCoMoMobileUserAgent,
  UserAgentMatcherTestBase::kIPhone4Safari,
  UserAgentMatcherTestBase::kIPhoneChrome21UserAgent,
  UserAgentMatcherTestBase::kIPhoneUserAgent,
  UserAgentMatcherTestBase::kIPodSafari,
  UserAgentMatcherTestBase::kJMobileUserAgent,
  UserAgentMatcherTestBase::kKDDIMobileUserAgent,
  UserAgentMatcherTestBase::kKWCMobileUserAgent,
  UserAgentMatcherTestBase::kLGEMobileUserAgent,
  UserAgentMatcherTestBase::kLGMIDPMobileUserAgent,
  UserAgentMatcherTestBase::kLGUPBrowserMobileUserAgent,
  UserAgentMatcherTestBase::kMOTMobileUserAgent,
  UserAgentMatcherTestBase::kMozillaMobileUserAgent,
  UserAgentMatcherTestBase::kNokiaMobileUserAgent,
  UserAgentMatcherTestBase::kOperaMiniMobileUserAgent,
  UserAgentMatcherTestBase::kOperaMobi9,
  UserAgentMatcherTestBase::kOperaMobilMobileUserAgent,
  UserAgentMatcherTestBase::kPanasonicMobileUserAgent,
  UserAgentMatcherTestBase::kPHILIPSUserAgent,
  UserAgentMatcherTestBase::kportalmmmMobileUserAgent,
  UserAgentMatcherTestBase::kSAGEMMobileUserAgent,
  UserAgentMatcherTestBase::kSAMSUNGMobileUserAgent,
  UserAgentMatcherTestBase::kSCHMobileUserAgent,
  UserAgentMatcherTestBase::kSECMobileUserAgent,
  UserAgentMatcherTestBase::kSHARPMobileUserAgent,
  UserAgentMatcherTestBase::kSIEMobileUserAgent,
  UserAgentMatcherTestBase::kSoftBankMobileUserAgent,
  UserAgentMatcherTestBase::kVodafoneMobileUserAgent,
  UserAgentMatcherTestBase::kZTEMobileUserAgent,
  UserAgentMatcherTestBase::kGoogleAdsBotMobileUserAgent
};

const char* const UserAgentMatcherTestBase::kDesktopUserAgents[] = {
  "not a mobile",
  UserAgentMatcherTestBase::kAmoiUserAgent,
  UserAgentMatcherTestBase::kBenqUserAgent,
  UserAgentMatcherTestBase::kCompalUserAgent,
  UserAgentMatcherTestBase::kFirefoxNokiaN800,
  UserAgentMatcherTestBase::kFLYUserAgent,
  UserAgentMatcherTestBase::kiUserAgent,
  UserAgentMatcherTestBase::kLENOVOUserAgent,
  UserAgentMatcherTestBase::kLGEUserAgent,
  UserAgentMatcherTestBase::kLGUserAgent,
  UserAgentMatcherTestBase::kMozillaUserAgent,
  UserAgentMatcherTestBase::kNECUserAgent,
  UserAgentMatcherTestBase::kOpera1101UserAgent,
  UserAgentMatcherTestBase::kPGUserAgent,
  UserAgentMatcherTestBase::kRoverUserAgent,
  UserAgentMatcherTestBase::kSafariUserAgent,
  UserAgentMatcherTestBase::kSAGEMUserAgent,
  UserAgentMatcherTestBase::kSCHUserAgent,
  UserAgentMatcherTestBase::kSGHUserAgent,
  UserAgentMatcherTestBase::kSHARPUserAgent,
  UserAgentMatcherTestBase::kSIEUserAgent,
  UserAgentMatcherTestBase::kSpiceUserAgent,
  UserAgentMatcherTestBase::kTIANYUUserAgent,
  UserAgentMatcherTestBase::kWinWAPUserAgent,
  UserAgentMatcherTestBase::kYourWapUserAgent,
  UserAgentMatcherTestBase::kGoogleAdsBotUserAgent
};

const char* const UserAgentMatcherTestBase::kTabletUserAgents[] = {
  UserAgentMatcherTestBase::kGenericAndroidUserAgent,
  UserAgentMatcherTestBase::kIPadTabletUserAgent,
  UserAgentMatcherTestBase::kIPadUserAgent,
  UserAgentMatcherTestBase::kKindleTabletUserAgent,
  UserAgentMatcherTestBase::kNexus7ChromeUserAgent,
  UserAgentMatcherTestBase::kSilkTabletUserAgent
};

const char* const
UserAgentMatcherTestBase::kImageInliningSupportedUserAgents[] = {
  "",  // Empty user agent.
  UserAgentMatcherTestBase::kAndroidChrome21UserAgent,
  UserAgentMatcherTestBase::kAndroidHCUserAgent,
  UserAgentMatcherTestBase::kAndroidICSUserAgent,
  UserAgentMatcherTestBase::kChromeUserAgent,
  UserAgentMatcherTestBase::kFirefoxUserAgent,
  UserAgentMatcherTestBase::kIe9UserAgent,
  UserAgentMatcherTestBase::kIe10UserAgent,
  UserAgentMatcherTestBase::kIPhoneUserAgent,
  UserAgentMatcherTestBase::kOpera8UserAgent,
  UserAgentMatcherTestBase::kSafariUserAgent,
  UserAgentMatcherTestBase::kIe11UserAgents[0],
  UserAgentMatcherTestBase::kIe11UserAgents[1],
  UserAgentMatcherTestBase::kIe11UserAgents[2],
  UserAgentMatcherTestBase::kIe11UserAgents[3],
  UserAgentMatcherTestBase::kGoogleAdsBotUserAgent,
  UserAgentMatcherTestBase::kGoogleAdsBotMobileUserAgent,
};

const int UserAgentMatcherTestBase::kIe11UserAgentsArraySize =
    arraysize(kIe11UserAgents);
const int UserAgentMatcherTestBase::kMobileUserAgentsArraySize =
    arraysize(kMobileUserAgents);
const int UserAgentMatcherTestBase::kDesktopUserAgentsArraySize =
    arraysize(kDesktopUserAgents);
const int UserAgentMatcherTestBase::kTabletUserAgentsArraySize =
    arraysize(kTabletUserAgents);
const int UserAgentMatcherTestBase::kImageInliningSupportedUserAgentsArraySize =
    arraysize(kImageInliningSupportedUserAgents);

UserAgentMatcherTestBase::UserAgentMatcherTestBase() {
  user_agent_matcher_.reset(new UserAgentMatcher());
}

bool UserAgentMatcherTestBase::IsMobileUserAgent(
    const StringPiece& user_agent) {
  return user_agent_matcher_->GetDeviceTypeForUA(user_agent) ==
      UserAgentMatcher::kMobile;
}

bool UserAgentMatcherTestBase::IsDesktopUserAgent(
    const StringPiece& user_agent) {
  return user_agent_matcher_->GetDeviceTypeForUA(user_agent) ==
      UserAgentMatcher::kDesktop;
}

bool UserAgentMatcherTestBase::IsTabletUserAgent(
    const StringPiece& user_agent) {
  return user_agent_matcher_->GetDeviceTypeForUA(user_agent) ==
      UserAgentMatcher::kTablet;
}

void UserAgentMatcherTestBase::VerifyGetDeviceTypeForUA() {
  for (int i = 0; i < kMobileUserAgentsArraySize; ++i) {
    EXPECT_TRUE(IsMobileUserAgent(kMobileUserAgents[i]))
        << "\"" << kMobileUserAgents[i] << "\""
        << " not detected as mobile user agent.";
  }

  for (int i = 0; i < kDesktopUserAgentsArraySize; ++i) {
    EXPECT_TRUE(IsDesktopUserAgent(kDesktopUserAgents[i]))
        << "\"" << kDesktopUserAgents[i] << "\""
        << " not detected as desktop user agent.";
  }

  for (int i = 0; i < kTabletUserAgentsArraySize; ++i) {
    EXPECT_TRUE(IsTabletUserAgent(kTabletUserAgents[i]))
        << "\"" << kTabletUserAgents[i] << "\""
        << " not detected as tablet user agent.";
  }
}

void UserAgentMatcherTestBase::VerifyImageInliningSupport() {
  for (int i = 0;
       i < kImageInliningSupportedUserAgentsArraySize;
       ++i) {
    EXPECT_TRUE(user_agent_matcher_->SupportsImageInlining(
                    kImageInliningSupportedUserAgents[i]))
        << "\"" << kImageInliningSupportedUserAgents[i]
        << "\" not detected as a user agent that supports image inlining";
  }
  EXPECT_FALSE(user_agent_matcher_->SupportsImageInlining(
      "random user agent"));
}

void UserAgentMatcherTestBase::VerifyMobilizationSupport() {
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kAndroidChrome18UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsMobilization(
      kAndroidHCUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kIPhone4Safari));
  EXPECT_FALSE(user_agent_matcher_->SupportsMobilization(
      kIPhoneChrome21UserAgent));;
  EXPECT_FALSE(user_agent_matcher_->SupportsMobilization(
      kBlackBerryOS6UserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kFirefoxUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kFirefoxMobileUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kChrome42UserAgent));
  EXPECT_FALSE(user_agent_matcher_->SupportsMobilization(
      kOperaMiniMobileUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kGoogleAdsBotUserAgent));
  EXPECT_TRUE(user_agent_matcher_->SupportsMobilization(
      kGoogleAdsBotMobileUserAgent));
}

}  // namespace net_instaweb
