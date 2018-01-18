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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_

#include <bitset>
#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dense_hash_map.h"
#include "pagespeed/kernel/base/enum_set.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/gtest_prod.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/rde_hash_map.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/sha1_signature.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_hash.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/wildcard.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/kernel/util/copy_on_write.h"

namespace net_instaweb {

class MessageHandler;
class PurgeSet;
class RequestHeaders;

struct HttpOptions;

// Defines a set of customizations that can be applied to any Rewrite.  There
// are multiple categories of customizations:
//   - filter sets (controllable individually or by level)
//   - options (arbitrarily typed variables)
//   - domain customization (see DomainLawyer class).
//   - FileLoadPolicy (enables reading resources as files from the file system)
// RewriteOptions can be specified in several ways, forming a hierarchy:
//   - Globally for a process
//   - Customized per server (e.g. Apache VirtualHost)
//   - Customized at Directory level (e.g. Apache <Directory> or .htaccess)
//   - Tuned at the request-level (e.g. via request-headers or query-params).
// The hierarchy is implemented via Merging.
//
// The options are themselves a complex system.  Many Option objects are
// instantiated for each RewriteOptions instance.  RewriteOptions can be
// constructed and destroyed multiple times per request so to reduce
// this cost, the static aspects of Options are factored out into
// Properties, which are intialized once per process via
// RewriteOptions::Initialize.  Subclasses may also add new Properties
// and so property-list-merging takes place at Initialization time.
class RewriteOptions {
 protected:
  // These being protected is against the style guide but necessary to keep
  // them protected while still being used by the Option class hierarchy.
  // Note that iwyu.py incorrectly complains about the template classes but
  // scripts/iwyu manually removes the warning.
  template<class ValueType> class Property;
  template<class RewriteOptionsSubclass, class OptionClass> class PropertyLeaf;

 public:
  // If you add or remove anything from this list, you must also update the
  // kFilterVectorStaticInitializer array in rewrite_options.cc.  If you add
  // an image-related filter or a css-related filter, you must add it to the
  // kRelatedFilters array in image_rewrite_filter.cc and/or css_filter.cc
  //
  // Each filter added here should go into at least one of the filter-arrays
  // in rewrite_options.cc, even if it's just kDangerousFilterSet.
  //
  // Filters that can improve bandwidth but have basically zero risk
  // of breaking pages should be added to
  // kOptimizeForBandwidthFilterSet.  Filters with relatively low risk
  // should be added to kCoreFilterSet.
  enum Filter {
    kAddBaseTag,  // Update kFirstFilter if you add something before this.
    kAddHead,
    kAddIds,
    kAddInstrumentation,
    kComputeStatistics,
    kCachePartialHtmlDeprecated,
    kCanonicalizeJavascriptLibraries,
    kCollapseWhitespace,
    kCombineCss,
    kCombineHeads,
    kCombineJavascript,
    kComputeCriticalCss,
    kComputeVisibleTextDeprecated,
    kConvertGifToPng,
    kConvertJpegToProgressive,
    kConvertJpegToWebp,
    kConvertMetaTags,
    kConvertPngToJpeg,
    kConvertToWebpAnimated,
    kConvertToWebpLossless,
    kDebug,
    kDecodeRewrittenUrls,
    kDedupInlinedImages,
    kDeferIframe,
    kDeferJavascript,
    kDelayImages,
    kDeterministicJs,
    kDisableJavascript,
    kDivStructure,
    kElideAttributes,
    kExperimentCollectMobImageInfo,
    kExperimentHttp2,  // used while developing proper HTTP2 features.
    kExplicitCloseTags,
    kExtendCacheCss,
    kExtendCacheImages,
    kExtendCachePdfs,
    kExtendCacheScripts,
    kFallbackRewriteCssUrls,
    kFixReflows,
    kFlattenCssImports,
    kFlushSubresources,
    kHandleNoscriptRedirect,
    kHintPreloadSubresources,
    kHtmlWriterFilter,
    kIncludeJsSourceMaps,
    kInlineCss,
    kInlineGoogleFontCss,
    kInlineImages,
    kInlineImportToLink,
    kInlineJavascript,
    kInPlaceOptimizeForBrowser,
    kInsertAmpLink,
    kInsertDnsPrefetch,
    kInsertGA,
    kInsertImageDimensions,
    kJpegSubsampling,
    kLazyloadImages,
    kLeftTrimUrls,
    kLocalStorageCache,
    kMakeGoogleAnalyticsAsync,
    kMakeShowAdsAsync,
    kMobilize,
    kMobilizePrecompute,  // TODO(jud): This is unused, remove it.
    kMoveCssAboveScripts,
    kMoveCssToHead,
    kOutlineCss,
    kOutlineJavascript,
    kPedantic,
    kPrioritizeCriticalCss,
    kRecompressJpeg,
    kRecompressPng,
    kRecompressWebp,
    kRemoveComments,
    kRemoveQuotes,
    kResizeImages,
    kResizeMobileImages,
    kResizeToRenderedImageDimensions,
    kResponsiveImages,
    kResponsiveImagesZoom,
    kRewriteCss,
    kRewriteDomains,
    kRewriteJavascriptExternal,
    kRewriteJavascriptInline,
    kRewriteStyleAttributes,
    kRewriteStyleAttributesWithUrl,
    kServeDeprecationNotice,
    kSplitHtml,
    kSplitHtmlHelper,
    kSpriteImages,
    kStripImageColorProfile,
    kStripImageMetaData,
    kStripScripts,
    kEndOfFilters
  };

  // When PageSpeed first started there was just off/on.  Off wasn't entirely
  // off, though, because:
  // 1) If you turned it off because it broke something it's helpful to be able
  //    to turn it back on with query params while testing filter combinations
  //    to see what you broke.
  // 2) After turning off PageSpeed you might still get some requests for
  //    .pagespeed. resources and you'd like to serve them.
  //
  // Around when the ngx_pagespeed port was getting started we were having
  // discussions about how this was a bad setup for security purposes.  Someone
  // might want to completely disable the module, in a way where attackers
  // couldn't re-enable it by sending query parameters or .pagespeed. requests.
  // So we released ngx_pagepeed with "off" as a hard off.  Discussion on this
  // progressed, and we decided to add "unplugged" for mod_pagespeed which did
  // the same thing as "off" in ngx_pagespeed.  This left us in a state where
  // (a) mod_pagespeed and ngx_pagespeed disagreed about what "off" meant and
  // (b) there was no way to get mod_pagespeed's meaning of "off" in
  // ngx_pagespeed.
  //
  // Since these are central user-controlled configuration knobs, and we don't
  // want to surprise people by changing what they do, we decided to fix this by
  // adding "standby" to ngx_pagespeed to do what "off" does in mod_pagespeed.
  // Now we can tell people to use unplugged / standby / on, for both mps and
  // nps, with the same meanings.
  enum EnabledEnum {
    // Deprecated.
    //   In Apache: equivalent to 'standby' below.
    //   In Nginx: equivalent to 'unplugged' below.
    kEnabledOff,
    // Pagespeed runs normally.  Can be overridden via query param.
    kEnabledOn,
    // Completely passive. Do not serve .pagespeed. Return from handlers
    // immediately. Cannot be overridden via query param.
    kEnabledUnplugged,
    // Don't optimize HTML. Do serve .pagespeed. Can be overridden via query
    // param.
    kEnabledStandby,
  };

  // Any new Option added should have a corresponding name here that must be
  // passed in when Add*Property is called in AddProperties(). You must also
  // update the LookupOptionByNameTest method in rewrite_options_test.cc. If
  // you add an image-related option or css-related option you must also add
  // it to the kRelatedOptions array in image_rewrite_filter.cc and/or
  // css_filter.cc.
  static const char kAcceptInvalidSignatures[];
  static const char kAccessControlAllowOrigins[];
  static const char kAddOptionsToUrls[];
  static const char kAllowLoggingUrlsInLogRecord[];
  static const char kAllowOptionsToBeSetByCookies[];
  static const char kAllowVaryOn[];
  static const char kAlwaysRewriteCss[];
  static const char kAmpLinkPattern[];
  static const char kAnalyticsID[];
  static const char kAvoidRenamingIntrospectiveJavascript[];
  static const char kAwaitPcacheLookup[];
  static const char kBeaconReinstrumentTimeSec[];
  static const char kBeaconUrl[];
  static const char kCacheFragment[];
  static const char kCacheSmallImagesUnrewritten[];
  static const char kClientDomainRewrite[];
  static const char kCombineAcrossPaths[];
  static const char kContentExperimentID[];
  static const char kContentExperimentVariantID[];
  static const char kCriticalImagesBeaconEnabled[];
  static const char kCssFlattenMaxBytes[];
  static const char kCssImageInlineMaxBytes[];
  static const char kCssInlineMaxBytes[];
  static const char kCssOutlineMinBytes[];
  static const char kCssPreserveURLs[];
  static const char kDefaultCacheHtml[];
  static const char kDisableBackgroundFetchesForBots[];
  static const char kDisableRewriteOnNoTransform[];
  static const char kDomainRewriteCookies[];
  static const char kDomainRewriteHyperlinks[];
  static const char kDomainShardCount[];
  static const char kDownstreamCachePurgeMethod[];
  static const char kDownstreamCacheRebeaconingKey[];
  static const char kDownstreamCacheRewrittenPercentageThreshold[];
  static const char kEnableAggressiveRewritersForMobile[];
  static const char kEnableCachePurge[];
  static const char kEnableDeferJsExperimental[];
  static const char kEnableExtendedInstrumentation[];
  static const char kEnableLazyLoadHighResImages[];
  static const char kEnablePrioritizingScripts[];
  static const char kEnabled[];
  static const char kEnrollExperiment[];
  static const char kExperimentCookieDurationMs[];
  static const char kExperimentSlot[];
  static const char kFetcherTimeOutMs[];
  static const char kFinderPropertiesCacheExpirationTimeMs[];
  static const char kFinderPropertiesCacheRefreshTimeMs[];
  static const char kFlushBufferLimitBytes[];
  static const char kFlushHtml[];
  static const char kFollowFlushes[];
  static const char kGoogleFontCssInlineMaxBytes[];
  static const char kForbidAllDisabledFilters[];
  static const char kHideRefererUsingMeta[];
  static const char kHttpCacheCompressionLevel[];
  static const char kHonorCsp[];
  static const char kIdleFlushTimeMs[];
  static const char kImageInlineMaxBytes[];
  // TODO(huibao): Unify terminology for image rewrites. For example,
  // kImageJpeg*Quality might be renamed to kImageJpegQuality.
  static const char kImageJpegNumProgressiveScans[];
  static const char kImageJpegNumProgressiveScansForSmallScreens[];
  static const char kImageJpegQualityForSaveData[];
  static const char kImageJpegRecompressionQuality[];
  static const char kImageJpegRecompressionQualityForSmallScreens[];
  static const char kImageLimitOptimizedPercent[];
  static const char kImageLimitRenderedAreaPercent[];
  static const char kImageLimitResizeAreaPercent[];
  static const char kImageMaxRewritesAtOnce[];
  static const char kImagePreserveURLs[];
  static const char kImageRecompressionQuality[];
  static const char kImageResolutionLimitBytes[];
  static const char kImageWebpQualityForSaveData[];
  static const char kImageWebpRecompressionQuality[];
  static const char kImageWebpRecompressionQualityForSmallScreens[];
  static const char kImageWebpAnimatedRecompressionQuality[];
  static const char kImageWebpTimeoutMs[];
  static const char kImplicitCacheTtlMs[];
  static const char kIncreaseSpeedTracking[];
  static const char kInlineOnlyCriticalImages[];
  static const char kInPlacePreemptiveRewriteCss[];
  static const char kInPlacePreemptiveRewriteCssImages[];
  static const char kInPlacePreemptiveRewriteImages[];
  static const char kInPlacePreemptiveRewriteJavascript[];
  static const char kInPlaceResourceOptimization[];
  static const char kInPlaceRewriteDeadlineMs[];
  static const char kInPlaceSMaxAgeSec[];
  static const char kInPlaceWaitForOptimized[];
  static const char kJsInlineMaxBytes[];
  static const char kJsOutlineMinBytes[];
  static const char kJsPreserveURLs[];
  static const char kLazyloadImagesAfterOnload[];
  static const char kLazyloadImagesBlankUrl[];
  static const char kLoadFromFileCacheTtlMs[];
  static const char kLogBackgroundRewrite[];
  static const char kLogMobilizationSamples[];
  static const char kLogRewriteTiming[];
  static const char kLogUrlIndices[];
  static const char kLowercaseHtmlNames[];
  static const char kMaxCacheableResponseContentLength[];
  static const char kMaxCombinedCssBytes[];
  static const char kMaxCombinedJsBytes[];
  static const char kMaxHtmlCacheTimeMs[];
  static const char kMaxHtmlParseBytes[];
  static const char kMaxImageSizeLowResolutionBytes[];
  static const char kMaxInlinedPreviewImagesIndex[];
  static const char kMaxLowResImageSizeBytes[];
  static const char kMaxLowResToHighResImageSizePercentage[];
  static const char kMaxRewriteInfoLogSize[];
  static const char kMaxUrlSegmentSize[];
  static const char kMaxUrlSize[];
  static const char kMetadataCacheStalenessThresholdMs[];
  static const char kMinImageSizeLowResolutionBytes[];
  static const char kMinResourceCacheTimeToRewriteMs[];
  static const char kModifyCachingHeaders[];
  static const char kNoop[];
  static const char kNoTransformOptimizedImages[];
  static const char kNonCacheablesForCachePartialHtml[];
  static const char kObliviousPagespeedUrls[];
  static const char kOptionCookiesDurationMs[];
  static const char kOverrideCachingTtlMs[];
  static const char kPreserveSubresourceHints[];
  static const char kPreserveUrlRelativity[];
  static const char kPrivateNotVaryForIE[];
  static const char kProactiveResourceFreshening[];
  static const char kProactivelyFreshenUserFacingRequest[];
  static const char kProgressiveJpegMinBytes[];
  static const char kPubliclyCacheMismatchedHashesExperimental[];
  static const char kRejectBlacklistedStatusCode[];
  static const char kRejectBlacklisted[];
  static const char kRemoteConfigurationTimeoutMs[];
  static const char kRemoteConfigurationUrl[];
  static const char kReportUnloadTime[];
  static const char kRequestOptionOverride[];
  static const char kRespectVary[];
  static const char kRespectXForwardedProto[];
  static const char kResponsiveImageDensities[];
  static const char kRewriteDeadlineMs[];
  static const char kRewriteLevel[];
  static const char kRewriteRandomDropPercentage[];
  static const char kRewriteUncacheableResources[];
  static const char kRunningExperiment[];
  static const char kServeStaleIfFetchError[];
  static const char kServeStaleWhileRevalidateThresholdSec[];
  static const char kServeXhrAccessControlHeaders[];
  static const char kStickyQueryParameters[];
  static const char kSupportNoScriptEnabled[];
  static const char kTestOnlyPrioritizeCriticalCssDontApplyOriginalCss[];
  static const char kUrlSigningKey[];
  static const char kUseAnalyticsJs[];
  static const char kUseBlankImageForInlinePreview[];
  static const char kUseExperimentalJsMinifier[];
  static const char kUseFallbackPropertyCacheValues[];
  static const char kUseImageScanlineApi[];
  static const char kXModPagespeedHeaderValue[];
  static const char kXPsaBlockingRewrite[];
  // Options that require special handling, e.g. non-scalar values
  static const char kAllow[];
  static const char kBlockingRewriteRefererUrls[];
  static const char kDisableFilters[];
  static const char kDisallow[];
  static const char kDomain[];
  static const char kDownstreamCachePurgeLocationPrefix[];
  static const char kEnableFilters[];
  static const char kExperimentVariable[];
  static const char kExperimentSpec[];
  static const char kForbidFilters[];
  static const char kInlineResourcesWithoutExplicitAuthorization[];
  static const char kRetainComment[];
  static const char kPermitIdsForCssCombining[];
  // 2-argument ones:
  static const char kAddResourceHeader[];
  static const char kCustomFetchHeader[];
  static const char kLoadFromFile[];
  static const char kLoadFromFileMatch[];
  static const char kLoadFromFileRule[];
  static const char kLoadFromFileRuleMatch[];
  static const char kMapOriginDomain[];
  static const char kMapProxyDomain[];
  static const char kMapRewriteDomain[];
  static const char kShardDomain[];
  // 3-argument ones:
  static const char kLibrary[];
  static const char kUrlValuedAttribute[];
  // apache/ or system/ specific:
  // TODO(matterbury): move these to system_rewrite_options.cc?
  static const char kCacheFlushFilename[];
  static const char kCacheFlushPollIntervalSec[];
  static const char kCompressMetadataCache[];
  static const char kFetcherProxy[];
  static const char kFetchHttps[];
  static const char kFileCacheCleanInodeLimit[];
  static const char kFileCacheCleanIntervalMs[];
  static const char kFileCacheCleanSizeKb[];
  static const char kFileCachePath[];
  static const char kLogDir[];
  static const char kLruCacheByteLimit[];
  static const char kLruCacheKbPerProcess[];
  static const char kMemcachedServers[];
  static const char kMemcachedThreads[];
  static const char kMemcachedTimeoutUs[];
  static const char kProxySuffix[];
  static const char kRateLimitBackgroundFetches[];
  static const char kServeWebpToAnyAgent[];
  static const char kSlurpDirectory[];
  static const char kSlurpFlushLimit[];
  static const char kSlurpReadOnly[];
  static const char kSslCertDirectory[];
  static const char kSslCertFile[];
  static const char kStatisticsEnabled[];
  static const char kStatisticsLoggingChartsCSS[];
  static const char kStatisticsLoggingChartsJS[];
  static const char kStatisticsLoggingEnabled[];
  static const char kStatisticsLoggingIntervalMs[];
  static const char kStatisticsLoggingMaxFileSizeKb[];
  static const char kTestProxy[];
  static const char kTestProxySlurp[];
  static const char kUseSharedMemLocking[];
  // The option name you have when you don't have an option name.
  static const char kNullOption[];

  // We allow query params to be set in custom beacon URLs through the
  // ModPagespeedBeaconUrl option, but we don't use those query params for
  // validation of a beacon URL. The http and https fields should be the URLs
  // that beacon responses are to be sent to, while http_in and https_in are the
  // fields that should be validated on the server to verify if a URL is a
  // beacon request (they are just a precomputation of the corresponding
  // outbound URL with query params stripped).
  struct BeaconUrl {
    GoogleString http;
    GoogleString https;
    GoogleString http_in;
    GoogleString https_in;
  };

  struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
  };

  struct MobTheme {
    Color background_color;
    Color foreground_color;
    GoogleString logo_url;
  };

  struct NameValue {
    NameValue(StringPiece name_in, StringPiece value_in) {
      name_in.CopyToString(&name);
      value_in.CopyToString(&value);
    }
    GoogleString name;
    GoogleString value;
  };

  // We only create this class so that we get the correct ParseFromString()
  // version for parsing densities.
  class ResponsiveDensities : public std::vector<double> {
  };

  class AllowVaryOn {
   public:
    // Strings for display.
    static const char kNoneString[];
    static const char kAutoString[];

    AllowVaryOn() :
        allow_auto_(false),
        allow_accept_(false),
        allow_save_data_(false),
        allow_user_agent_(false) {
    }

    GoogleString ToString() const;

    bool allow_auto() const {
      return allow_auto_;
    }
    void set_allow_auto(bool v) {
      allow_auto_ = v;
    }
    bool allow_accept() const {
      return allow_accept_;
    }
    void set_allow_accept(bool v) {
      allow_accept_ = v;
    }
    bool allow_save_data() const {
      return allow_save_data_ || allow_auto_;
    }
    void set_allow_save_data(bool v) {
      allow_save_data_ = v;
    }
    bool allow_user_agent() const {
      return allow_user_agent_;
    }
    void set_allow_user_agent(bool v) {
      allow_user_agent_ = v;
    }

   private:
    // All of the properties must be included in
    // RewriteOptions::OptionSignature.
    bool allow_auto_;
    bool allow_accept_;
    bool allow_save_data_;
    bool allow_user_agent_;
  };

  bool AllowVaryOnAuto() const {
    return allow_vary_on_.value().allow_auto();
  }
  bool AllowVaryOnAccept() const {
    return allow_vary_on_.value().allow_accept();
  }
  bool AllowVaryOnSaveData() const {
    return allow_vary_on_.value().allow_save_data();
  }
  bool AllowVaryOnUserAgent() const {
    return allow_vary_on_.value().allow_user_agent();
  }
  GoogleString AllowVaryOnToString() const {
    return ToString(allow_vary_on_.value());
  }

  // Returns true if PageSpeed responds differently for image requests with
  // Save-Data header, i.e., using a unique quality and adding
  // "Vary: Save-Data" header.
  bool SupportSaveData() const {
    return (HasValidSaveDataQualities() && AllowVaryOnSaveData());
  }

  void set_allow_vary_on(const AllowVaryOn& x) {
    set_option(x, &allow_vary_on_);
  }

  // Image qualities and parameters, after applying the inheritance rules.
  int64 ImageJpegQuality() const;
  int64 ImageJpegQualityForSmallScreen() const;
  int64 ImageJpegQualityForSaveData() const;
  int64 ImageWebpQuality() const;
  int64 ImageWebpQualityForSmallScreen() const;
  int64 ImageWebpQualityForSaveData() const;
  int64 ImageWebpAnimatedQuality() const;
  int64 ImageJpegNumProgressiveScansForSmallScreen() const;
  // Returns true if any quality for small screen is valid and different from
  // the base quality.
  bool HasValidSmallScreenQualities() const;
  // Returns true if any quality for Save-Data is valid and different from the
  // base quality.
  bool HasValidSaveDataQualities() const;

  // This version index serves as global signature key.  Much of the
  // data emitted in signatures is based on the option ordering, which
  // can change as we add new options.  So every time there is a
  // binary-incompatible change to the option ordering, we bump this
  // version.
  //
  // Note: we now use a two-letter code for identifying enabled filters, so
  // there is no need bump the option version when changing the filter enum.
  //
  // Updating this value will have the indirect effect of flushing the metadata
  // cache. The HTTPCache can be flushed by updating kHttpCacheVersion in
  // http_cache.cc.
  //
  // This version number should be incremented if any default-values
  // are changed, either in an Add*Property() call or via
  // options->set_default.
  static const int kOptionsVersion = 14;

  // Number of bytes used for signature hashing.
  static const int kHashBytes = 20;

  // Number of bytes capacity in the URL invalidation set.
  static const int kCachePurgeBytes = 25000;

  // Determines the scope at which an option is evaluated.  In Apache,
  // for example, kDirectoryScope indicates it can be changed via .htaccess
  // files, which is the only way that sites using shared hosting can change
  // settings.
  //
  // The options are ordered from most permissive to least permissive.
  enum OptionScope {
    kQueryScope,      // customized at query (query-param, request headers,
                      // response headers)
    kDirectoryScope,  // customized at directory level (.htaccess, <Directory>)
    kServerScope,     // customized at server level (e.g. VirtualHost)
    // Customized at process level only (command-line flags). This is a legacy
    // value that will make us accept it in a VirtualHost in Apache for
    // backwards compatibility; it should not be used for new options.
    kLegacyProcessScope,

    // Customized at process level and enforced as such.
    kProcessScopeStrict,
  };

  static const char kCacheExtenderId[];
  static const char kCssCombinerId[];
  static const char kCssFilterId[];
  static const char kCssImportFlattenerId[];
  static const char kCssInlineId[];
  static const char kGoogleFontCssInlineId[];
  static const char kImageCombineId[];
  static const char kImageCompressionId[];
  static const char kInPlaceRewriteId[];
  static const char kJavascriptCombinerId[];
  static const char kJavascriptInlineId[];
  static const char kJavascriptMinId[];
  static const char kJavascriptMinSourceMapId[];
  static const char kLocalStorageCacheId[];
  static const char kPrioritizeCriticalCssId[];

  // Return the appropriate human-readable filter name for the given filter,
  // e.g. "CombineCss".
  static const char* FilterName(Filter filter);

  // Returns a two-letter id code for this filter, used for for encoding
  // URLs.
  static const char* FilterId(Filter filter);

  // Returns the number of filter ids. This is used to loop over all filter ids
  // using the FilterId() method.
  static int NumFilterIds();

  // Used for enumerating over all entries in the Filter enum.
  static const Filter kFirstFilter = kAddBaseTag;

  typedef EnumSet<Filter, kEndOfFilters> FilterSet;
  typedef std::vector<Filter> FilterVector;

  // Convenience name for a set of rewrite filter ids.
  typedef std::set<GoogleString> FilterIdSet;

  // Lookup the given name to see if it's a filter name or one of the special
  // names like "core" or "rewrite_images", and if so add the corresponding
  // filter(s) to the given set. If the given name doesn't match -and- if
  // handler is not NULL, logs a warning message to handler. Returns true if
  // the name matched and the set was updated, false otherwise.
  static bool AddByNameToFilterSet(const StringPiece& option, FilterSet* set,
                                   MessageHandler* handler);

  // Convenience name for (name,value) pairs of options (typically filter
  // parameters), as well as sets of those pairs.
  typedef std::pair<GoogleString, GoogleString> OptionStringPair;
  typedef std::set<OptionStringPair> OptionSet;

  // The base class for a Property.  This class contains fields of
  // Properties that are independent of type.
  class PropertyBase {
   public:
    PropertyBase(const char* id, StringPiece option_name)
        : id_(id),
          help_text_(nullptr),
          option_name_(option_name),
          scope_(kDirectoryScope),
          do_not_use_for_signature_computation_(false),
          index_(-1) {
    }
    virtual ~PropertyBase();

    // Connect the specified RewriteOption to this property.
    // set_index() must previously have been called on this.
    virtual void InitializeOption(RewriteOptions* options) const = 0;

    void set_do_not_use_for_signature_computation(bool x) {
      do_not_use_for_signature_computation_ = x;
    }
    bool is_used_for_signature_computation() const {
      return !do_not_use_for_signature_computation_;
    }

    void set_scope(OptionScope x) { scope_ = x; }
    OptionScope scope() const { return scope_; }

    void set_help_text(const char* x) { help_text_ = x; }
    const char* help_text() const { return help_text_; }

    void set_index(int index) { index_ = index; }
    const char* id() const { return id_; }
    StringPiece option_name() const { return option_name_; }
    int index() const { return index_; }

    bool safe_to_print() const { return safe_to_print_; }
    void set_safe_to_print(bool safe_to_print) {
      safe_to_print_ = safe_to_print;
    }

   private:
    const char* id_;
    const char* help_text_;
    StringPiece option_name_;  // Key into all_options_.
    OptionScope scope_;
    bool do_not_use_for_signature_computation_;  // Default is false.
    bool safe_to_print_;  // Safe to print in debug filter output.
    int index_;

    DISALLOW_COPY_AND_ASSIGN(PropertyBase);
  };

  typedef std::vector<PropertyBase*> PropertyVector;

  // Base class for Option -- the instantiation of a Property that
  // occurs in each RewriteOptions instance.
  class OptionBase {
   public:
    OptionBase() {}
    virtual ~OptionBase();

    // Returns if parsing was successful. error_detail will be appended to
    // to an error message if this returns false. Implementors are not
    // required to set *error_detail; its the callers responsibility to do so.
    virtual bool SetFromString(StringPiece value_string,
                               GoogleString* error_detail) = 0;
    virtual void Merge(const OptionBase* src) = 0;
    virtual bool was_set() const = 0;
    virtual GoogleString Signature(const Hasher* hasher) const = 0;
    virtual GoogleString ToString() const = 0;
    const char* id() const { return property()->id(); }
    const char* help_text() const { return property()->help_text(); }
    OptionScope scope() const { return property()->scope(); }
    StringPiece option_name() const { return property()->option_name(); }
    bool is_used_for_signature_computation() const {
      return property()->is_used_for_signature_computation();
    }
    virtual const PropertyBase* property() const = 0;
  };

  // Convenience name for a set of rewrite options.
  typedef std::vector<OptionBase*> OptionBaseVector;

  // TODO(huibao): Use bitmask for the values of the enums, and make combination
  // of rewrite levels possible.
  enum RewriteLevel {
    // Enable no filters. Parse HTML but do not perform any
    // transformations. This is the default value. Most users should
    // explicitly enable the kCoreFilters level by calling
    // SetRewriteLevel(kCoreFilters).
    kPassThrough,

    // Enable filters that make resources smaller, but carry no risk
    // of site breakage.  Turning this on implies inplace resource
    // optimization and preserve-URLs.
    kOptimizeForBandwidth,

    // Enable the core set of filters. These filters are considered
    // generally safe for most sites, though even safe filters can
    // break some sites. Most users should specify this option, and
    // then optionally add or remove specific filters based on
    // specific needs.
    kCoreFilters,

    // Enable the filters which are essential to make webpages designed for
    // desktop computers look good on mobile devices.
    kMobilizeFilters,

    // Enable all filters intended for core, but some of which might
    // need more testing. Good for if users are willing to test out
    // the results of the rewrite more closely.
    kTestingCoreFilters,

    // Enable all filters. This includes filters you should never turn
    // on for a real page, like StripScripts!
    kAllFilters,
  };

  // Used for return value of SetOptionFromName.
  enum OptionSettingResult {
    kOptionOk,
    kOptionNameUnknown,
    kOptionValueInvalid
  };

  static const char kDefaultAllowVaryOn[];
  static const int kDefaultBeaconReinstrumentTimeSec;
  static const int64 kDefaultCssFlattenMaxBytes;
  static const int64 kDefaultCssImageInlineMaxBytes;
  static const int64 kDefaultCssInlineMaxBytes;
  static const int64 kDefaultCssOutlineMinBytes;
  static const int64 kDefaultGoogleFontCssInlineMaxBytes;
  static const int64 kDefaultImageInlineMaxBytes;
  static const int64 kDefaultJsInlineMaxBytes;
  static const int64 kDefaultJsOutlineMinBytes;
  static const int64 kDefaultProgressiveJpegMinBytes;
  static const int64 kDefaultMaxCacheableResponseContentLength;
  static const int64 kDefaultMaxHtmlCacheTimeMs;
  static const int64 kDefaultMaxHtmlParseBytes;
  static const int64 kDefaultMaxLowResImageSizeBytes;
  static const int kDefaultMaxLowResToFullResImageSizePercentage;
  static const int64 kDefaultMetadataInputErrorsCacheTtlMs;
  static const int64 kDefaultMinResourceCacheTimeToRewriteMs;
  static const char kDefaultDownstreamCachePurgeMethod[];
  static const int64 kDefaultDownstreamCacheRewrittenPercentageThreshold;
  static const int64 kDefaultIdleFlushTimeMs;
  static const int64 kDefaultFlushBufferLimitBytes;
  static const int64 kDefaultImplicitCacheTtlMs;
  static const int64 kDefaultPrioritizeVisibleContentCacheTimeMs;
  static const char kDefaultBeaconUrl[];
  static const int64 kDefaultImageRecompressQuality;
  static const int64 kDefaultImageJpegQualityForSaveData;
  static const int64 kDefaultImageJpegRecompressQuality;
  static const int64 kDefaultImageJpegRecompressQualityForSmallScreens;
  static const int kDefaultImageLimitOptimizedPercent;
  static const int kDefaultImageLimitRenderedAreaPercent;
  static const int kDefaultImageLimitResizeAreaPercent;
  static const int64 kDefaultImageResolutionLimitBytes;
  static const int64 kDefaultImageJpegNumProgressiveScans;
  static const int64 kDefaultImageWebpQualityForSaveData;
  static const int64 kDefaultImageWebpRecompressQuality;
  static const int64 kDefaultImageWebpAnimatedRecompressQuality;
  static const int64 kDefaultImageWebpRecompressQualityForSmallScreens;
  static const int64 kDefaultImageWebpTimeoutMs;
  static const int kDefaultDomainShardCount;
  static const int64 kDefaultOptionCookiesDurationMs;
  static const int64 kDefaultLoadFromFileCacheTtlMs;
  static const double kDefaultResponsiveImageDensities[];

  // IE limits URL size overall to about 2k characters.  See
  // http://support.microsoft.com/kb/208427/EN-US
  static const int kDefaultMaxUrlSize;

  static const int kDefaultImageMaxRewritesAtOnce;

  // See http://github.com/apache/incubator-pagespeed-mod/issues/9
  // Apache evidently limits each URL path segment (between /) to
  // about 256 characters.  This is not fundamental URL limitation
  // but is Apache specific.
  static const int kDefaultMaxUrlSegmentSize;

  // Default time to wait for rewrite before returning original resource.
  static const int kDefaultRewriteDeadlineMs;

  // Default number of first N images for which low res image is generated by
  // DelayImagesFilter.
  static const int kDefaultMaxInlinedPreviewImagesIndex;
  // Default minimum image size above which low res image is generated by
  // InlinePreviewImagesFilter.
  static const int64 kDefaultMinImageSizeLowResolutionBytes;
  // Default maximum image size below which low res image is generated by
  // InlinePreviewImagesFilter.
  static const int64 kDefaultMaxImageSizeLowResolutionBytes;
  // Default cache expiration value for finder properties in pcache.
  static const int64 kDefaultFinderPropertiesCacheExpirationTimeMs;
  // Default cache refresh value for finder properties in pcache.
  static const int64 kDefaultFinderPropertiesCacheRefreshTimeMs;

  // Default duration after which the experiment cookie will expire on the
  // user's browser.
  static const int64 kDefaultExperimentCookieDurationMs;

  // Default time in milliseconds for which a metadata cache entry may be used
  // after expiry.
  static const int64 kDefaultMetadataCacheStalenessThresholdMs;

  // Default maximum size of the combined CSS resource.
  static const int64 kDefaultMaxCombinedCssBytes;

  // Default maximum size of the combined js resource generated by JsCombiner.
  static const int64 kDefaultMaxCombinedJsBytes;

  static const int kDefaultExperimentTrafficPercent;
  // Default Custom Variable slot in which to put Experiment information.
  static const int kDefaultExperimentSlot;

  static const char kDefaultBlockingRewriteKey[];

  static const char kRejectedRequestUrlKeyName[];

  static const int kDefaultPropertyCacheHttpStatusStabilityThreshold;

  static const int kDefaultMaxRewriteInfoLogSize;

  // This class is a separate subset of options for running an experiment.
  // These options can be specified by a spec string that looks like:
  // "id=<number greater than 0>;level=<rewrite level>;enabled=
  // <comma-separated-list of filters to enable>;disabled=
  // <comma-separated-list of filters to disable>;options=
  // <comma-separated-list of option=value pairs to set>.
  class ExperimentSpec {
   public:
    // Creates a ExperimentSpec parsed from spec.
    // If spec doesn't have an id, then id_ will be set to
    // experiment::kExperimentNotSet.  These ExperimentSpecs will then be
    // rejected by AddExperimentSpec().
    ExperimentSpec(const StringPiece& spec, const RewriteOptions* options,
                   MessageHandler* handler);

    // Creates a ExperimentSpec with id_=id.  All other variables
    // are initialized to 0.
    // This is primarily used for setting up the control and for cloning.
    explicit ExperimentSpec(int id);

    virtual ~ExperimentSpec();

    // Return a ExperimentSpec with all the same information as this one.
    virtual ExperimentSpec* Clone();

    bool is_valid() const { return id_ >= 0; }

    // Accessors.
    int id() const { return id_; }
    int percent() const { return percent_; }
    const GoogleString& ga_id() const { return ga_id_; }
    int slot() const { return ga_variable_slot_; }
    RewriteLevel rewrite_level() const { return rewrite_level_; }
    FilterSet enabled_filters() const { return enabled_filters_; }
    FilterSet disabled_filters() const { return disabled_filters_; }
    OptionSet filter_options() const { return filter_options_; }
    bool matches_device_type(UserAgentMatcher::DeviceType type) const;
    bool use_default() const { return use_default_; }
    GoogleString ToString() const;

    // Mutate the origin domains in DomainLawyer with alternate_origin_domains_.
    void ApplyAlternateOriginsToDomainLawyer(DomainLawyer* domain_lawyer,
                                             MessageHandler* handler) const;

   protected:
    // Merges a spec into this. This follows the same semantics as
    // RewriteOptions. Specifically, filter/options list get unioned, and
    // vars get overwritten, except ID.  In the event of a conflict, e.g.
    // preserve vs extend_cache, *this will take precedence.
    void Merge(const ExperimentSpec& spec);

    typedef std::bitset<net_instaweb::UserAgentMatcher::kEndOfDeviceType>
        DeviceTypeBitSet;

    static bool ParseDeviceTypeBitSet(const StringPiece& in,
                                      DeviceTypeBitSet* out,
                                      MessageHandler* handler);

    struct AlternateOriginDomainSpec {
      StringVector serving_domains;
      GoogleString origin_domain;
      GoogleString host_header;
    };

    // Simple check that 's' is not obviously an invalid host. Used to avoid
    // problems when a port number is accidentally placed in a host field.
    static bool LooksLikeValidHost(const StringPiece& s);

    // Parses the string after an 'alternate_origin_domain' experiment
    // option, returning if it was successfull. If it returns false, the spec is
    // invalid and should be discarded.
    static bool ParseAlternateOriginDomain(const StringPiece& in,
                                           AlternateOriginDomainSpec* out,
                                           MessageHandler* handler);

    // Combine consecutive entries in a StringPieceVector such that
    // [ a, b, "host, port" ] can be turned into [ a, b, host:port ].
    // Inspects vec[first_pos] and vec[first_pos + 1]. If they appear to be a
    // quoted tuple, will replace vec[first_pos] with a combined value and
    // vec[first_pos + 1] will be removed, ie: vec will reduce in size by 1.
    // combined_container is used as the underlying storage for the combined
    // string, if necessary.
    static void CombineQuotedHostPort(StringPieceVector* vec, size_t first_pos,
                                      GoogleString* combined_container);

    // Returns a copy of the input string that will be surrounded by double
    // quotes if the input string contains a colon. This is used to turn
    // host:port into "host:port" when printing an ExperimentSpec.
    static GoogleString QuoteHostPort(const GoogleString& in);

   private:
    FRIEND_TEST(RewriteOptionsTest, ExperimentMergeTest);
    FRIEND_TEST(RewriteOptionsTest, DeviceTypeMergeTest);
    FRIEND_TEST(RewriteOptionsTest, AlternateOriginDomainMergeTest);

    // Parse 'spec' and set the FilterSets, rewrite level, inlining thresholds,
    // and OptionSets accordingly.
    void Initialize(const StringPiece& spec, MessageHandler* handler);

    //
    // If you add any members to this list, be sure to also add them to the
    // Merge method.
    //
    int id_;  // id for this experiment
    GoogleString ga_id_;  // Google Analytics ID for this experiment
    int ga_variable_slot_;
    int percent_;  // percentage of traffic to go through this experiment.
    RewriteLevel rewrite_level_;
    FilterSet enabled_filters_;
    FilterSet disabled_filters_;
    OptionSet filter_options_;
    // bitset to indicate which device types this ExperimentSpec should apply
    // to. If NULL, no device type was specified and the experiment applies
    // to all device types.
    scoped_ptr<DeviceTypeBitSet> matches_device_types_;
    // Use whatever RewriteOptions' settings are without experiments
    // for this experiment.
    bool use_default_;
    // vector of parsed alternate_origin_domain options. These mutations will
    // be applied to a DomainLawyer when passed to MutateDomainLawer.
    typedef std::vector<AlternateOriginDomainSpec> AlternateOriginDomains;
    AlternateOriginDomains alternate_origin_domains_;

    DISALLOW_COPY_AND_ASSIGN(ExperimentSpec);
  };

  // Represents the content type of user-defined url-valued attributes.
  struct ElementAttributeCategory {
    GoogleString element;
    GoogleString attribute;
    semantic_type::Category category;
  };

  // Identifies static properties of RewriteOptions that must be
  // initialized before the properties can be used.  Primarily for the
  // benefit of unit tests and valgrind sanity, Initialize/Terminate
  // is balance-checked.
  //
  // TODO(jmarantz): Add static properties -- currently there are none.
  class Properties {
   public:
    // Initializes a static Properties* object.  Pass the address of a static
    // member variable.  A count is kept of how many times Initialize is called.
    //
    // True will be returned if this was the first call to initialize
    // the properties object, and this can be used by implementations
    // to decide whether to initialize other static variables.
    //
    // Initialization is not thread-safe.
    static bool Initialize(Properties** properties);

    // Terminates a static Properties* object.  Pass the address of a static
    // member variable.
    //
    // True will be returned if Terminate has been called the same number
    // of times as Initialize is called, and this can be used to decide
    // whether to clean up other static variables.
    //
    // Termination is not thread-safe.
    static bool Terminate(Properties** properties_handle);

    // Returns the number of properties
    int size() const { return property_vector_.size(); }

    const PropertyBase* property(int index) const {
      return property_vector_[index];
    }
    PropertyBase* property(int index) { return property_vector_[index]; }

    // Merges the passed-in property-vector into this one, sorting the
    // merged properties.  Each property's needs its index into the
    // merged vector for initializing subclass-specific Options in
    // each constructor.  So this method mutates its input by setting
    // an index field in each property.
    void Merge(Properties* properties);

    void push_back(PropertyBase* p) { property_vector_.push_back(p); }

   private:
    // This object should not be constructed/destructed directly; it should be
    // created by calling Properties::Initialize and Properties::Terminate.
    Properties();
    ~Properties();

    // initialization_count_ acts as a reference count: it is incremented on
    // Initialize(), and decremented on Terminate().  At 0 the object is
    // deleted.
    int initialization_count_;

    // owns_properties_ is set to true if the PropertyBase* in the vector should
    // be deleted when Terminate is called bringing initialization_count_ to 0.
    //   RewriteOptions::properties_.owns_properties_ is true.
    //   RewriteOptions::all_properties_.owns_properties_ is false.
    bool owns_properties_;
    PropertyVector property_vector_;
  };

  // Maps a filter's enum (kAddHead) to its id ("ah") and name ("Add Head").
  struct FilterEnumToIdAndNameEntry {
    RewriteOptions::Filter filter_enum;
    const char* filter_id;
    const char* filter_name;
  };

  static bool ParseRewriteLevel(const StringPiece& in, RewriteLevel* out);

  typedef std::set<semantic_type::Category> ResourceCategorySet;

  static bool ParseInlineUnauthorizedResourceType(
      const StringPiece& in,
      ResourceCategorySet* resource_types);

  // Parse a beacon url, or a pair of beacon urls (http https) separated by a
  // space.  If only an http url is given, the https url is derived from it
  // by simply substituting the protocol.
  static bool ParseBeaconUrl(const StringPiece& in, BeaconUrl* out);

  // Checks if either of the optimizing rewrite options are ON and it includes
  // kRecompressJPeg, kRecompressPng, kRecompressWebp, kConvertGifToPng,
  // kConvertJpegToWebp, kConvertPngToJpeg, and kConvertToWebpLossless.
  bool ImageOptimizationEnabled() const;

  explicit RewriteOptions(ThreadSystem* thread_system);
  virtual ~RewriteOptions();

  // Static initialization of members.  Calls to Initialize and
  // Terminate must be matched.  Returns 'true' for the first
  // Initialize call and the last Terminate call.
  static bool Initialize();
  static bool Terminate();

#ifndef NDEBUG
  // Determines whether it's OK to modify the RewriteOptions in the
  // current thread.  Note that this is stricter than necessary, but
  // makes it easier to reason about potential thread safety issues
  // for copy-on-write sharing of substructures.
  //
  // This is exposed as an external API for ease of unit testing.
  bool ModificationOK() const;

  // Determines whether it's OK to merge from the RewriteOptions object
  // in the current thread.  Note that this is stricter than necessary, but
  // makes it easier to reason about potential thread safety issues
  // for copy-on-write sharing of substructures.
  //
  // This is exposed as an external API for ease of unit testing.
  bool MergeOK() const;
#endif

  // Initializes the Options objects in a RewriteOptions instance
  // based on the supplied Properties vector.  Note that subclasses
  // can statically define additional properties, in which case they
  // should call this method from their constructor.
  void InitializeOptions(const Properties* properties);

  bool modified() const { return modified_; }

  // Sets the default rewrite level for this RewriteOptions object only.
  // Note that the defaults for other RewriteOptions objects are unaffected.
  //
  // TODO(jmarantz): Get rid of this method.  The semantics it requires are
  // costly to implement and don't add much value.
  void SetDefaultRewriteLevel(RewriteLevel level) {
    // Do not set the modified bit -- we are only changing the default.
    level_.set_default(level);
  }
  void SetRewriteLevel(RewriteLevel level) {
    set_option(level, &level_);
  }

  // Returns true iff given name and value are valid to set as a http header.
  // If false is returned, error_message will have a descriptive failure
  // message set.
  bool ValidateConfiguredHttpHeader(const GoogleString& name,
                                    const GoogleString& value,
                                    GoogleString* error_message);

  // If false was returned, no changes are made, and error_message will be
  // assigned a failure description. If true was returned, the name/value
  // passed validation and have been appended to resource_headers_.
  // Multiple calls to ValidateAndAddResourceHeader with the same name will
  // result in multiple headers being appended with the same name.
  bool ValidateAndAddResourceHeader(const StringPiece& name,
                                    const StringPiece& value,
                                    GoogleString* error_message);

  // Unconditionally appends the given name / value to resource_headers_. Use
  // ValidateAndAddResourceHeader if you need validation.
  void AddResourceHeader(const StringPiece& name, const StringPiece& value);

  const NameValue* resource_header(int i) const {
    return resource_headers_[i];
  }

  int num_resource_headers() const {
    return resource_headers_.size();
  }

  // Specify a header to insert when fetching subresources.
  void AddCustomFetchHeader(const StringPiece& name, const StringPiece& value);

  const NameValue* custom_fetch_header(int i) const {
    return custom_fetch_headers_[i];
  }

  int num_custom_fetch_headers() const {
    return custom_fetch_headers_.size();
  }

  // Returns the spec with the id_ that matches id.  Returns NULL if no
  // spec matches.
  ExperimentSpec* GetExperimentSpec(int id) const;

  // Returns false if id is negative, or if the id is reserved
  // for NoExperiment or NotSet, or if we already have an experiment
  // with that id.
  bool AvailableExperimentId(int id);

  // Creates a ExperimentSpec from spec and adds it to the configuration,
  // returning it on success and NULL on failure.
  virtual ExperimentSpec* AddExperimentSpec(const StringPiece& spec,
                                            MessageHandler* handler);

  // Sets which side of the experiment these RewriteOptions are on.
  // Cookie-setting must be done separately.
  // experiment::kExperimentNotSet indicates it hasn't been set.
  // experiment::kNoExperiment indicates this request shouldn't be
  // in any experiment.
  // Then sets the rewriters to match the experiment indicated by id.
  // Returns true if succeeded in setting state.
  virtual bool SetExperimentState(int id);

  // We encode experiment information in urls as an experiment index: the first
  // ExperimentSpec is a, the next is b, and so on.  Empty string or an invalid
  // letter means kNoExperiment.
  void SetExperimentStateStr(const StringPiece& experiment_index);

  int experiment_id() const { return experiment_id_; }

  int experiment_spec_id(int i) const {
    return experiment_specs_[i]->id();
  }

  // Returns a string representation of experiment_id() suitable for consumption
  // by SetExperimentStateStr(), encoding the index of the current experiment
  // (not its id).  If we're not running an experiment, returns the empty
  // string.
  GoogleString GetExperimentStateStr() const;

  ExperimentSpec* experiment_spec(int i) const {
    return experiment_specs_[i];
  }

  int num_experiments() const { return experiment_specs_.size(); }

  bool enroll_experiment() const {
    return enroll_experiment_id() != experiment::kForceNoExperiment;
  }

  // Store that when we see <element attribute=X> we should treat X as a URL
  // pointing to a resource of the type indicated by category.  For example,
  // while by default we would treat the 'src' attribute of an a 'img' element
  // as the URL for an image and will cache-extend, inline, or otherwise
  // optimize it as appropriate, we would not do the same for the 'src'
  // atrtribute of a 'span' element (<span src=...>) because there's no "src"
  // attribute of "span" in the HTML spec.  If someone needed us to treat
  // span.src as a URL, however, they could call:
  //    AddUrlValuedAttribute("src", "span", appropriate_category)
  //
  // Makes copies of element and attribute.
  void AddUrlValuedAttribute(const StringPiece& element,
                             const StringPiece& attribute,
                             semantic_type::Category category);

  // Look up a url-valued attribute, return details via element, attribute,
  // and category.  index must be less than num_url_valued_attributes().
  void UrlValuedAttribute(int index,
                          StringPiece* element,
                          StringPiece* attribute,
                          semantic_type::Category* category) const;

  int num_url_valued_attributes() const {
    if (url_valued_attributes_ == NULL) {
      return 0;
    } else {
      return url_valued_attributes_->size();
    }
  }

  void AddInlineUnauthorizedResourceType(semantic_type::Category category);
  bool HasInlineUnauthorizedResourceType(
      semantic_type::Category category) const;
  void ClearInlineUnauthorizedResourceTypes();
  void set_inline_unauthorized_resource_types(ResourceCategorySet x);

  // Store size, md5 hash and canonical url for library recognition.
  bool RegisterLibrary(
      uint64 bytes, StringPiece md5_hash, StringPiece canonical_url) {
    return WriteableJavascriptLibraryIdentification()->RegisterLibrary(
        bytes, md5_hash, canonical_url);
  }

  // Return the javascript_library_identification_ object that applies to
  // the current configuration (NULL if identification is disabled).
  const JavascriptLibraryIdentification* javascript_library_identification()
      const {
    if (Enabled(kCanonicalizeJavascriptLibraries)) {
      return javascript_library_identification_.get();
    } else {
      return NULL;
    }
  }

  RewriteLevel level() const { return level_.value(); }

  // Enables filters specified without a prefix or with a prefix of '+' and
  // disables filters specified with a prefix of '-'. Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool AdjustFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Adds a set of filters to the enabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool EnableFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Adds a set of filters to the disabled set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool DisableFiltersByCommaSeparatedList(const StringPiece& filters,
                                          MessageHandler* handler);

  // Adds a set of filters to the forbidden set.  Returns false if any
  // of the filter names are invalid, but all the valid ones will be
  // added anyway.
  bool ForbidFiltersByCommaSeparatedList(const StringPiece& filters,
                                         MessageHandler* handler);

  // Set rewrite level to kPassThrough and explicitly disable all filters.
  void DisableAllFilters();

  // Explicitly disable all filters which are not *currently* explicitly enabled
  //
  // Note: Do not call EnableFilter(...) for this options object after calling
  // DisableAllFilters..., because the Disable list will not be auto-updated.
  //
  // Used to deal with query param ?ModPagespeedFilter=foo
  // Which implies that all filters not listed should be disabled.
  void DisableAllFiltersNotExplicitlyEnabled();

  // Adds the filter to the list of enabled filters. However, if the filter
  // is also present in either the list of disabled or forbidden filters,
  // that takes precedence and it is not enabled.
  void EnableFilter(Filter filter);
  // Guarantees that a filter would be enabled even if it is present in the list
  // of disabled filters by removing it from disabled & forbidden filter lists.
  void ForceEnableFilter(Filter filter);
  void DisableFilter(Filter filter);
  void DisableIfNotExplictlyEnabled(Filter filter);
  void ForbidFilter(Filter filter);
  void EnableFilters(const FilterSet& filter_set);
  void DisableFilters(const FilterSet& filter_set);
  void ForbidFilters(const FilterSet& filter_set);
  // Clear all explicitly enabled and disabled filters. Some filters may still
  // be enabled by the rewrite level and HtmlWriterFilter will be enabled.
  void ClearFilters();

  // Induces a filter to be considered enabled by turning on the AllFilters
  // level and disabling the filters that are not wanted.  This is used for
  // testing the desired behavior of PreserveUrls, which is to disable inlining
  // and combining when the level is CoreFilters, but allow explictly enabled
  // combiners and inliners to work.
  //
  // Beware: this mode of enabling filters is difficult to manage
  // across multiple levels of test inheritance.  In particular, once
  // SoftEnableFilterForTesting is called, EnableFilter will no longer
  // work due to the explicit disables.  It does work to call
  // SoftEnableFilterForTesting multiple times to enable several
  // filters, and it also works to call EnableFilter first.  ForceEnable
  // also works.
  //
  // It is only necessary to call this in tests with PreserveUrls.
  //
  // Caveat emptor.
  void SoftEnableFilterForTesting(Filter filter);

  // Enables extend_cache_css, extend_cache_images, and extend_cache_scripts.
  // Does not enable extend_cache_pdfs.
  void EnableExtendCacheFilters();

  bool Enabled(Filter filter) const;
  bool Forbidden(Filter filter) const;
  bool Forbidden(StringPiece filter_id) const;

  // Returns the set of enabled filters that require JavaScript for execution.
  void GetEnabledFiltersRequiringScriptExecution(FilterVector* filters) const;

  // Disables all filters that depend on executing custom javascript.
  void DisableFiltersRequiringScriptExecution();

  // Disables all filters that cannot be run in an Ajax call.
  void DisableFiltersThatCantRunInAjax();

  // Determines whether any filter is enabled that requires a 'head'
  // element to work.
  bool RequiresAddHead() const;

  // Returns true if any filter benefits from per-origin property cache
  // information.
  bool UsePerOriginPropertyCachePage() const;

  // Adds pairs of (option, value) to the option set. The option names and
  // values are not checked for validity, just stored. If the string piece
  // was parsed correctly, this returns true. If there were parsing errors this
  // returns false. The set is still populated on error.
  static bool AddCommaSeparatedListToOptionSet(
      const StringPiece& options, OptionSet* set, MessageHandler* handler);

  // Set Option 'name' to 'value'. Returns whether it succeeded or the kind of
  // failure (wrong name or value), and writes the diagnostic into 'msg'.
  // This only understands simple scalar options, and not more general things
  // like filter lists, blacklists, etc.
  OptionSettingResult SetOptionFromName(
      StringPiece name, StringPiece value, GoogleString* msg);

  // Like above, but doesn't bother formatting the error message.
  OptionSettingResult SetOptionFromName(
      StringPiece name, StringPiece value);

  // Same as SetOptionFromName, but only works with options that are valid
  // to use as query parameters, returning kOptionNameUnknown for properties
  // where the scope() is not kQueryScope.
  OptionSettingResult SetOptionFromQuery(StringPiece name, StringPiece value);

  // Same as SetOptionFromName, but only works with options that are valid
  // to use as remote config, returning kOptionNameUnknown for properties
  // where the scope() is not kQueryScope or kDirectoryScope.
  OptionSettingResult SetOptionFromRemoteConfig(StringPiece name,
                                                StringPiece value);

  GoogleString ScopeEnumToString(OptionScope scope);

  // Advanced option parsing, that can understand non-scalar values
  // (unlike SetOptionFromName), and which is extensible by platforms.
  // Returns whether succeeded or the kind of failure, and writes the
  // diagnostic into *msg.
  //
  // If you extend any of these you also need to extend Merge() and
  // SubclassSignatureLockHeld().
  virtual OptionSettingResult ParseAndSetOptionFromName1(
      StringPiece name, StringPiece arg,
      GoogleString* msg, MessageHandler* handler);
  // Parses and sets options like ParseAndSetOptionFromName1, but with a maximum
  // scope specified.  ParseAndSetOptionFromName1 will apply options from any
  // scope, whereas ParseAndSetOptionFromNameWithScope will only apply options
  // within max_scope scope.
  OptionSettingResult ParseAndSetOptionFromNameWithScope(
      StringPiece name, StringPiece arg, OptionScope max_scope,
      GoogleString* msg, MessageHandler* handler);

  virtual OptionSettingResult ParseAndSetOptionFromName2(
      StringPiece name, StringPiece arg1, StringPiece arg2,
      GoogleString* msg, MessageHandler* handler);

  virtual OptionSettingResult ParseAndSetOptionFromName3(
      StringPiece name, StringPiece arg1, StringPiece arg2, StringPiece arg3,
      GoogleString* msg, MessageHandler* handler);

  // Returns the id and value of the specified option-enum in *id and *value.
  // Sets *was_set to true if this option has been altered from the default.
  //
  // If this option was not found, false is returned, and *id, *was_set, and
  // *value will be left unassigned.
  bool OptionValue(StringPiece option_name, const char** id,
                   bool* was_set, GoogleString* value) const;

  // Set all of the options to their values specified in the option set.
  // Returns true if all options in the set were successful, false if not.
  bool SetOptionsFromName(const OptionSet& option_set, MessageHandler* handler);

  // Sets Option 'name' to 'value'. Returns whether it succeeded and logs
  // any warnings to 'handler'.
  bool SetOptionFromNameAndLog(StringPiece name,
                               StringPiece value,
                               MessageHandler* handler);

  // These static methods are used by Option<T>::SetFromString to set
  // Option<T>::value_ from a string representation of it.
  static bool ParseFromString(StringPiece value_string, bool* value);
  static bool ParseFromString(StringPiece value_string, EnabledEnum* value);
  static bool ParseFromString(StringPiece value_string, int* value) {
    return StringToInt(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string, int64* value) {
    return StringToInt64(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string, double* value) {
    return StringToDouble(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string, GoogleString* value) {
    value_string.CopyToString(value);
    return true;
  }
  static bool ParseFromString(StringPiece value_string, RewriteLevel* value) {
    return ParseRewriteLevel(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string,
                              ResourceCategorySet* value) {
    return ParseInlineUnauthorizedResourceType(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string, BeaconUrl* value) {
    return ParseBeaconUrl(value_string, value);
  }
  static bool ParseFromString(StringPiece value_string, Color* color);
  static bool ParseFromString(StringPiece value_string, MobTheme* theme);
  static bool ParseFromString(StringPiece value_string,
                              ResponsiveDensities* value);
  static bool ParseFromString(StringPiece value_string,
                              protobuf::MessageLite* proto);
  static bool ParseFromString(StringPiece value_string,
                              AllowVaryOn* allow_vary_on);

  // TODO(jmarantz): consider setting flags in the set_ methods so that
  // first's explicit settings can override default values from second.
  int64 css_outline_min_bytes() const { return css_outline_min_bytes_.value(); }
  void set_css_outline_min_bytes(int64 x) {
    set_option(x, &css_outline_min_bytes_);
  }

  const GoogleString& ga_id() const { return ga_id_.value(); }
  void set_ga_id(const GoogleString& id) {
    set_option(id, &ga_id_);
  }

  void set_content_experiment_id(const GoogleString& s) {
    set_option(s, &content_experiment_id_);
  }
  const GoogleString& content_experiment_id() const {
    return content_experiment_id_.value();
  }

  void set_content_experiment_variant_id(const GoogleString& s) {
    set_option(s, &content_experiment_variant_id_);
  }
  const GoogleString& content_experiment_variant_id() const {
    return content_experiment_variant_id_.value();
  }

  bool is_content_experiment() const {
    return !content_experiment_id().empty() &&
           !content_experiment_variant_id().empty();
  }

  bool use_analytics_js() const {
    return use_analytics_js_.value();
  }
  void set_use_analytics_js(bool x) {
    set_option(x, &use_analytics_js_);
  }

  bool increase_speed_tracking() const {
    return increase_speed_tracking_.value();
  }
  void set_increase_speed_tracking(bool x) {
    set_option(x, &increase_speed_tracking_);
  }

  int64 js_outline_min_bytes() const { return js_outline_min_bytes_.value(); }
  void set_js_outline_min_bytes(int64 x) {
    set_option(x, &js_outline_min_bytes_);
  }

  int64 progressive_jpeg_min_bytes() const {
    return progressive_jpeg_min_bytes_.value();
  }
  void set_progressive_jpeg_min_bytes(int64 x) {
    set_option(x, &progressive_jpeg_min_bytes_);
  }

  int64 css_flatten_max_bytes() const { return css_flatten_max_bytes_.value(); }
  void set_css_flatten_max_bytes(int64 x) {
    set_option(x, &css_flatten_max_bytes_);
  }
  bool cache_small_images_unrewritten() const {
    return cache_small_images_unrewritten_.value();
  }
  void set_cache_small_images_unrewritten(bool x) {
    set_option(x, &cache_small_images_unrewritten_);
  }
  int64 image_resolution_limit_bytes() const {
    return image_resolution_limit_bytes_.value();
  }
  void set_image_resolution_limit_bytes(int64 x) {
    set_option(x, &image_resolution_limit_bytes_);
  }

  // Retrieve the image inlining threshold, but return 0 if it's disabled.
  int64 ImageInlineMaxBytes() const;
  void set_image_inline_max_bytes(int64 x);
  // Retrieve the css image inlining threshold, but return 0 if it's disabled.
  int64 CssImageInlineMaxBytes() const;
  void set_css_image_inline_max_bytes(int64 x) {
    set_option(x, &css_image_inline_max_bytes_);
  }
  // The larger of ImageInlineMaxBytes and CssImageInlineMaxBytes.
  int64 MaxImageInlineMaxBytes() const;
  int64 css_inline_max_bytes() const { return css_inline_max_bytes_.value(); }
  void set_css_inline_max_bytes(int64 x) {
    set_option(x, &css_inline_max_bytes_);
  }
  int64 google_font_css_inline_max_bytes() const {
    return google_font_css_inline_max_bytes_.value();
  }
  void set_google_font_css_inline_max_bytes(int64 x) {
    set_option(x, &google_font_css_inline_max_bytes_);
  }
  int64 js_inline_max_bytes() const { return js_inline_max_bytes_.value(); }
  void set_js_inline_max_bytes(int64 x) {
    set_option(x, &js_inline_max_bytes_);
  }
  int64 max_html_cache_time_ms() const {
    return max_html_cache_time_ms_.value();
  }
  void set_max_html_cache_time_ms(int64 x) {
    set_option(x, &max_html_cache_time_ms_);
  }
  int64 max_html_parse_bytes() const {
    return max_html_parse_bytes_.value();
  }
  void set_max_html_parse_bytes(int64 x) {
    set_option(x, &max_html_parse_bytes_);
  }
  int64 max_cacheable_response_content_length() const {
    return max_cacheable_response_content_length_.value();
  }
  void set_max_cacheable_response_content_length(int64 x) {
    set_option(x, &max_cacheable_response_content_length_);
  }
  int64 min_resource_cache_time_to_rewrite_ms() const {
    return min_resource_cache_time_to_rewrite_ms_.value();
  }
  void set_min_resource_cache_time_to_rewrite_ms(int64 x) {
    set_option(x, &min_resource_cache_time_to_rewrite_ms_);
  }
  bool need_to_store_experiment_data() const {
    return need_to_store_experiment_data_;
  }
  void set_need_to_store_experiment_data(bool x) {
    need_to_store_experiment_data_ = x;
  }

  int64 blocking_fetch_timeout_ms() const {
    return blocking_fetch_timeout_ms_.value();
  }
  void set_blocking_fetch_timeout_ms(int64 x) {
    set_option(x, &blocking_fetch_timeout_ms_);
  }
  bool override_ie_document_mode() const {
    return override_ie_document_mode_.value();
  }
  void set_override_ie_document_mode(bool x) {
    set_option(x, &override_ie_document_mode_);
  }

  bool preserve_subresource_hints() const {
    return preserve_subresource_hints_.value();
  }
  void set_preserve_subresource_hints(bool x) {
    set_option(x, &preserve_subresource_hints_);
  }


  bool preserve_url_relativity() const {
    return preserve_url_relativity_.value();
  }
  void set_preserve_url_relativity(bool x) {
    set_option(x, &preserve_url_relativity_);
  }

  // Returns whether the given URL is valid for use in the cache, given the
  // timestamp stored in the cache.
  //
  // It first checks time_ms against the global cache invalidation timestamp.
  //
  // It next checks if PurgeCacheUrl has been called on url with a timestamp
  // earlier than time_ms.  Note: this is not a wildcard check but an
  // exact lookup.
  //
  // Finally, if search_wildcards is true, it scans
  // url_cache_invalidation_entries_ for entries with timestamp_ms > time_ms and
  // url matching the url_pattern.
  //
  // In most contexts where you'd call this you should consider instead
  // calling OptionsAwareHTTPCacheCallback::IsCacheValid instead, which takes
  // into account request-headers.
  bool IsUrlCacheValid(StringPiece url, int64 time_ms,
                       bool search_wildcards) const;

  // If timestamp_ms greater than or equal to the last timestamp in
  // url_cache_invalidation_entries_, then appends an UrlCacheInvalidationEntry
  // with 'timestamp_ms' and 'url_pattern' to url_cache_invalidation_entries_.
  // Else does nothing.
  //
  // Also see PurgeCacheUrl.  AddUrlCacheInvalidationEntry with a non-wildcard
  // pattern and ignores_metadata_and_pcache==false is equivalent to
  // PurgeCacheUrl.
  //
  // If ignores_metadata_and_pcache is true, metadata is not
  // invalidated and property cache is invalidated of URLs matching
  // url_pattern.  If false, metadata cache and property cache entries
  // may be invalidated, depending on whether there are wildcards in
  // the pattern, and whether enable_cache_purge() is true.  Note that
  // HTTP cache invalidation is always exactly for the URLs matching
  // url_pattern.  This should probably always be set to false.
  void AddUrlCacheInvalidationEntry(StringPiece url_pattern,
                                    int64 timestamp_ms,
                                    bool ignores_metadata_and_pcache);

  // Purge a cache entry for an exact URL, not a wildcard.
  void PurgeUrl(StringPiece url, int64 timestamp_ms);

  // Checks if url_cache_invalidation_entries_ is in increasing order of
  // timestamp.  For testing.
  bool IsUrlCacheInvalidationEntriesSorted() const;

  // Supply optional mutex for setting a global cache invalidation
  // timestamp.  Ownership of 'lock' is transfered to this.
  void set_cache_invalidation_timestamp_mutex(ThreadSystem::RWLock* lock) {
    cache_purge_mutex_.reset(lock);
  }

  // Cache invalidation timestamp is in milliseconds since 1970.  It is used
  // for invalidating everything in the cache written prior to the timestamp.
  //
  // TODO(jmarantz): rename to cache_invalidation_timestamp_ms().
  int64 cache_invalidation_timestamp() const;

  // Determines whether there is a valid cache_invalidation_timestamp.  It
  // is invalid to call cache_invalidation_timestamp() if
  // has_cache_invalidation_timestamp_ms() is false.
  bool has_cache_invalidation_timestamp_ms() const;

  // Sets the cache invalidation timestamp -- in milliseconds since
  // 1970.  This function is meant to be called on a RewriteOptions*
  // immediately after instantiation.
  //
  // It can also be used to mutate the invalidation timestamp of a
  // RewriteOptions instance that is used as a base for cloning/merging,
  // in which case you should make sure to establish a real mutex with
  // set_cache_invalidation_timestamp_mutex for such instances.
  bool UpdateCacheInvalidationTimestampMs(int64 timestamp_ms)
      LOCKS_EXCLUDED(cache_purge_mutex_.get());

  // Mutates the options PurgeSet by copying the contents from purge_set.
  // Returns true if the contents actually changed, false if the incoming
  // purge_set was identical to purge_set already in this.
  bool UpdateCachePurgeSet(const CopyOnWrite<PurgeSet>& purge_set)
      LOCKS_EXCLUDED(cache_purge_mutex_.get());

  // Generates a human-readable view of the PurgeSet, including timestamps
  // in GMT.
  GoogleString PurgeSetString() const;

  // How much inactivity of HTML input will result in PSA introducing a flush.
  // Values <= 0 disable the feature.
  int64 idle_flush_time_ms() const {
    return idle_flush_time_ms_.value();
  }
  void set_idle_flush_time_ms(int64 x) {
    set_option(x, &idle_flush_time_ms_);
  }

  // How much accumulated HTML will result in PSA introducing a flush.
  int64 flush_buffer_limit_bytes() const {
    return flush_buffer_limit_bytes_.value();
  }

  void set_flush_buffer_limit_bytes(int64 x) {
    set_option(x, &flush_buffer_limit_bytes_);
  }

  // The maximum length of a URL segment.
  // for http://a/b/c.d, this is == strlen("c.d")
  int max_url_segment_size() const { return max_url_segment_size_.value(); }
  void set_max_url_segment_size(int x) {
    set_option(x, &max_url_segment_size_);
  }

  int image_max_rewrites_at_once() const {
    return image_max_rewrites_at_once_.value();
  }
  void set_image_max_rewrites_at_once(int x) {
    set_option(x, &image_max_rewrites_at_once_);
  }

  // The maximum size of the entire URL.  If '0', this is left unlimited.
  int max_url_size() const { return max_url_size_.value(); }
  void set_max_url_size(int x) {
    set_option(x, &max_url_size_);
  }

  int rewrite_deadline_ms() const { return rewrite_deadline_ms_.value(); }
  void set_rewrite_deadline_ms(int x) {
    set_option(x, &rewrite_deadline_ms_);
  }

  bool test_instant_fetch_rewrite_deadline() const {
    return test_instant_fetch_rewrite_deadline_.value();
  }
  void set_test_instant_fetch_rewrite_deadline(bool x) {
    set_option(x, &test_instant_fetch_rewrite_deadline_);
  }

  void set_test_only_prioritize_critical_css_dont_apply_original_css(bool x) {
    set_option(x, &test_only_prioritize_critical_css_dont_apply_original_css_);
  }
  bool test_only_prioritize_critical_css_dont_apply_original_css() const {
    return test_only_prioritize_critical_css_dont_apply_original_css_.value();
  }

  int domain_shard_count() const { return domain_shard_count_.value(); }
  // The argument is int64 to allow it to be set from the http header or url
  // query param and int64_query_params_ only allows setting of 64 bit values.
  void set_domain_shard_count(int64 x) {
    int value = x;
    set_option(value, &domain_shard_count_);
  }

  void set_enabled(EnabledEnum x) {
    set_option(x, &enabled_);
  }
  bool enabled() const {
    return enabled_.value() == kEnabledOn;
  }
  bool unplugged() const {
    return enabled_.value() == kEnabledUnplugged;
  }
  bool standby() const {
    return !enabled() && !unplugged();
  }

  void set_add_options_to_urls(bool x) {
    set_option(x, &add_options_to_urls_);
  }

  bool add_options_to_urls() const {
    return add_options_to_urls_.value();
  }

  void set_publicly_cache_mismatched_hashes_experimental(bool x) {
    set_option(x, &publicly_cache_mismatched_hashes_experimental_);
  }

  bool publicly_cache_mismatched_hashes_experimental() const {
    return publicly_cache_mismatched_hashes_experimental_.value();
  }

  void set_oblivious_pagespeed_urls(bool x) {
    set_option(x, &oblivious_pagespeed_urls_);
  }

  bool oblivious_pagespeed_urls() const {
    return oblivious_pagespeed_urls_.value();
  }

  void set_in_place_rewriting_enabled(bool x) {
    set_option(x, &in_place_rewriting_enabled_);
  }

  bool in_place_rewriting_enabled() const {
    return CheckBandwidthOption(in_place_rewriting_enabled_);
  }

  void set_in_place_wait_for_optimized(bool x) {
    set_option(x, &in_place_wait_for_optimized_);
  }

  bool in_place_wait_for_optimized() const {
    return (in_place_wait_for_optimized_.value() ||
            (in_place_rewrite_deadline_ms() < 0));
  }

  void set_in_place_rewrite_deadline_ms(int x) {
    set_option(x, &in_place_rewrite_deadline_ms_);
  }

  int in_place_rewrite_deadline_ms() const {
    return in_place_rewrite_deadline_ms_.value();
  }

  void set_in_place_s_maxage_sec(int x) {
    set_option(x, &in_place_s_maxage_sec_);
  }

  int in_place_s_maxage_sec() const {
    return in_place_s_maxage_sec_.value();
  }

  int EffectiveInPlaceSMaxAgeSec() const {
    return modify_caching_headers() ? in_place_s_maxage_sec() : -1;
  }

  void set_in_place_preemptive_rewrite_css(bool x) {
    set_option(x, &in_place_preemptive_rewrite_css_);
  }
  bool in_place_preemptive_rewrite_css() const {
    return CheckBandwidthOption(in_place_preemptive_rewrite_css_);
  }

  void set_in_place_preemptive_rewrite_css_images(bool x) {
    set_option(x, &in_place_preemptive_rewrite_css_images_);
  }
  bool in_place_preemptive_rewrite_css_images() const {
    return CheckBandwidthOption(in_place_preemptive_rewrite_css_images_);
  }

  void set_in_place_preemptive_rewrite_images(bool x) {
    set_option(x, &in_place_preemptive_rewrite_images_);
  }
  bool in_place_preemptive_rewrite_images() const {
    return CheckBandwidthOption(in_place_preemptive_rewrite_images_);
  }

  void set_in_place_preemptive_rewrite_javascript(bool x) {
    set_option(x, &in_place_preemptive_rewrite_javascript_);
  }
  bool in_place_preemptive_rewrite_javascript() const {
    return CheckBandwidthOption(in_place_preemptive_rewrite_javascript_);
  }

  void set_private_not_vary_for_ie(bool x) {
    set_option(x, &private_not_vary_for_ie_);
  }
  bool private_not_vary_for_ie() const {
    return private_not_vary_for_ie_.value();
  }

  void set_combine_across_paths(bool x) {
    set_option(x, &combine_across_paths_);
  }
  bool combine_across_paths() const { return combine_across_paths_.value(); }

  void set_log_background_rewrites(bool x) {
    set_option(x, &log_background_rewrites_);
  }
  bool log_background_rewrites() const {
    return log_background_rewrites_.value();
  }

  void set_log_mobilization_samples(bool x) {
    set_option(x, &log_mobilization_samples_);
  }
  bool log_mobilization_samples() const {
    return log_mobilization_samples_.value();
  }

  void set_log_rewrite_timing(bool x) {
    set_option(x, &log_rewrite_timing_);
  }
  bool log_rewrite_timing() const { return log_rewrite_timing_.value(); }

  void set_log_url_indices(bool x) {
    set_option(x, &log_url_indices_);
  }
  bool log_url_indices() const { return log_url_indices_.value(); }

  void set_lowercase_html_names(bool x) {
    set_option(x, &lowercase_html_names_);
  }
  bool lowercase_html_names() const { return lowercase_html_names_.value(); }

  void set_always_rewrite_css(bool x) {
    set_option(x, &always_rewrite_css_);
  }
  bool always_rewrite_css() const { return always_rewrite_css_.value(); }

  void set_respect_vary(bool x) {
    set_option(x, &respect_vary_);
  }
  bool respect_vary() const { return respect_vary_.value(); }

  void set_respect_x_forwarded_proto(bool x) {
    set_option(x, &respect_x_forwarded_proto_);
  }
  bool respect_x_forwarded_proto() const {
    return respect_x_forwarded_proto_.value();
  }

  void set_flush_html(bool x) { set_option(x, &flush_html_); }
  bool flush_html() const { return flush_html_.value(); }

  void set_serve_stale_if_fetch_error(bool x) {
    set_option(x, &serve_stale_if_fetch_error_);
  }
  bool serve_stale_if_fetch_error() const {
    return serve_stale_if_fetch_error_.value();
  }

  void set_serve_xhr_access_control_headers(bool x) {
    set_option(x, &serve_xhr_access_control_headers_);
  }
  bool serve_xhr_access_control_headers() const {
    return serve_xhr_access_control_headers_.value();
  }

  void set_proactively_freshen_user_facing_request(bool x) {
    set_option(x, &proactively_freshen_user_facing_request_);
  }
  bool proactively_freshen_user_facing_request() const {
    return proactively_freshen_user_facing_request_.value();
  }

  void set_serve_stale_while_revalidate_threshold_sec(int64 x) {
    set_option(x, &serve_stale_while_revalidate_threshold_sec_);
  }
  int64 serve_stale_while_revalidate_threshold_sec() const {
    return serve_stale_while_revalidate_threshold_sec_.value();
  }

  void set_default_cache_html(bool x) { set_option(x, &default_cache_html_); }
  bool default_cache_html() const { return default_cache_html_.value(); }

  void set_modify_caching_headers(bool x) {
    set_option(x, &modify_caching_headers_);
  }
  bool modify_caching_headers() const {
    return modify_caching_headers_.value();
  }

  void set_inline_only_critical_images(bool x) {
    set_option(x, &inline_only_critical_images_);
  }
  bool inline_only_critical_images() const {
    return inline_only_critical_images_.value();
  }

  void set_critical_images_beacon_enabled(bool x) {
    set_option(x, &critical_images_beacon_enabled_);
  }
  bool critical_images_beacon_enabled() const {
    return critical_images_beacon_enabled_.value();
  }

  void set_beacon_reinstrument_time_sec(int x) {
    set_option(x, &beacon_reinstrument_time_sec_);
  }
  int beacon_reinstrument_time_sec() const {
    return beacon_reinstrument_time_sec_.value();
  }

  void set_accept_invalid_signatures(bool x) {
    set_option(x, &accept_invalid_signatures_);
  }
  bool accept_invalid_signatures() const {
    return accept_invalid_signatures_.value();
  }

  void set_remote_configuration_timeout_ms(int64 x) {
    set_option(x, &remote_configuration_timeout_ms_);
  }
  int64 remote_configuration_timeout_ms() const {
    return remote_configuration_timeout_ms_.value();
  }

  void set_remote_configuration_url(StringPiece p) {
    set_option(GoogleString(p.data(), p.size()), &remote_configuration_url_);
  }
  const GoogleString& remote_configuration_url() const {
    return remote_configuration_url_.value();
  }

  void set_http_cache_compression_level(int x) {
    set_option(x, &http_cache_compression_level_);
  }
  int http_cache_compression_level() const {
    return http_cache_compression_level_.value();
  }

  void set_request_option_override(StringPiece p) {
    set_option(GoogleString(p.data(), p.size()), &request_option_override_);
  }
  const GoogleString& request_option_override() const {
    return request_option_override_.value();
  }

  void set_url_signing_key(StringPiece p) {
    set_option(GoogleString(p.data(), p.size()), &url_signing_key_);
  }
  const GoogleString& url_signing_key() const {
    return url_signing_key_.value();
  }

  void set_lazyload_images_after_onload(bool x) {
    set_option(x, &lazyload_images_after_onload_);
  }
  bool lazyload_images_after_onload() const {
    return lazyload_images_after_onload_.value();
  }

  void set_lazyload_images_blank_url(StringPiece p) {
    set_option(GoogleString(p.data(), p.size()), &lazyload_images_blank_url_);
  }
  const GoogleString& lazyload_images_blank_url() const {
    return lazyload_images_blank_url_.value();
  }

  void set_max_inlined_preview_images_index(int x) {
    set_option(x, &max_inlined_preview_images_index_);
  }
  int max_inlined_preview_images_index() const {
    return max_inlined_preview_images_index_.value();
  }

  void set_use_blank_image_for_inline_preview(bool x) {
    set_option(x, &use_blank_image_for_inline_preview_);
  }
  bool use_blank_image_for_inline_preview() const {
    return use_blank_image_for_inline_preview_.value();
  }

  void set_min_image_size_low_resolution_bytes(int64 x) {
    set_option(x, &min_image_size_low_resolution_bytes_);
  }
  int64 min_image_size_low_resolution_bytes() const {
    return min_image_size_low_resolution_bytes_.value();
  }

  void set_max_image_size_low_resolution_bytes(int64 x) {
    set_option(x, &max_image_size_low_resolution_bytes_);
  }
  int64 max_image_size_low_resolution_bytes() const {
    return max_image_size_low_resolution_bytes_.value();
  }

  void set_experiment_cookie_duration_ms(int64 x) {
    set_option(x, &experiment_cookie_duration_ms_);
  }
  int64 experiment_cookie_duration_ms() const {
    return experiment_cookie_duration_ms_.value();
  }

  void set_finder_properties_cache_expiration_time_ms(int64 x) {
    set_option(x, &finder_properties_cache_expiration_time_ms_);
  }
  int64 finder_properties_cache_expiration_time_ms() const {
    return finder_properties_cache_expiration_time_ms_.value();
  }

  void set_finder_properties_cache_refresh_time_ms(int64 x) {
    set_option(x, &finder_properties_cache_refresh_time_ms_);
  }
  int64 finder_properties_cache_refresh_time_ms() const {
    return finder_properties_cache_refresh_time_ms_.value();
  }

  void set_rewrite_random_drop_percentage(int x) {
    set_option(x, &rewrite_random_drop_percentage_);
  }
  int rewrite_random_drop_percentage() const {
    return rewrite_random_drop_percentage_.value();
  }

  // css_preserve_urls() is determined by the following rules in order:
  // 1. Value set by the user, if the user has explicitly set it.
  // 2. Default value (true) for OptimizeForBandwidth, if this is the rewrite
  //    level.
  // 3. Default value (true) for MobilizeFilters, if this is the rewrite level.
  // 4. Default value (false) otherwise.
  bool css_preserve_urls() const {
    return (CheckBandwidthOption(css_preserve_urls_) ||
            CheckMobilizeFiltersOption(css_preserve_urls_));
  }
  void set_css_preserve_urls(bool x) {
    set_option(x, &css_preserve_urls_);
  }

  bool image_preserve_urls() const {
    return CheckBandwidthOption(image_preserve_urls_);
  }
  void set_image_preserve_urls(bool x) {
    set_option(x, &image_preserve_urls_);
  }

  bool js_preserve_urls() const {
    return CheckBandwidthOption(js_preserve_urls_);
  }
  void set_js_preserve_urls(bool x) {
    set_option(x, &js_preserve_urls_);
  }

  void set_metadata_cache_staleness_threshold_ms(int64 x) {
    set_option(x, &metadata_cache_staleness_threshold_ms_);
  }
  int64 metadata_cache_staleness_threshold_ms() const {
    return metadata_cache_staleness_threshold_ms_.value();
  }

  void set_metadata_input_errors_cache_ttl_ms(int64 x) {
    set_option(x, &metadata_input_errors_cache_ttl_ms_);
  }
  int64 metadata_input_errors_cache_ttl_ms() const {
    return metadata_input_errors_cache_ttl_ms_.value();
  }

  const GoogleString& downstream_cache_purge_method() const {
    return downstream_cache_purge_method_.value();
  }
  void set_downstream_cache_purge_method(StringPiece p) {
    set_option(p.as_string(), &downstream_cache_purge_method_);
  }

  const GoogleString& downstream_cache_purge_location_prefix() const {
    return downstream_cache_purge_location_prefix_.value();
  }
  void set_downstream_cache_purge_location_prefix(StringPiece p) {
    // Remove any trailing slashes. Leaving them in causes the request to have
    // multiple trailing slashes.
    while (p.ends_with("/")) {
      p.remove_suffix(1);
    }
    set_option(p.as_string(), &downstream_cache_purge_location_prefix_);
  }
  bool IsDownstreamCacheIntegrationEnabled() const {
    return !downstream_cache_purge_location_prefix().empty();
  }

  void set_downstream_cache_rebeaconing_key(StringPiece p) {
      set_option(p.as_string(), &downstream_cache_rebeaconing_key_);
  }
  const GoogleString& downstream_cache_rebeaconing_key() const {
    return downstream_cache_rebeaconing_key_.value();
  }
  bool IsDownstreamCacheRebeaconingKeyConfigured() const {
    return !downstream_cache_rebeaconing_key().empty();
  }
  // Return true only if downstream cache rebeaconing key is configured and
  // the key argument matches the configured key.
  bool MatchesDownstreamCacheRebeaconingKey(StringPiece key) const {
    if (!IsDownstreamCacheRebeaconingKeyConfigured()) {
      return false;
    }
    return StringCaseEqual(key, downstream_cache_rebeaconing_key());
  }

  void set_downstream_cache_rewritten_percentage_threshold(int64 x) {
    set_option(x, &downstream_cache_rewritten_percentage_threshold_);
  }
  int64 downstream_cache_rewritten_percentage_threshold() const {
    return downstream_cache_rewritten_percentage_threshold_.value();
  }

  const BeaconUrl& beacon_url() const { return beacon_url_.value(); }
  void set_beacon_url(const GoogleString& beacon_url) {
    GoogleString ignored_error_detail;
    beacon_url_.SetFromString(beacon_url, &ignored_error_detail);
  }

  // Return false in a subclass if you want to disallow all URL trimming in CSS.
  virtual bool trim_urls_in_css() const { return true; }

  void set_image_jpeg_recompress_quality(int64 x) {
    set_option(x, &image_jpeg_recompress_quality_);
  }

  void set_image_jpeg_recompress_quality_for_small_screens(int64 x) {
    set_option(x, &image_jpeg_recompress_quality_for_small_screens_);
  }

  void set_image_jpeg_quality_for_save_data(int64 x) {
    set_option(x, &image_jpeg_quality_for_save_data_);
  }

  int64 image_recompress_quality() const {
    return image_recompress_quality_.value();
  }
  void set_image_recompress_quality(int64 x) {
    set_option(x, &image_recompress_quality_);
  }

  int image_limit_optimized_percent() const {
    return image_limit_optimized_percent_.value();
  }
  void set_image_limit_optimized_percent(int x) {
    set_option(x, &image_limit_optimized_percent_);
  }
  int image_limit_resize_area_percent() const {
    return image_limit_resize_area_percent_.value();
  }
  void set_image_limit_resize_area_percent(int x) {
    set_option(x, &image_limit_resize_area_percent_);
  }

  int image_limit_rendered_area_percent() const {
    return image_limit_rendered_area_percent_.value();
  }
  void set_image_limit_rendered_area_percent(int x) {
    set_option(x, &image_limit_rendered_area_percent_);
  }

  int64 image_jpeg_num_progressive_scans() const {
    return image_jpeg_num_progressive_scans_.value();
  }
  void set_image_jpeg_num_progressive_scans(int64 x) {
    set_option(x, &image_jpeg_num_progressive_scans_);
  }

  void set_image_jpeg_num_progressive_scans_for_small_screens(int64 x) {
    set_option(x, &image_jpeg_num_progressive_scans_for_small_screens_);
  }

  void set_image_webp_recompress_quality(int64 x) {
    set_option(x, &image_webp_recompress_quality_);
  }

  void set_image_webp_recompress_quality_for_small_screens(int64 x) {
    set_option(x, &image_webp_recompress_quality_for_small_screens_);
  }

  void set_image_webp_animated_recompress_quality(int64 x) {
    set_option(x, &image_webp_animated_recompress_quality_);
  }

  void set_image_webp_quality_for_save_data(int64 x) {
    set_option(x, &image_webp_quality_for_save_data_);
  }

  int64 image_webp_timeout_ms() const {
    return image_webp_timeout_ms_.value();
  }
  void set_image_webp_timeout_ms(int64 x) {
    set_option(x, &image_webp_timeout_ms_);
  }

  bool domain_rewrite_hyperlinks() const {
    return CheckMobilizeFiltersOption(domain_rewrite_hyperlinks_);
  }
  void set_domain_rewrite_hyperlinks(bool x) {
    set_option(x, &domain_rewrite_hyperlinks_);
  }

  bool domain_rewrite_cookies() const {
    return CheckMobilizeFiltersOption(domain_rewrite_cookies_);
  }
  void set_domain_rewrite_cookies(bool x) {
    set_option(x, &domain_rewrite_cookies_);
  }

  bool client_domain_rewrite() const {
    return client_domain_rewrite_.value();
  }
  void set_client_domain_rewrite(bool x) {
    set_option(x, &client_domain_rewrite_);
  }

  void set_follow_flushes(bool x) { set_option(x, &follow_flushes_); }
  bool follow_flushes() const { return follow_flushes_.value(); }

  void set_enable_defer_js_experimental(bool x) {
    set_option(x, &enable_defer_js_experimental_);
  }
  bool enable_defer_js_experimental() const {
    return enable_defer_js_experimental_.value();
  }

  void set_disable_rewrite_on_no_transform(bool x) {
    set_option(x, &disable_rewrite_on_no_transform_);
  }
  bool disable_rewrite_on_no_transform() const {
    return disable_rewrite_on_no_transform_.value();
  }

  void set_disable_background_fetches_for_bots(bool x) {
    set_option(x, &disable_background_fetches_for_bots_);
  }
  bool disable_background_fetches_for_bots() const {
    return disable_background_fetches_for_bots_.value();
  }

  void set_enable_cache_purge(bool x) {
    set_option(x, &enable_cache_purge_);
  }
  bool enable_cache_purge() const {
    return enable_cache_purge_.value();
  }

  void set_proactive_resource_freshening(bool x) {
    set_option(x, &proactive_resource_freshening_);
  }
  bool proactive_resource_freshening() const {
    return proactive_resource_freshening_.value();
  }

  void set_lazyload_highres_images(bool x) {
    set_option(x, &lazyload_highres_images_);
  }
  bool lazyload_highres_images() const {
    return lazyload_highres_images_.value();
  }

  void set_use_fallback_property_cache_values(bool x) {
    set_option(x, &use_fallback_property_cache_values_);
  }
  bool use_fallback_property_cache_values() const {
    return use_fallback_property_cache_values_.value();
  }

  void set_await_pcache_lookup(bool x) {
    set_option(x, &await_pcache_lookup_);
  }
  bool await_pcache_lookup() const {
    return await_pcache_lookup_.value();
  }

  void set_enable_prioritizing_scripts(bool x) {
    set_option(x, &enable_prioritizing_scripts_);
  }
  bool enable_prioritizing_scripts() const {
    return enable_prioritizing_scripts_.value();
  }

  const GoogleString& blocking_rewrite_key() const {
    return blocking_rewrite_key_.value();
  }
  void set_blocking_rewrite_key(StringPiece p) {
    set_option(p.as_string(), &blocking_rewrite_key_);
  }

  void EnableBlockingRewriteForRefererUrlPattern(
      StringPiece url_pattern) {
    Modify();
    blocking_rewrite_referer_urls_.MakeWriteable()->Allow(url_pattern);
  }

  bool IsBlockingRewriteEnabledForReferer(StringPiece url) const {
    return blocking_rewrite_referer_urls_->Match(url, false);
  }

  bool IsBlockingRewriteRefererUrlPatternPresent() const {
    return blocking_rewrite_referer_urls_->num_wildcards() > 0;
  }

  bool rewrite_uncacheable_resources() const {
    return rewrite_uncacheable_resources_.value();
  }

  void set_rewrite_uncacheable_resources(bool x) {
    set_option(x, &rewrite_uncacheable_resources_);
  }

  void set_running_experiment(bool x) {
    set_option(x, &running_experiment_);
  }
  bool running_experiment() const {
    return running_experiment_.value();
  }

  // x should be between 1 and 5 inclusive.
  void set_experiment_ga_slot(int x) {
    set_option(x, &experiment_ga_slot_);
  }
  int experiment_ga_slot() const { return experiment_ga_slot_.value(); }

  void set_enroll_experiment_id(int x) {
    set_option(x, &enroll_experiment_id_);
  }
  int enroll_experiment_id() const { return enroll_experiment_id_.value(); }

  void set_report_unload_time(bool x) {
    set_option(x, &report_unload_time_);
  }
  bool report_unload_time() const {
    return report_unload_time_.value();
  }

  void set_implicit_cache_ttl_ms(int64 x) {
    set_option(x, &implicit_cache_ttl_ms_);
  }
  int64 implicit_cache_ttl_ms() const {
    return implicit_cache_ttl_ms_.value();
  }

  void set_load_from_file_cache_ttl_ms(int64 x) {
    set_option(x, &load_from_file_cache_ttl_ms_);
  }
  int64 load_from_file_cache_ttl_ms() const {
    return load_from_file_cache_ttl_ms_.value();
  }
  bool load_from_file_cache_ttl_ms_was_set() const {
    return load_from_file_cache_ttl_ms_.was_set();
  }

  void set_x_header_value(StringPiece p) {
    set_option(p.as_string(), &x_header_value_);
  }
  const GoogleString& x_header_value() const {
    return x_header_value_.value();
  }

  void set_avoid_renaming_introspective_javascript(bool x) {
    set_option(x, &avoid_renaming_introspective_javascript_);
  }
  bool avoid_renaming_introspective_javascript() const {
    return avoid_renaming_introspective_javascript_.value();
  }

  void set_forbid_all_disabled_filters(bool x) {
    set_option(x, &forbid_all_disabled_filters_);
  }
  bool forbid_all_disabled_filters() const {
    return forbid_all_disabled_filters_.value();
  }

  bool reject_blacklisted() const { return reject_blacklisted_.value(); }
  void set_reject_blacklisted(bool x) {
    set_option(x, &reject_blacklisted_);
  }

  HttpStatus::Code reject_blacklisted_status_code() const {
    return static_cast<HttpStatus::Code>(
        reject_blacklisted_status_code_.value());
  }
  void set_reject_blacklisted_status_code(HttpStatus::Code x) {
    set_option(static_cast<int>(x), &reject_blacklisted_status_code_);
  }

  bool support_noscript_enabled() const {
    return support_noscript_enabled_.value();
  }
  void set_support_noscript_enabled(bool x) {
    set_option(x, &support_noscript_enabled_);
  }

  bool enable_extended_instrumentation() const {
    return enable_extended_instrumentation_.value();
  }
  void set_enable_extended_instrumentation(bool x) {
    set_option(x, &enable_extended_instrumentation_);
  }

  bool use_experimental_js_minifier() const {
    return use_experimental_js_minifier_.value();
  }
  void set_use_experimental_js_minifier(bool x) {
    set_option(x, &use_experimental_js_minifier_);
  }

  void set_max_combined_css_bytes(int64 x) {
    set_option(x, &max_combined_css_bytes_);
  }
  int64 max_combined_css_bytes() const {
    return max_combined_css_bytes_.value();
  }

  void set_max_combined_js_bytes(int64 x) {
    set_option(x, &max_combined_js_bytes_);
  }
  int64 max_combined_js_bytes() const {
    return max_combined_js_bytes_.value();
  }

  void set_pre_connect_url(StringPiece p) {
    set_option(GoogleString(p.data(), p.size()), &pre_connect_url_);
  }
  const GoogleString& pre_connect_url() const {
    return pre_connect_url_.value();
  }
  void set_property_cache_http_status_stability_threshold(int x) {
    set_option(x, &property_cache_http_status_stability_threshold_);
  }
  int property_cache_http_status_stability_threshold() const {
    return property_cache_http_status_stability_threshold_.value();
  }

  void set_max_rewrite_info_log_size(int x) {
    set_option(x, &max_rewrite_info_log_size_);
  }
  int max_rewrite_info_log_size() const {
    return max_rewrite_info_log_size_.value();
  }

  void set_enable_aggressive_rewriters_for_mobile(bool x) {
    set_option(x, &enable_aggressive_rewriters_for_mobile_);
  }
  bool enable_aggressive_rewriters_for_mobile() const {
    return enable_aggressive_rewriters_for_mobile_.value();
  }

  void set_allow_logging_urls_in_log_record(bool x) {
    set_option(x, &allow_logging_urls_in_log_record_);
  }
  bool allow_logging_urls_in_log_record() const {
    return allow_logging_urls_in_log_record_.value();
  }

  void set_allow_options_to_be_set_by_cookies(bool x) {
    set_option(x, &allow_options_to_be_set_by_cookies_);
  }
  bool allow_options_to_be_set_by_cookies() const {
    return allow_options_to_be_set_by_cookies_.value();
  }

  void set_non_cacheables_for_cache_partial_html(StringPiece p) {
    set_option(p.as_string(), &non_cacheables_for_cache_partial_html_);
  }
  const GoogleString& non_cacheables_for_cache_partial_html() const {
    return non_cacheables_for_cache_partial_html_.value();
  }

  void set_no_transform_optimized_images(bool x) {
    set_option(x, &no_transform_optimized_images_);
  }
  bool no_transform_optimized_images() const {
    return no_transform_optimized_images_.value();
  }

  void set_access_control_allow_origins(StringPiece p) {
    set_option(p.as_string(), &access_control_allow_origins_);
  }
  const GoogleString& access_control_allow_origins() const {
    return access_control_allow_origins_.value();
  }

  void set_hide_referer_using_meta(bool x) {
    set_option(x, &hide_referer_using_meta_);
  }
  bool hide_referer_using_meta() const {
    return hide_referer_using_meta_.value();
  }

  void set_max_low_res_image_size_bytes(int64 x) {
    set_option(x, &max_low_res_image_size_bytes_);
  }
  int64 max_low_res_image_size_bytes() const {
    return max_low_res_image_size_bytes_.value();
  }

  void set_max_low_res_to_full_res_image_size_percentage(int x) {
    set_option(x, &max_low_res_to_full_res_image_size_percentage_);
  }
  int max_low_res_to_full_res_image_size_percentage() const {
    return max_low_res_to_full_res_image_size_percentage_.value();
  }

  void set_serve_rewritten_webp_urls_to_any_agent(bool x) {
    set_option(x, &serve_rewritten_webp_urls_to_any_agent_);
  }
  bool serve_rewritten_webp_urls_to_any_agent() const {
    return serve_rewritten_webp_urls_to_any_agent_.value();
  }

  void set_cache_fragment(StringPiece p) {
    set_option(p.as_string(), &cache_fragment_);
  }
  const GoogleString& cache_fragment() const {
    return cache_fragment_.value();
  }

  void set_sticky_query_parameters(StringPiece p) {
    set_option(p.as_string(), &sticky_query_parameters_);
  }
  const GoogleString& sticky_query_parameters() const {
    return sticky_query_parameters_.value();
  }

  void set_option_cookies_duration_ms(int64 x) {
    set_option(x, &option_cookies_duration_ms_);
  }
  int64 option_cookies_duration_ms() const {
    return option_cookies_duration_ms_.value();
  }

  void set_responsive_image_densities(const ResponsiveDensities& x) {
    set_option(x, &responsive_image_densities_);
  }
  const ResponsiveDensities& responsive_image_densities() const {
    return responsive_image_densities_.value();
  }

  const GoogleString& amp_link_pattern() const {
    return amp_link_pattern_.value();
  }
  void set_amp_link_pattern(const GoogleString& id) {
    set_option(id, &amp_link_pattern_);
  }

  bool honor_csp() const {
    return honor_csp_.value();
  }
  void set_honor_csp(bool x) {
    set_option(x, &honor_csp_);
  }

  virtual bool DisableDomainRewrite() const { return false; }

  // Merge src into 'this'.  Generally, options that are explicitly
  // set in src will override those explicitly set in 'this' (except that
  // filters forbidden in 'this' cannot be enabled by 'src'), although
  // option Merge implementations can be redefined by specific Option
  // class implementations (e.g. OptionInt64MergeWithMax).  One
  // semantic subject to interpretation is when a core-filter is
  // disabled in the first set and not in the second.  My judgement is
  // that the 'disable' from 'this' should override the core-set
  // membership in the 'src', but not an 'enable' in the 'src'.
  //
  // You can make an exact duplicate of RewriteOptions object 'src' via
  // (new 'typeof src')->Merge(src), aka Clone().
  //
  // Merge expects that 'src' and 'this' are the same type.  If that's
  // not true, this function will DCHECK.
  virtual void Merge(const RewriteOptions& src);

  // Merge the process scope options (including strict ones) from src into
  // this.
  void MergeOnlyProcessScopeOptions(const RewriteOptions& src);

  // Registers a wildcard pattern for to be allowed, potentially overriding
  // previous Disallow wildcards.
  void Allow(StringPiece wildcard_pattern) {
    Modify();
    allow_resources_.MakeWriteable()->Allow(wildcard_pattern);
  }

  // Registers a wildcard pattern for to be disallowed, potentially overriding
  // previous Allow wildcards.
  void Disallow(StringPiece wildcard_pattern) {
    Modify();
    allow_resources_.MakeWriteable()->Disallow(wildcard_pattern);
  }

  // Like Allow().  See IsAllowedWhenInlining().
  void AllowWhenInlining(StringPiece wildcard_pattern) {
    Modify();
    allow_when_inlining_resources_.MakeWriteable()->Allow(wildcard_pattern);
  }

  // Helper function to Disallow something except when inlining.  Useful for
  // resources that you expect to be on good CDNs but may still be worth
  // inlining if small enough.
  void AllowOnlyWhenInlining(StringPiece wildcard_pattern) {
    Disallow(wildcard_pattern);
    AllowWhenInlining(wildcard_pattern);
  }

  // Like Disallow().  See IsAllowedWhenInlining().
  void DisallowWhenInlining(StringPiece wildcard_pattern) {
    Modify();
    allow_when_inlining_resources_.MakeWriteable()->Disallow(wildcard_pattern);
  }

  // Blacklist of javascript files that don't like their names changed.
  // This should be called for root options to set defaults.
  // TODO(sligocki): Rename to allow for more general initialization.
  virtual void DisallowTroublesomeResources();

  // Disallows resources that are served on well-distributed CDNs
  // already, and are likely to be in browser-caches, or that are
  // troublesome resources stored on external domains.  Note: this is
  // not currently called by mod_pagespeed.
  virtual void DisallowResourcesForProxy();

  // When someone asks for a readonly lawyer, we can return a pointer to
  // the potentially shared DomainLawyer* object.  But if you want a mutable
  // one, we clone whatever Lawyer we had and detach it from the shared
  // group.  Here are several scenarios.
  //   1. We are setting up the global_options() for a ServerContext on
  //      startup.  There will be no concurrent access, and at this point
  //      there will be no sharing with other RewriteOptions.
  //   2. We are merging down global_options() based on the
  //      vhost/directory/.htaccess data and need to update (among
  //      other things) the settings.  This may happen concurrently for
  //      several different server-scoped, directory-scoped, or request-scoped
  //      RewriteOptions objects.  Those will all be attached to their
  //      parent when the options get created.  However writing to these
  //      will effectively detach them.
  // One case that would be problematic is a mutation of a parent
  // RewriteOptions->WriteableDomainLawyer() concurrent with instantiating
  // a new child RewriteOptions.  However this does not occur in our system.
  //
  // The only similar place this does occur is cache_invalidation_timestamp_
  // which can be mutated when there are active children.
  const DomainLawyer* domain_lawyer() const { return domain_lawyer_.get(); }
  DomainLawyer* WriteableDomainLawyer();

  FileLoadPolicy* file_load_policy() { return &file_load_policy_; }
  const FileLoadPolicy* file_load_policy() const { return &file_load_policy_; }

  // Determines, based on the sequence of Allow/Disallow calls above, whether
  // a url is allowed.
  bool IsAllowed(StringPiece url) const {
    return allow_resources_->Match(url, true /* default allow */);
  }

  // Call this when:
  //
  //  1. IsAllowed() returns false and
  //  2. The url is for a resource we're planning to inline if successful.
  //
  // If it returns true, it's ok to fetch, rewrite, and inline this resource as
  // if IsAllowed() had returned true.
  bool IsAllowedWhenInlining(StringPiece url) const {
    return allow_when_inlining_resources_->Match(
        url, false /* default disallow */);
  }

  // Adds a new comment wildcard pattern to be retained.
  void RetainComment(StringPiece comment) {
    Modify();
    retain_comments_.MakeWriteable()->Allow(comment);
  }

  // If enabled, the 'remove_comments' filter will remove all HTML comments.
  // As discussed in Issue 237, some comments have semantic value and must
  // be retained.
  bool IsRetainedComment(StringPiece comment) const {
    return retain_comments_->Match(comment, false);
  }

  // Adds a new class name for which lazyload should be disabled.
  void DisableLazyloadForClassName(StringPiece class_name) {
    Modify();
    lazyload_enabled_classes_.MakeWriteable()->Disallow(class_name);
  }

  // Checks if lazyload images is enabled for the specified class.
  bool IsLazyloadEnabledForClassName(StringPiece class_name) const {
    return lazyload_enabled_classes_->Match(class_name, true);
  }

  // Adds a new comment wildcard pattern to be retained.
  void AddCssCombiningWildcard(StringPiece id_wildcard) {
    Modify();
    css_combining_permitted_ids_.MakeWriteable()->Allow(id_wildcard);
  }

  bool IsAllowedIdForCssCombining(StringPiece id) const {
    return css_combining_permitted_ids_->Match(id, false);
  }

  bool CssCombiningMayPermitIds() const {
    return !css_combining_permitted_ids_->empty();
  }

  void set_override_caching_ttl_ms(int64 x) {
    set_option(x, &override_caching_ttl_ms_);
  }
  int64 override_caching_ttl_ms() const {
    return override_caching_ttl_ms_.value();
  }

  // Overrides the cache ttl for all urls matching the wildcard with
  // override_caching_ttl_ms().
  void AddOverrideCacheTtl(StringPiece wildcard) {
    Modify();
    override_caching_wildcard_.MakeWriteable()->Allow(wildcard);
  }

  // Is the cache TTL overridden for the given url?
  bool IsCacheTtlOverridden(StringPiece url) const {
    return override_caching_wildcard_->Match(url, false);
  }

  void AddRejectedUrlWildcard(const GoogleString& wildcard) {
    AddRejectedHeaderWildcard(kRejectedRequestUrlKeyName, wildcard);
  }

  void AddRejectedHeaderWildcard(StringPiece header_name,
                                 const GoogleString& wildcard) {
    Modify();
    std::pair<FastWildcardGroupMap::iterator, bool> insert_result =
        rejected_request_map_.insert(std::make_pair(
            header_name, static_cast<FastWildcardGroup*>(NULL)));

    if (insert_result.second) {
      insert_result.first->second = new FastWildcardGroup;
    }
    insert_result.first->second->Allow(wildcard);
  }

  // Determine if the request url needs to be declined based on the url,
  // request headers and rewrite options.
  bool IsRequestDeclined(const GoogleString& url,
                         const RequestHeaders* request_headers) const;

  // Make an identical copy of these options and return it.  This does
  // *not* copy the signature, and the returned options are not in
  // a frozen state.
  virtual RewriteOptions* Clone() const;

  // Make an empty options object of the same type as this.
  virtual RewriteOptions* NewOptions() const;

  // Computes a signature for the RewriteOptions object, including all
  // contained classes (DomainLawyer, FileLoadPolicy, WildCardGroups).
  //
  // Computing a signature "freezes" the class instance.  Attempting
  // to modify a RewriteOptions after freezing will DCHECK.
  void ComputeSignature() LOCKS_EXCLUDED(cache_purge_mutex_.get());
  void ComputeSignatureLockHeld() SHARED_LOCKS_REQUIRED(cache_purge_mutex_);

  // If you subclass RewriteOptions and store any configuration data that's not
  // an Option, use this hook to include the signature of your additional data.
  virtual GoogleString SubclassSignatureLockHeld() { return ""; }

  // Freeze a RewriteOptions so we can't modify it anymore and thus
  // know that it's safe to read it from multiple threads, but don't
  // bother calculating its signature since we will only be using this
  // instance for merging and cloning.
  void Freeze();

  // Clears the computed signature, unfreezing the options object.
  // Warning: Please note that using this method is extremely risky and should
  // be avoided as much as possible. If you are planning to use this, please
  // discuss this with your team-mates and ensure that you clearly understand
  // its implications. Also, please do repeat this warning at every place you
  // use this method.
  //
  // Returns true if the signature was previously computed, and thus should
  // be recomputed after modification.
  bool ClearSignatureWithCaution();

  bool frozen() const { return frozen_; }

  // Clears a computed signature, unfreezing the options object.  This
  // is intended for testing.  Returns whether the options were frozen
  // in the first place.
  bool ClearSignatureForTesting() {
    bool frozen = frozen_;
    ClearSignatureWithCaution();
    return frozen;
  }

  // Returns the computed signature.
  const GoogleString& signature() const {
    // We take a reader-lock because we may be looking at the
    // global_options signature concurrent with updating it if someone
    // flushes cache.  Note that the default mutex implementation is
    // NullRWLock, which isn't actually a mutex.  Only (currently) for the
    // Apache global_options() object do we create a real mutex.  We
    // don't expect contention here because we take a reader-lock and the
    // only time we Write is if someone flushes the cache.
    ThreadSystem::ScopedReader lock(cache_purge_mutex_.get());
    DCHECK(frozen_);
    DCHECK(!signature_.empty());
    return signature_;
  }

  virtual GoogleString OptionsToString() const;
  GoogleString FilterSetToString(const FilterSet& filter_set) const;
  GoogleString EnabledFiltersToString() const;
  // Returns a string containing the enabled options which do not leak sensitive
  // information about the server state.
  GoogleString SafeEnabledOptionsToString() const;

  // Returns a string identifying the currently running experiment to be used in
  // tagging Google Analytics data.
  virtual GoogleString ToExperimentString() const;

  // Returns a string with more information about the currently running
  // experiment. Primarily used for tagging Google Analytics data.  This format
  // is not at all specific to Google Analytics, however.
  virtual GoogleString ToExperimentDebugString() const;

  // Convert an id string like "ah" to a Filter enum like kAddHead.
  // Returns kEndOfFilters if the id isn't known.
  static Filter LookupFilterById(const StringPiece& filter_id);

  // Convert the filter name to a Filter.
  static Filter LookupFilter(const StringPiece& filter_name);

  // Looks up an option id/name and returns the corresponding PropertyBase if
  // found, or NULL if the id/name is not found.
  static const PropertyBase* LookupOptionById(StringPiece option_id);
  static const PropertyBase* LookupOptionByName(StringPiece option_name);

  // Looks up an option id and returns the corresponding name, or kNullOption
  // if the id is not found. Example: for "ii" it returns "ImageInlineMaxBytes".
  static const StringPiece LookupOptionNameById(StringPiece option_id);

  // Determine if the given option name is valid/known.
  static bool IsValidOptionName(StringPiece name);

  // Determine if this is an option name that used to do things, and which
  // we may therefore want to accept w/a warning for backwards compatibility.
  static bool IsDeprecatedOptionName(StringPiece option_name);

  // Return the list of all options.  Used to initialize the configuration
  // vector to the Apache configuration system.
  const OptionBaseVector& all_options() const {
    return all_options_;
  }

  static const Properties* deprecated_properties() {
    return deprecated_properties_;
  }

  // Determines whether this and that are the same.  Uses the signature() to
  // short-cut most of the deep comparisons, but then compares directly some
  // options and other fields that are omitted from the signature.
  bool IsEqual(const RewriteOptions& that) const;

  // Returns the hasher used for signatures and URLs to purge.
  const Hasher* hasher() const { return &hasher_; }

  const SHA1Signature* sha1signature() const { return &sha1signature_; }

  ThreadSystem* thread_system() const { return thread_system_; }

  // Produces a new HttpOptions each time this is called, shouldn't be a big
  // deal since we don't call it very often and HttpOptions are pretty light,
  // but we might want to reconsider if those assumptions change.
  HttpOptions ComputeHttpOptions() const;

  // Returns true if this configuration turns on options that may need
  // the dependencies cohort to operate.
  bool NeedsDependenciesCohort() const;

 protected:
  // Helper class to represent an Option, whose value is held in some class T.
  // An option is explicitly initialized with its default value, although the
  // default value can be altered later.  It keeps track of whether a
  // value has been explicitly set (independent of whether that happens to
  // coincide with the default value).
  //
  // It can use this knowledge to intelligently merge a 'base' option value
  // into a 'new' option value, allowing explicitly set values from 'base'
  // to override default values from 'new'.
  template<class T> class OptionTemplateBase : public OptionBase {
   public:
    typedef T ValueType;

    OptionTemplateBase() : was_set_(false), property_(NULL) {}

    virtual bool was_set() const { return was_set_; }

    void set(const T& val) {
      was_set_ = true;
      value_ = val;
    }

    void set_default(const T& val) {
      if (!was_set_) {
        value_ = val;
      }
    }

    const T& value() const { return value_; }
    T& mutable_value() { was_set_ = true; return value_; }

    // The signature of the Merge implementation must match the base-class.  The
    // caller is responsible for ensuring that only the same typed Options are
    // compared.  In RewriteOptions::Merge this is guaranteed because the
    // vector<OptionBase*> all_options_ is sorted on option_name().  We DCHECK
    // that the option_name of this and src are the same.
    virtual void Merge(const OptionBase* src) {
      DCHECK(option_name() == src->option_name());
      MergeHelper(static_cast<const OptionTemplateBase*>(src));
    }

    void MergeHelper(const OptionTemplateBase* src) {
      // Even if !src->was_set, the default value needs to be transferred
      // over in case it was changed with set_default or SetDefaultRewriteLevel.
      if (src->was_set_ || !was_set_) {
        value_ = src->value_;
        was_set_ = src->was_set_;
      }
    }

    // The static properties of an Option are held in a Property<T>*.
    void set_property(const Property<T>* property) {
      property_ = property;

      // Note that the copying of default values here is only required
      // to support SetDefaultRewriteLevel, which it should be
      // possible to remove.  Otherwise we could just pull the
      // default value out of properties_ when !was_set_;
      value_ = property->default_value();
    }
    virtual const PropertyBase* property() const { return property_; }

    // Sets a the option default value globally.  This is thread-unsafe,
    // and reaches into the option property_ field via a const-cast to
    // mutate it.  Note that this method does not affect the current value
    // of the instantiated option.
    //
    // TODO(jmarantz): consider an alternate structure where the
    // Property<T>* can be easily located programmatically
    // rather than going through a dummy Option object.
    void set_global_default(const T& val) {
      Property<T>* property = const_cast<Property<T>*>(property_);
      property->set_default(val);
    }

    // Sets a the option's participation in Signatures globally.  This
    // is thread-unsafe, and reaches into the option property_ field
    // via a const-cast to mutate it.  Note that this method does not
    // affect the current value of the instantiated option.
    //
    // TODO(jmarantz): consider an alternate structure where the
    // Property<T>* can be easily located programmatically
    // rather than going through a dummy Option object.
    void DoNotUseForSignatureComputation() {
      Property<T>* property = const_cast<Property<T>*>(property_);
      property->set_do_not_use_for_signature_computation(true);
    }

   private:
    bool was_set_;
    T value_;
    const Property<T>* property_;

    DISALLOW_COPY_AND_ASSIGN(OptionTemplateBase);
  };

  // Subclassing OptionTemplateBase so that the conversion functions that need
  // to invoke static overloaded functions are declared only here.  Enables
  // subclasses of RewriteOptions to override these in case they use Option
  // types not visible here.
  template<class T> class Option : public OptionTemplateBase<T> {
   public:
    Option() {}

    // Sets value_ from value_string.
    virtual bool SetFromString(StringPiece value_string,
                               GoogleString* error_detail) {
      T value;
      bool success = RewriteOptions::ParseFromString(value_string, &value);
      if (success) {
        this->set(value);
      }
      return success;
    }

    virtual GoogleString Signature(const Hasher* hasher) const {
      return RewriteOptions::OptionSignature(this->value(), hasher);
    }

    virtual GoogleString ToString() const {
      return RewriteOptions::ToString(this->value());
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(Option);
  };

 protected:
  // Adds a new Property to 'properties' (the last argument).
  template<class RewriteOptionsSubclass, class OptionClass>
  static void AddProperty(
      typename OptionClass::ValueType default_value,
      OptionClass RewriteOptionsSubclass::*offset,
      const char* id,
      StringPiece option_name,
      OptionScope scope,
      const char* help_text,
      bool safe_to_print,
      Properties* properties) {
    PropertyBase* property =
        new PropertyLeaf<RewriteOptionsSubclass, OptionClass>(
            default_value, offset, id, option_name);
    property->set_scope(scope);
    property->set_help_text(help_text);
    property->set_safe_to_print(safe_to_print);
    properties->push_back(property);
  }

  static void AddDeprecatedProperty(StringPiece option_name,
                                    OptionScope scope) {
    deprecated_properties_->push_back(
        new DeprecatedProperty(option_name, scope));
  }

  // Merges properties into all_properties so that
  // RewriteOptions::Merge and SetOptionFromName can work across
  // options from RewriteOptions and all relevant subclasses.
  //
  // Each RewriteOptions subclass keeps its own property lists using
  // its own private Properties* member variables.  The private lists
  // are used for initialization of default-values during
  // construction.  We cannot initialize subclass default option
  // values during RewriteOptions construction because options with
  // non-POD ValueType (e.g. GoogleString) have not yet been
  // initialized, so we have to keep separate per-class property-lists
  // for use during construction.  However, we use a global sorted
  // list for fast merging and setting-by-option-name.
  static void MergeSubclassProperties(Properties* properties);

  // Populates all_options_, based on the passed-in index, which
  // should correspond to the property index calculated after
  // sorting all_properties_.  This enables us to sort the all_properties_
  // vector once, and use that to give us all_options_ that is sorted
  // the same way.
  void set_option_at(int index, OptionBase* option) {
    all_options_[index] = option;
  }

  // When setting an option, however, we generally are doing so
  // with a variable rather than a constant so it makes sense to pass
  // it by reference.
  template<class T>
  void set_option(const T& new_value, OptionTemplateBase<T>* option) {
    option->set(new_value);
    Modify();
  }

  // Marks the config as modified.
  void Modify();

  // Sets the global default value for 'x_header_value'.  Note that setting
  // this Option reaches through to the underlying property and sets the
  // default value there, and in fact does *not affect the value of the
  // instantiated RewriteOptions object.
  //
  // TODO(jmarantz): Remove this method and make another one that operate
  // directly on the Property.
  void set_default_x_header_value(StringPiece x_header_value) {
    x_header_value_.set_global_default(x_header_value.as_string());
  }

  // Enable/disable filters and set options according to the current
  // ExperimentSpec that experiment_id_ matches. Returns true if the state was
  // set successfully.
  bool SetupExperimentRewriters();

  // Enables filters needed by Experiment regardless of experiment.
  virtual void SetRequiredExperimentFilters();

  // Helper method to add pre-configured ExperimentSpec objects to the internal
  // vector of ExperimentSpec's. Returns true if the experiment was added
  // successfully. Takes ownership of (and may delete) spec.
  bool InsertExperimentSpecInVector(ExperimentSpec* spec);

  // Protected option values so that derived class can modify.
  Option<BeaconUrl> beacon_url_;

  // The value we put for the X-Mod-Pagespeed header. Default is our version.
  Option<GoogleString> x_header_value_;

 protected:
  // Property representing options that got deprecated. Doesn't
  // actually have a corresponding option.
  class DeprecatedProperty : public PropertyBase {
   public:
    explicit DeprecatedProperty(StringPiece option_name, OptionScope scope)
        : PropertyBase("", option_name) {
      set_do_not_use_for_signature_computation(true);
      set_help_text("Deprecated. Do not use");
      set_safe_to_print(false);
      set_scope(scope);
    }

    void InitializeOption(RewriteOptions* options) const override {
      CHECK(false) << "Deprecated properties shouldn't back options!";
    }
  };

  // Type-specific class of Property.  This subclass of PropertyBase
  // knows what sort of value the Option will hold, and so we can put
  // the default value here.
  template<class ValueType>
  class Property : public PropertyBase {
   public:
    // When adding a new Property, we take the default_value by value,
    // not const-reference.  This is because when calling AddProperty
    // we may want to use a compile-time constant
    // (e.g. Timer::kHourMs) which does not have a linkable address.
    Property(ValueType default_value,
             const char* id,
             StringPiece option_name)
        : PropertyBase(id, option_name),
          default_value_(default_value) {
    }

    void set_default(ValueType value) { default_value_ = value; }
    const ValueType& default_value() const { return default_value_; }

   private:
    ValueType default_value_;

    DISALLOW_COPY_AND_ASSIGN(Property);
  };

  // Leaf subclass of Property<ValueType>, which is templated on the class of
  // the corresponding Option.  The template parameters here are
  //   RewriteOptionsSubclass -- the subclass of RewriteOptions in which
  //     this option is instantiated, e.g. ApacheConfig.
  //   OptionClass -- the subclass of OptionBase that is being instantiated
  //     in each RewriteOptionsSubclass.
  // These template parameters are generally automatically discovered by
  // the compiler when AddProperty is called.
  //
  // TODO(jmarantz): It looks tempting to fold Property<T> and
  // PropertyLeaf<T> together, but this is difficult because of the
  // way that the Option class hiearchy is structured and the
  // precision of C++ pointers-to-members.  Attempting that is
  // probably a worthwhile follow-up task.
  template<class RewriteOptionsSubclass, class OptionClass>
  class PropertyLeaf : public Property<typename OptionClass::ValueType> {
   public:
    // Fancy C++ pointers to members; a typesafe version of offsetof.  See
    // http://publib.boulder.ibm.com/infocenter/comphelp/v8v101/index.jsp?
    // topic=%2Fcom.ibm.xlcpp8a.doc%2Flanguage%2Fref%2Fcplr034.htm
    typedef OptionClass RewriteOptionsSubclass::*OptionOffset;
    typedef typename OptionClass::ValueType ValueType;

    PropertyLeaf(ValueType default_value,
                 OptionOffset offset,
                 const char* id,
                 StringPiece option_name)
        : Property<ValueType>(default_value, id, option_name),
          offset_(offset) {
    }

    virtual void InitializeOption(RewriteOptions* options) const {
      RewriteOptionsSubclass* options_subclass =
          static_cast<RewriteOptionsSubclass*>(options);
      OptionClass& option = options_subclass->*offset_;
      option.set_property(this);
      DCHECK_NE(-1, this->index()) << "Call Property::set_index first.";
      options->set_option_at(this->index(), &option);
    }

   private:
    OptionOffset offset_;

    DISALLOW_COPY_AND_ASSIGN(PropertyLeaf);
  };

 private:
  // We need to check for valid settings with CacheFragment.
  class CacheFragmentOption : public Option<GoogleString> {
   public:
    virtual bool SetFromString(StringPiece value_string,
                               GoogleString* error_detail);
  };

  struct OptionIdCompare;

  // Enum type used to record what action must be taken to resolve conflicts
  // between "preserve URLs" and "extend cache" directives at different levels
  // of the merge.  The lower priority wins.  These must be calculated before
  // option/filter merging, and then performed after option/filter merging.
  enum MergeOverride { kNoAction, kDisablePreserve, kDisableFilter };

  static Properties* properties_;          // from RewriteOptions only
  static Properties* all_properties_;      // includes subclass properties

  static Properties* deprecated_properties_;

  FRIEND_TEST(RewriteOptionsTest, ExperimentMergeTest);
  FRIEND_TEST(RewriteOptionsTest, LookupOptionByNameTest);
  FRIEND_TEST(RewriteOptionsTest, ColorUtilTest);
  FRIEND_TEST(RewriteOptionsTest, ParseFloats);

  // Helper functions to check if given header need to be blocked.
  bool HasRejectedHeader(const StringPiece& header_name,
                         const RequestHeaders* request_headers) const;

  bool IsRejectedUrl(const GoogleString& url) const {
    return IsRejectedRequest(kRejectedRequestUrlKeyName, url);
  }

  bool IsRejectedRequest(StringPiece header_name,
                         StringPiece value) const {
    FastWildcardGroupMap::const_iterator it = rejected_request_map_.find(
        header_name);
    if (it != rejected_request_map_.end()) {
      return it->second->Match(value, false);
    }
    return false;
  }

  // Makes sure that javascript_library_identification_ points to an object
  // owned only by us, so that we can modify it; and returns the pointer to it.
  JavascriptLibraryIdentification* WriteableJavascriptLibraryIdentification();

  // A family of urls for which prioritize_visible_content filter can be
  // applied.  url_pattern represents the actual set of urls,
  // cache_time_ms is the duration for which the cacheable portions of pages of
  // the family can be cached, and non_cacheable_elements is a comma-separated
  // list of elements (e.g., "id:foo,class:bar") that cannot be cached for the
  // family.
  struct PrioritizeVisibleContentFamily {
    PrioritizeVisibleContentFamily(StringPiece url_pattern_string,
                                   int64 cache_time_ms_in,
                                   StringPiece non_cacheable_elements_in)
        : url_pattern(url_pattern_string),
          cache_time_ms(cache_time_ms_in),
          non_cacheable_elements(non_cacheable_elements_in.data(),
                                 non_cacheable_elements_in.size()) {}

    PrioritizeVisibleContentFamily* Clone() const {
      return new PrioritizeVisibleContentFamily(
          url_pattern.spec(), cache_time_ms, non_cacheable_elements);
    }

    GoogleString ComputeSignature() const {
      return StrCat(url_pattern.spec(), ";", Integer64ToString(cache_time_ms),
                    ";", non_cacheable_elements);
    }

    GoogleString ToString() const {
      return StrCat("URL pattern: ", url_pattern.spec(), ",  Cache time (ms): ",
                    Integer64ToString(cache_time_ms), ",  Non-cacheable: ",
                    non_cacheable_elements);
    }

    Wildcard url_pattern;
    int64 cache_time_ms;
    GoogleString non_cacheable_elements;
  };

  // A URL pattern cache invalidation entry.  All values cached for an URL that
  // matches url_pattern before timestamp_ms should be evicted.
  struct UrlCacheInvalidationEntry {
    UrlCacheInvalidationEntry(StringPiece url_pattern_in,
                              int64 timestamp_ms_in,
                              bool ignores_metadata_and_pcache_in)
        : url_pattern(url_pattern_in),
          timestamp_ms(timestamp_ms_in),
          ignores_metadata_and_pcache(ignores_metadata_and_pcache_in) {}

    UrlCacheInvalidationEntry* Clone() const {
      return new UrlCacheInvalidationEntry(
          url_pattern.spec(), timestamp_ms, ignores_metadata_and_pcache);
    }

    GoogleString ComputeSignature() const {
      if (ignores_metadata_and_pcache) {
        return "";
      }
      return StrCat(url_pattern.spec(), "@", Integer64ToString(timestamp_ms));
    }

    GoogleString ToString() const {
      return StrCat(
          url_pattern.spec(), ", ",
          (ignores_metadata_and_pcache ? "STRICT" : "REFERENCE"), " @ ",
          Integer64ToString(timestamp_ms));
    }

    Wildcard url_pattern;
    int64 timestamp_ms;
    bool ignores_metadata_and_pcache;
  };

  typedef std::vector<UrlCacheInvalidationEntry*>
      UrlCacheInvalidationEntryVector;
  typedef dense_hash_map<GoogleString, int64> UrlCacheInvalidationMap;

  // Sigh. The folding Hash struct is required so that we ignore case when
  // inserting. The folding Equal struct is required for looking up. Damned
  // if I know why one needs to specify both.
  typedef rde::hash_map<StringPiece, const PropertyBase*,
                        CaseFoldStringPieceHash, /* TLoadFactor4 = */ 6,
                        CaseFoldStringPieceEqual> PropertyNameMap;

  // Private methods to help add properties to
  // RewriteOptions::properties_.  Subclasses define their own
  // versions of these to add to their own private property-lists, and
  // subsequently merge them into RewriteOptions::all_properties_ via
  // MergeSubclassProperties.
  //
  // This version is for a property without a unique option_name_ field.
  // kNullOption will be used for the name, and thus SetOptionFromName cannot
  // be used for options associated with such properties.
  //
  // TODO(jmarantz): This method should be removed and such properties
  // should be moved into RequestContext.
  template<class OptionClass>
  static void AddRequestProperty(typename OptionClass::ValueType default_value,
                                 OptionClass RewriteOptions::*offset,
                                 const char* id, bool safe_to_print) {
    AddProperty(default_value, offset, id, kNullOption, kProcessScopeStrict,
                NULL, safe_to_print, properties_);
  }

  // Adds a property with a unique option_name_ field, allowing use of
  // SetOptionFromName.
  template<class OptionClass>
  static void AddBaseProperty(typename OptionClass::ValueType default_value,
                              OptionClass RewriteOptions::*offset,
                              const char* id,
                              StringPiece option_name,
                              OptionScope scope,
                              const char* help,
                              bool safe_to_print) {
    AddProperty(default_value, offset, id, option_name, scope, help,
                safe_to_print, properties_);
  }

  static void AddProperties();
  bool AddCommaSeparatedListToFilterSetState(
      const StringPiece& filters, FilterSet* set, MessageHandler* handler);
  static bool AddCommaSeparatedListToFilterSet(
      const StringPiece& filters, FilterSet* set, MessageHandler* handler);
  // Initialize the Filter id to enum reverse array used for fast lookups.
  static void InitFilterIdToEnumArray();
  static void InitOptionIdToPropertyArray();
  static void InitOptionNameToPropertyArray();
  // Inits fixed_resource_headers_ to a sorted list of headers not allowed in
  // AddResourceHeader.
  static void InitFixedResourceHeaders();

  // Helper for converting the result of SetOptionFromNameInternal into
  // a status/message pair. The returned result may be adjusted from the passed
  // in one (in particular when option_name is kNullOption).
  OptionSettingResult FormatSetOptionMessage(
      OptionSettingResult result, StringPiece name, StringPiece value,
      StringPiece error_detail, GoogleString* msg);

  // Backend to SetOptionFromName that doesn't do full message
  // formatting. *error_detail may not be always set.
  OptionSettingResult SetOptionFromNameInternal(
      StringPiece name, StringPiece value, OptionScope max_scope,
      GoogleString* error_detail);

  // These static methods enable us to generate signatures for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString OptionSignature(bool x, const Hasher* hasher) {
    return x ? "T" : "F";
  }
  static GoogleString OptionSignature(int x, const Hasher* hasher) {
    return IntegerToString(x);
  }
  static GoogleString OptionSignature(int64 x, const Hasher* hasher) {
    return Integer64ToString(x);
  }
  static GoogleString OptionSignature(const GoogleString& x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(RewriteLevel x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(ResourceCategorySet x,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(const BeaconUrl& beacon_url,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(const MobTheme& mob_theme,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(const ResponsiveDensities& densities,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(const AllowVaryOn& allow_vary_on,
                                      const Hasher* hasher);
  static GoogleString OptionSignature(
      const protobuf::MessageLite& proto,
      const Hasher* hasher);

  // These static methods enable us to generate strings for all
  // instantiated option-types from Option<T>::Signature().
  static GoogleString ToString(bool x) {
    return x ? "True" : "False";
  }
  static GoogleString ToString(int x) {
    return IntegerToString(x);
  }
  static GoogleString ToString(int64 x) {
    return Integer64ToString(x);
  }
  static GoogleString ToString(const GoogleString& x) {
    return x;
  }
  static GoogleString ToString(RewriteLevel x);
  static GoogleString ToString(const ResourceCategorySet &x);
  static GoogleString ToString(const BeaconUrl& beacon_url);
  static GoogleString ToString(const MobTheme& mob_theme);
  static GoogleString ToString(const Color& color);
  static GoogleString ToString(const ResponsiveDensities& densities);
  static GoogleString ToString(const protobuf::MessageLite& proto);
  static GoogleString ToString(const AllowVaryOn& allow_vary_on);

  // Returns true if p1's option_name is less than p2's. Used to order
  // all_properties_ and all_options_.
  static bool PropertyLessThanByOptionName(PropertyBase* p1, PropertyBase* p2) {
    return StringCaseCompare(p1->option_name(), p2->option_name()) < 0;
  }

  // Returns true if option's name is less than arg.
  static bool OptionNameLessThanArg(OptionBase* option, StringPiece arg) {
    return StringCaseCompare(option->option_name(), arg) < 0;
  }

  // Returns true if e1's timestamp is less than e2's.
  static bool CompareUrlCacheInvalidationEntry(UrlCacheInvalidationEntry* e1,
                                               UrlCacheInvalidationEntry* e2) {
    return e1->timestamp_ms < e2->timestamp_ms;
  }

  // Returns true if the first entry's id is less than the second's id.
  static bool FilterEnumToIdAndNameEntryLessThanById(
      const FilterEnumToIdAndNameEntry* e1,
      const FilterEnumToIdAndNameEntry* e2) {
    return strcmp(e1->filter_id, e2->filter_id) < 0;
  }

  // Return the effective option name. If the name is deprecated, the new name
  // will be returned; otherwise the name will be returned as is.
  static StringPiece GetEffectiveOptionName(StringPiece name);

  // Returns value of this option if it has been set; otherwise, returns true
  // if the rewrite level matches the argument. If nothing applies, returns
  // false.
  bool CheckLevelSpecificOption(RewriteLevel rewrite_level,
                                const Option<bool>& option) const;

  // In OptimizeForBandwidth mode, this sets up certain default filters
  // and options, which take effect only if not explicitly overridden.
  bool CheckBandwidthOption(const Option<bool>& option) const {
    return CheckLevelSpecificOption(kOptimizeForBandwidth, option);
  }

  // In MobilizeFilters mode, this sets up certain default filters and options,
  // which take effect only if not explicitly overridden.
  bool CheckMobilizeFiltersOption(const Option<bool>& option) const {
    return CheckLevelSpecificOption(kMobilizeFilters, option);
  }

  // Helps resolve the conflict between explicit extend_cache filters and
  // preserve settings, but does not perform the action itself.  This is so
  // that the standard option-merging and filter-merging that works over
  // all filters can function.  This function is called prior to merging
  // options and filters.  The resulting value is then passed to
  // ApplyMergeOverride, which must be run after the filter/option merging.
  MergeOverride ComputeMergeOverride(
      Filter filter,
      const Option<bool>& src_preserve_option,
      const Option<bool>& preserve_option,
      const RewriteOptions& src);

  // Applies the results of ComputeMergeOverride.
  void ApplyMergeOverride(
      MergeOverride merge_override,
      Filter filter,
      Option<bool>* preserve_option);

  bool modified_;
  bool frozen_;
  FilterSet enabled_filters_;
  FilterSet disabled_filters_;
  FilterSet forbidden_filters_;

  // Note: using the template class Option here saves a lot of repeated
  // and error-prone merging code.  However, it is not space efficient as
  // we are alternating int64s and bools in the structure.  If we cared
  // about that, then we would keep the bools in a bitmask.  But since
  // we don't really care we'll try to keep the code structured better.
  Option<RewriteLevel> level_;

  // List of URL wildcard patterns and timestamp for which it should be
  // invalidated.  In increasing order of timestamp.
  UrlCacheInvalidationEntryVector url_cache_invalidation_entries_;

  // Map of exact URLs to be invalidated; no wildcards.  Note that the
  // cache_purge_mutex_ is, by default, a NullRWLock.  You must call
  // set_cache_invalidation_timestamp_mutex to make it be a real mutex.
  // This is generally done only for the global context for each server,
  // so that we can atomically propagate cache flush updates into it while
  // it's running.
  CopyOnWrite<PurgeSet> purge_set_ GUARDED_BY(cache_purge_mutex_);

  scoped_ptr<ThreadSystem::RWLock> cache_purge_mutex_;
  Option<int64> css_flatten_max_bytes_;
  Option<bool> cache_small_images_unrewritten_;
  Option<bool> no_transform_optimized_images_;

  // Sets limit for image optimization
  Option<int64> image_resolution_limit_bytes_;
  Option<int64> css_image_inline_max_bytes_;
  Option<int64> css_inline_max_bytes_;
  Option<int64> css_outline_min_bytes_;
  Option<int64> google_font_css_inline_max_bytes_;

  // Preserve URL options
  Option<bool> css_preserve_urls_;
  Option<bool> js_preserve_urls_;
  Option<bool> image_preserve_urls_;

  Option<int64> image_inline_max_bytes_;
  Option<int64> js_inline_max_bytes_;
  Option<int64> js_outline_min_bytes_;
  Option<int64> progressive_jpeg_min_bytes_;
  // The max Cache-Control TTL for HTML.
  Option<int64> max_html_cache_time_ms_;
  // The maximum number of bytes of HTML that we parse, before redirecting to
  // ?ModPagespeed=off.
  Option<int64> max_html_parse_bytes_;
  // Resources with Cache-Control TTL less than this will not be rewritten.
  Option<int64> min_resource_cache_time_to_rewrite_ms_;
  Option<int64> idle_flush_time_ms_;
  Option<int64> flush_buffer_limit_bytes_;

  // How long to wait in blocking fetches before timing out.
  // Applies to ResourceFetch::BlockingFetch() and class SyncFetcherAdapter.
  // Does not apply to async fetches.
  Option<int64> blocking_fetch_timeout_ms_;

  // Option related to generic image quality. This is overridden by
  // image(jpeg/webp) specific options.
  Option<int64> image_recompress_quality_;

  // Options related to jpeg compression.
  Option<int64> image_jpeg_recompress_quality_;
  Option<int64> image_jpeg_recompress_quality_for_small_screens_;
  Option<int64> image_jpeg_quality_for_save_data_;
  Option<int64> image_jpeg_num_progressive_scans_;
  Option<int64> image_jpeg_num_progressive_scans_for_small_screens_;

  // Options governing when to retain optimized images vs keep original
  Option<int> image_limit_optimized_percent_;
  Option<int> image_limit_resize_area_percent_;
  Option<int> image_limit_rendered_area_percent_;

  // Options related to webp compression.
  Option<int64> image_webp_recompress_quality_;
  Option<int64> image_webp_recompress_quality_for_small_screens_;
  Option<int64> image_webp_animated_recompress_quality_;
  Option<int64> image_webp_quality_for_save_data_;
  Option<int64> image_webp_timeout_ms_;

  Option<int> image_max_rewrites_at_once_;
  Option<int> max_url_segment_size_;  // For http://a/b/c.d, use strlen("c.d").
  Option<int> max_url_size_;          // This is strlen("http://a/b/c.d").
  // The interval to wait for async rewrites to complete before flushing
  // content.  This deadline is per flush.
  Option<int> rewrite_deadline_ms_;
  // Maximum number of shards for rewritten resources in a directory.
  Option<int> domain_shard_count_;

  Option<EnabledEnum> enabled_;

  // Encode relevant rewrite options as URL query-parameters so that resources
  // can be reconstructed on servers without the same configuration file.
  Option<bool> add_options_to_urls_;

  // If this option is enabled, serves .pagespeed. resource URLs with
  // mismatching hashes with the same cache expiration as the inputs.
  // By default, we convert resources requests with the wrong hash to
  // Cache-Control:private,max-age=300 to avoid caching stale content
  // in proxies.
  Option<bool> publicly_cache_mismatched_hashes_experimental_;

  // Should in-place-resource-optimization(IPRO) be enabled?
  Option<bool> in_place_rewriting_enabled_;
  // Optimize before responding in in-place flow?
  Option<bool> in_place_wait_for_optimized_;
  // Interval to delay serving on the IPRO path while waiting for optimizations.
  // After this interval, the unoptimized resource will be served.
  Option<int> in_place_rewrite_deadline_ms_;
  // When we have a resource that we haven't optimized in-place yet, we add
  // s-maxage to the Cache-Control header until we get to optimizing it.  This
  // option controls how many seconds we set s-maxage to, and -1 disables
  // setting s-maxage at all.
  Option<int> in_place_s_maxage_sec_;
  // If set, preemptively rewrite images in CSS files on the HTML serving path
  // when IPRO of CSS is enabled.
  Option<bool> in_place_preemptive_rewrite_css_;
  // If set, preemptively rewrite images in CSS files on the IPRO serving path.
  Option<bool> in_place_preemptive_rewrite_css_images_;
  // If set, preemptively rewrite images in image files on the HTML serving path
  // when IPRO of images is enabled.
  Option<bool> in_place_preemptive_rewrite_images_;
  // If set, preemptively rewrite images in JS files on the HTML serving path
  // when IPRO of JS is enabled.
  Option<bool> in_place_preemptive_rewrite_javascript_;
  // Use cache-control:private rather than vary:accept when serving IPRO
  // resources to IE.  This avoids the need for an if-modified-since request
  // from IE on each cache hit.  The flip side is no proxy cache will store it
  // (though few or no proxy caches will store Vary: accept data in any case
  // unless they are specially configured to do so).
  Option<bool> private_not_vary_for_ie_;
  Option<bool> combine_across_paths_;
  Option<bool> log_background_rewrites_;
  Option<bool> log_mobilization_samples_;
  Option<bool> log_rewrite_timing_;   // Should we time HtmlParser?
  Option<bool> log_url_indices_;
  Option<bool> lowercase_html_names_;
  Option<bool> always_rewrite_css_;  // For tests/debugging.
  Option<bool> respect_vary_;
  Option<bool> respect_x_forwarded_proto_;
  Option<bool> flush_html_;
  // If set to true, ProxyFetch will request a flush on its RewriteDriver when
  // Flush() is called on it.
  Option<bool> follow_flushes_;
  // Should we serve stale responses if the fetch results in a server side
  // error.
  Option<bool> serve_stale_if_fetch_error_;
  // Should we serve access control headers in response headers.
  Option<bool> serve_xhr_access_control_headers_;
  // Proactively freshen user facing request if it is about to expire. So, that
  // subsequent requests will experience a cache hit.
  Option<bool> proactively_freshen_user_facing_request_;
  // Threshold for serving stale responses while revalidating in background.
  // 0 means don't serve stale content.
  Option<int64> serve_stale_while_revalidate_threshold_sec_;

  // When default_cache_html_ is false (default) we do not cache
  // input HTML which lacks Cache-Control headers. But, when set true,
  // we will cache those inputs for the implicit lifetime just like we
  // do for resources.
  Option<bool> default_cache_html_;
  // In general, we rewrite Cache-Control headers for HTML. We do this
  // for several reasons, but at least one is that our rewrites are not
  // necessarily publicly cacheable.
  // Some people don't like this, so we allow them to disable it.
  Option<bool> modify_caching_headers_;
  // In general, lazyload images loads images on scroll. However, some people
  // may want to load images when the onload event is fired instead. If set to
  // true, images are loaded when onload is fired.
  Option<bool> lazyload_images_after_onload_;
  // The initial image url to load in the lazyload images filter. If this is not
  // specified, we use a 1x1 inlined image.
  Option<GoogleString> lazyload_images_blank_url_;
  // Whether inline preview should use a blank image instead of a low resolution
  // version of the original image.
  Option<bool> use_blank_image_for_inline_preview_;
  // By default, inline_images will inline only critical images. However, some
  // people may want to inline all images (both critical and non-critical). If
  // set to false, all images will be inlined within the html.
  Option<bool> inline_only_critical_images_;
  // Indicates whether image rewriting filters should insert the critical images
  // beacon code.
  Option<bool> critical_images_beacon_enabled_;
  // Indicates whether the DomainRewriteFilter should also do client side
  // rewriting.
  Option<bool> client_domain_rewrite_;
  // Indicates whether DomainRewriteFilter should rewrite domain information
  // in Set-Cookie: headers.
  Option<bool> domain_rewrite_cookies_;
  // Indicates whether the DomainRewriteFilter should rewrite all tags,
  // including <a href> and <form action>.
  Option<bool> domain_rewrite_hyperlinks_;
  // Are we running the A/B experiment framework that uses cookies and Google
  // Analytics to track page speed statistics with multiple sets of rewriters?
  Option<bool> running_experiment_;
  // The experiment framework reports to Google Analytice in a custom variable
  // slot.  Specify which one to use.
  Option<int> experiment_ga_slot_;
  // For testing purposes you can force users to be enrolled in a specific
  // experiment.  This makes most sense in a query param.
  Option<int> enroll_experiment_id_;

  // When running a content experiment, which IDs should we use when logging to
  // Google Analytics?
  Option<GoogleString> content_experiment_id_;
  Option<GoogleString> content_experiment_variant_id_;

  // Log to analytics.js instead of ga.js.
  Option<bool> use_analytics_js_;

  // Increase the percentage of hits to 10% (current max) that have
  // site speed tracking in Google Analytics.
  Option<bool> increase_speed_tracking_;

  // If enabled we will report time taken before navigating to a new page. This
  // won't have effect, if onload beacon is sent before unload event is
  // trigggered.
  Option<bool> report_unload_time_;

  Option<bool> serve_rewritten_webp_urls_to_any_agent_;

  // Enables experimental code in defer js.
  Option<bool> enable_defer_js_experimental_;

  // Option to disable rewrite optimizations on no-transform header.
  Option<bool> disable_rewrite_on_no_transform_;

  // Option to disable pre-emptive background fetches for bot requests.
  Option<bool> disable_background_fetches_for_bots_;

  // Enables the Cache Purge API.  This is not on by default because
  // it requires saving input URLs to each metadata cache entry to
  // facilitate fast URL cache invalidation.
  //
  // Note that in the absence of this API, purging URLs can still
  // work, but it will invalidate either the entire metadata cache
  // (ignores_metadata_and_pcache==false in the call to
  // AddUrlCacheInvalidationEntry) or will not invalidate the metadata
  // cache entries at all (ignores_metadata_and_pcache==false).
  Option<bool> enable_cache_purge_;

  // If set, the urls of the inputs to the resource are saved in the metadata
  // cache entry. This increases the size of the cache entry, but can be used in
  // freshening of the embedded resources.
  Option<bool> proactive_resource_freshening_;

  // Enables the code to lazy load high res images.
  Option<bool> lazyload_highres_images_;

  // Some introspective javascript is very brittle and may break if we
  // make any changes.  Enables code to detect such cases and avoid renaming.
  Option<bool> avoid_renaming_introspective_javascript_;

  // Overrides the IE document mode to use the highest mode available.
  Option<bool> override_ie_document_mode_;

  // Test-only flag to get fetch deadlines to trigger instantly.
  Option<bool> test_instant_fetch_rewrite_deadline_;

  // Indicates whether the prioritize_critical_css filter should invoke its
  // JavaScript function to load all the "hidden" CSS files at onload.
  // Intended for testing only.
  Option<bool> test_only_prioritize_critical_css_dont_apply_original_css_;

  // Enables blocking rewrite of html. RewriteDriver provides a flag
  // fully_rewrite_on_flush which makes sure that all rewrites are done before
  // the response is flushed to the client. If the value of the
  // X-PSA-Blocking-Rewrite header matches this key, the
  // RewriteDriver::fully_rewrite_on_flush flag will be set.
  Option<GoogleString> blocking_rewrite_key_;

  // Indicates how often we should reinstrument pages with the critical images
  // beacon, based on the time since the last write to the property cache by a
  // beacon response.
  Option<int> beacon_reinstrument_time_sec_;

  // Number of first N images for which low res image is generated. Negative
  // values will bypass image index check.
  Option<int> max_inlined_preview_images_index_;
  // Minimum image size above which low res image is generated.
  Option<int64> min_image_size_low_resolution_bytes_;
  // Maximum image size below which low res image is generated.
  Option<int64> max_image_size_low_resolution_bytes_;
  // Percentage (an integer between 0 and 100 inclusive) of images rewrites to
  // drop.
  Option<int> rewrite_random_drop_percentage_;

  // For proxies operating in in-place mode this allows fetching optimized
  // resources from sites that have MPS, etc configured.
  Option<bool> oblivious_pagespeed_urls_;

  // Cache expiration time in msec for properties of finders.
  // Critical images /flush early information will be valid for the time
  // specified.
  Option<int64> finder_properties_cache_expiration_time_ms_;

  // Cache refresh time in msec for properties of finders. The properties are
  // refreshed when their age is larger than the specified value. However, the
  // property will be used until finder_properties_cache_expiration_time_ms_.
  Option<int64> finder_properties_cache_refresh_time_ms_;
  // Duration after which the experiment cookie will expire on the user's
  // browser (in msec).
  Option<int64> experiment_cookie_duration_ms_;

  // The maximum time beyond expiry for which a metadata cache entry may be
  // used.
  Option<int64> metadata_cache_staleness_threshold_ms_;

  // The metadata cache ttl for input resources which are 4xx errors.
  Option<int64> metadata_input_errors_cache_ttl_ms_;

  // The HTTP method to use ("PURGE", "GET" etc.) for purge requests sent to
  // downstream caches (e.g. proxy_cache, Varnish).
  Option<GoogleString> downstream_cache_purge_method_;

  // The host:port/path prefix to be used for purging the cached responses.
  Option<GoogleString> downstream_cache_purge_location_prefix_;

  // The webmaster-provided key used to authenticate rebeaconing requests from
  // downstream caches.
  Option<GoogleString> downstream_cache_rebeaconing_key_;

  // Threshold for amount of rewriting finished before the response was served
  // out (expressed as a percentage) and simultaneously stored in the downstream
  // cache  beyond which the response will not be purged from the cache even if
  // more rewriting is possible now. If the threshold is exceeded, this means
  // that the version in the cache is good enough and hence need not be purged.
  Option<int64> downstream_cache_rewritten_percentage_threshold_;

  // The number of milliseconds of cache TTL we assign to resources that
  // are "likely cacheable" (e.g. images, js, css, not html) and have no
  // explicit cache ttl or expiration date.
  Option<int64> implicit_cache_ttl_ms_;

  // The number of miliseconds of cache TTL we assign to resources that are
  // loaded from file and "likely cacheable" and have no explicit cache ttl or
  // expiration date. If this option is not set explicitly, fall back to using
  // implicit_cache_ttl_ms for load from file cache ttl.
  Option<int64> load_from_file_cache_ttl_ms_;

  // Maximum length (in bytes) of response content.
  Option<int64> max_cacheable_response_content_length_;

  // Keep the original subresource hints
  Option<bool> preserve_subresource_hints_;

  // Keep rewritten URLs as relative as the original resource URL was.
  // TODO(sligocki): Remove this option once we know it's always safe.
  Option<bool> preserve_url_relativity_;

  Option<GoogleString> ga_id_;

  // Use fallback values from property cache.
  Option<bool> use_fallback_property_cache_values_;
  // Always wait for property cache lookup to finish.
  Option<bool> await_pcache_lookup_;
  // Enable Prioritizing of scripts in defer javascript.
  Option<bool> enable_prioritizing_scripts_;
  // Enables rewriting of uncacheable resources.
  Option<bool> rewrite_uncacheable_resources_;
  // Forbid turning on of any disabled (not enabled) filters either via query
  // parameters or request headers or .htaccess for Directory. Note that this
  // is a latch so that setting it at some directory level forces it on for
  // that and all lower levels, as otherwise someone could just create a
  // sub-directory and enable it in a .htaccess in there.
  Option<bool> forbid_all_disabled_filters_;
  // Enables aggressive rewriters for mobile user agents.
  Option<bool> enable_aggressive_rewriters_for_mobile_;

  // If this is true (it defaults to false) ProxyInterface frontend will
  // reject requests where PSA is not enabled or URL is blacklisted with
  // status code reject_blacklisted_status_code_ (default 403) rather than
  // proxy them in passthrough mode. This does not affect behavior for
  // resource rewriting.
  Option<bool> reject_blacklisted_;
  Option<int> reject_blacklisted_status_code_;

  // Support handling of clients without javascript support.  This is applicable
  // only if any filter that inserts new javascript (e.g., lazyload_images) is
  // enabled.
  Option<bool> support_noscript_enabled_;

  // If this set to true, we add additional instrumentation code to page that
  // reports more information in the beacon.
  Option<bool> enable_extended_instrumentation_;

  Option<bool> use_experimental_js_minifier_;

  // Maximum size allowed for the combined CSS resource.
  // Negative value will bypass the size check.
  Option<int64> max_combined_css_bytes_;

  // Maximum size allowed for the combined js resource.
  // Negative value will bypass the size check.
  Option<int64> max_combined_js_bytes_;

  // Url to which pre connect requests will be sent.
  Option<GoogleString> pre_connect_url_;
  // The number of requests for which the status code should remain same so that
  // we consider it to be stable.
  Option<int> property_cache_http_status_stability_threshold_;
  // The maximum number of rewrite info logs stored for a single request.
  Option<int> max_rewrite_info_log_size_;

  // The cache TTL with which to override the urls matching the
  // override_caching_ WildCardGroup. Note that we do not override the cache TTL
  // for any urls if this value is negative. The same TTL value is used for all
  // urls that match override_caching_wildcard_.
  Option<int64> override_caching_ttl_ms_;
  CopyOnWrite<FastWildcardGroup> override_caching_wildcard_;

  // Whether to allow logging urls as part of LogRecord.
  Option<bool> allow_logging_urls_in_log_record_;

  // Whether to allow options to be set by cookies.
  Option<bool> allow_options_to_be_set_by_cookies_;

  // Non cacheables used when partial HTML is cached.
  Option<GoogleString> non_cacheables_for_cache_partial_html_;

  // Comma separated list of origins that are allowed to make cross-origin
  // requests. These domain requests are served with
  // Access-Control-Allow-Origin header.
  Option<GoogleString> access_control_allow_origins_;

  // If set to true, hides the referer by adding meta tag to the HTML.
  Option<bool> hide_referer_using_meta_;

  // Options to control the edge-case behaviour of inline-previewed images. The
  // idea is to avoid inline-previewing when:
  // a. low-res image is large.
  // b. low-res image is not small enough compared to the full-res version.
  Option<int64> max_low_res_image_size_bytes_;
  Option<int> max_low_res_to_full_res_image_size_percentage_;

  // The URL from which to pull remote configurations.
  Option<GoogleString> remote_configuration_url_;
  // The timeout, in milliseconds for the remote configuration file fetch.
  Option<int64> remote_configuration_timeout_ms_;

  // The level to set the gzip compression of HTTPCache items.
  Option<int> http_cache_compression_level_;

  // Pass this string in url to allow for pagespeed options.
  Option<GoogleString> request_option_override_;

  // The key used to sign .pagespeed resources if URL signing is enabled.
  Option<GoogleString> url_signing_key_;

  // If set to true, accepts urls with invalid signatures.
  Option<bool> accept_invalid_signatures_;

  // sticky_query_parameters_ is the token specified in the configuration that
  // must be specified in a request's query parameters/headers for the other
  // options in the request to be converted to cookies.
  // option_cookies_duration_ms_ is how long the cookie will live for when set.
  Option<GoogleString> sticky_query_parameters_;
  Option<int64> option_cookies_duration_ms_;

  // Comma separated list of densities to use for responsive images.
  Option<ResponsiveDensities> responsive_image_densities_;

  // The pattern to use for generating the canonical AMP page link from
  // the existing URL.
  // TODO(sjnickerson): Make this Option<AmpLinkPattern> so that parsing and
  // validation can happen up front.
  Option<GoogleString> amp_link_pattern_;

  // Whether our CSP support is on or not.
  Option<bool> honor_csp_;

  // If set, how to fragment the http cache.  Otherwise the server's hostname,
  // from the Host header, is used.
  CacheFragmentOption cache_fragment_;

  // Be sure to update constructor when new fields are added so that they are
  // added to all_options_, which is used for Merge, and eventually, Compare.
  OptionBaseVector all_options_;
  size_t initialized_options_;  // Counts number of options initialized so far.

  // Reverse map from filter id string to corresponding Filter enum.  Note
  // that this is not indexed by filter enum; it's indexed alphabetically by id.
  static const FilterEnumToIdAndNameEntry* filter_id_to_enum_array_[
      kEndOfFilters];

  // Reverse map from option name string to corresponding PropertyBase.
  static PropertyNameMap* option_name_to_property_map_;

  // Reverse map from option id string to corresponding PropertyBase.
  static const PropertyBase** option_id_to_property_array_;

  // When compiled for debug, we lazily check whether the all the Option<>
  // member variables in all_options have unique IDs.
  //
  // Note that we include this member-variable in the structrue even under
  // optimization as otherwise it might be very bad news indeed if someone
  // mixed debug/opt object files in an executable.
  bool options_uniqueness_checked_;

  bool need_to_store_experiment_data_;
  int experiment_id_;  // Which experiment configuration are we in?
  int experiment_percent_;  // Total traffic going through experiments.
  std::vector<ExperimentSpec*> experiment_specs_;

  // Headers to add resource responses.
  // TODO(oschaaf): Wrap this and custom_fetch_headers_ in CopyOnWrite through
  // a new class. Move header validations over there as well so we can share
  // them.
  std::vector<NameValue*> resource_headers_;

  // Headers to add to subresource requests.
  std::vector<NameValue*> custom_fetch_headers_;

  // If this is non-NULL it tells us additional attributes that should be
  // interpreted as containing urls.
  scoped_ptr<std::vector<ElementAttributeCategory> > url_valued_attributes_;

  Option<ResourceCategorySet> inline_unauthorized_resource_types_;

  Option<int64> noop_;

  // Comma separated list of headers which we can vary-on, or "Auto", or "None".
  Option<AllowVaryOn> allow_vary_on_;

  CopyOnWrite<JavascriptLibraryIdentification>
      javascript_library_identification_;

  CopyOnWrite<DomainLawyer> domain_lawyer_;
  FileLoadPolicy file_load_policy_;

  CopyOnWrite<FastWildcardGroup> allow_resources_;
  CopyOnWrite<FastWildcardGroup> allow_when_inlining_resources_;
  CopyOnWrite<FastWildcardGroup> retain_comments_;
  CopyOnWrite<FastWildcardGroup> lazyload_enabled_classes_;
  CopyOnWrite<FastWildcardGroup> css_combining_permitted_ids_;

  // When certain url patterns are in the referer we want to do a blocking
  // rewrite.
  CopyOnWrite<FastWildcardGroup> blocking_rewrite_referer_urls_;

  // Using StringPiece here is safe since all entries in this map have static
  // strings as the key.
  typedef std::map<StringPiece, FastWildcardGroup*> FastWildcardGroupMap;
  FastWildcardGroupMap rejected_request_map_;

  GoogleString signature_;
  MD5Hasher hasher_;  // Used to compute named signatures.
  SHA1Signature sha1signature_;

  ThreadSystem* thread_system_;

  // When compiled for debug, keep track of the last thread to modify
  // this object.  If cloned or merged from prior to
  // ComputeSignature(), we check that the thread doing the Merge is
  // in the same thread that modified it.
  //
  // We also ensure that only one thread modifies a RewriteOptions
  // object.
  //
  // Note: we don't ifdef the member variable declaration as having the
  // structure-size dependent on debug-ifdef seems dangerous when used
  // as a library against externally compiled code.  We do ifdef its
  // usage within the class implementation, however.
  scoped_ptr<ThreadSystem::ThreadId> last_thread_id_;

  DISALLOW_COPY_AND_ASSIGN(RewriteOptions);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_REWRITE_OPTIONS_H_
