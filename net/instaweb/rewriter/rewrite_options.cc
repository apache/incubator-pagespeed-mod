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

#include "net/instaweb/rewriter/public/rewrite_options.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <set>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/dynamic_annotations.h"  // RunningOnValgrind
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_rw_lock.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/purge_set.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

// Option names.
// TODO(matterbury): Evaluate these filters to check which ones aren't global,
// rather are (say) Apache specific, and move them out.
// TODO(jmarantz): Use consistent naming from semantic_type.h for all option
// names that reference css/styles/js/scripts etc. such as CssPreserveUrls.
const char RewriteOptions::kAddOptionsToUrls[] = "AddOptionsToUrls";
const char RewriteOptions::kAcceptInvalidSignatures[] =
    "AcceptInvalidSignatures";
const char RewriteOptions::kAccessControlAllowOrigins[] =
    "AccessControlAllowOrigins";
const char RewriteOptions::kAllowLoggingUrlsInLogRecord[] =
    "AllowLoggingUrlsInLogRecord";
const char RewriteOptions::kAllowOptionsToBeSetByCookies[] =
    "AllowOptionsToBeSetByCookies";
const char RewriteOptions::kAlwaysRewriteCss[] = "AlwaysRewriteCss";
const char RewriteOptions::kAmpLinkPattern[] = "AmpLinkPattern";
const char RewriteOptions::kAnalyticsID[] = "AnalyticsID";
const char RewriteOptions::kAvoidRenamingIntrospectiveJavascript[] =
    "AvoidRenamingIntrospectiveJavascript";
const char RewriteOptions::kAwaitPcacheLookup[] = "AwaitPcacheLookup";
const char RewriteOptions::kBeaconReinstrumentTimeSec[] =
    "BeaconReinstrumentTimeSec";
const char RewriteOptions::kBeaconUrl[] = "BeaconUrl";
const char RewriteOptions::kCacheFragment[] = "CacheFragment";
const char RewriteOptions::kCacheSmallImagesUnrewritten[] =
    "CacheSmallImagesUnrewritten";
const char RewriteOptions::kClientDomainRewrite[] = "ClientDomainRewrite";
const char RewriteOptions::kCombineAcrossPaths[] = "CombineAcrossPaths";
const char RewriteOptions::kCompressMetadataCache[] = "CompressMetadataCache";
const char RewriteOptions::kContentExperimentID[] = "ContentExperimentID";
const char RewriteOptions::kContentExperimentVariantID[] =
    "ContentExperimentVariantID";
const char RewriteOptions::kCriticalImagesBeaconEnabled[] =
    "CriticalImagesBeaconEnabled";
const char RewriteOptions::kCssFlattenMaxBytes[] = "CssFlattenMaxBytes";
const char RewriteOptions::kCssImageInlineMaxBytes[] = "CssImageInlineMaxBytes";
const char RewriteOptions::kCssInlineMaxBytes[] = "CssInlineMaxBytes";
const char RewriteOptions::kCssOutlineMinBytes[] = "CssOutlineMinBytes";
const char RewriteOptions::kCssPreserveURLs[] = "CssPreserveURLs";
const char RewriteOptions::kDefaultCacheHtml[] = "DefaultCacheHtml";
const char RewriteOptions::kDisableRewriteOnNoTransform[] =
    "DisableRewriteOnNoTransform";
const char RewriteOptions::kDisableBackgroundFetchesForBots[] =
    "DisableBackgroundFetchesForBots";
const char RewriteOptions::kDomainRewriteCookies[] = "DomainRewriteCookies";
const char RewriteOptions::kDomainRewriteHyperlinks[] =
    "DomainRewriteHyperlinks";
const char RewriteOptions::kDomainShardCount[] = "DomainShardCount";
const char RewriteOptions::kDownstreamCachePurgeMethod[] =
    "DownstreamCachePurgeMethod";
const char RewriteOptions::kDownstreamCacheRebeaconingKey[] =
    "DownstreamCacheRebeaconingKey";
const char RewriteOptions::kDownstreamCacheRewrittenPercentageThreshold[] =
    "DownstreamCacheRewrittenPercentageThreshold";
const char RewriteOptions::kEnableAggressiveRewritersForMobile[] =
    "EnableAggressiveRewritersForMobile";
const char RewriteOptions::kEnableDeferJsExperimental[] =
    "EnableDeferJsExperimental";
const char RewriteOptions::kEnableCachePurge[] = "EnableCachePurge";
const char RewriteOptions::kEnableExtendedInstrumentation[] =
    "EnableExtendedInstrumentation";
const char RewriteOptions::kEnableLazyLoadHighResImages[] =
    "EnableLazyLoadHighResImages";
const char RewriteOptions::kEnablePrioritizingScripts[] =
    "EnablePrioritizingScripts";
const char RewriteOptions::kEnabled[] = "EnableRewriting";
const char RewriteOptions::kEnrollExperiment[] = "EnrollExperiment";
const char RewriteOptions::kExperimentCookieDurationMs[] =
    "ExperimentCookieDurationMs";
const char RewriteOptions::kExperimentSlot[] = "ExperimentSlot";
const char RewriteOptions::kFetcherProxy[] = "FetchProxy";
const char RewriteOptions::kFinderPropertiesCacheExpirationTimeMs[] =
    "FinderPropertiesCacheExpirationTimeMs";
const char RewriteOptions::kFinderPropertiesCacheRefreshTimeMs[] =
    "FinderPropertiesCacheRefreshTimeMs";
const char RewriteOptions::kFlushBufferLimitBytes[] = "FlushBufferLimitBytes";
const char RewriteOptions::kFlushHtml[] = "FlushHtml";
const char RewriteOptions::kFollowFlushes[] = "FollowFlushes";
const char RewriteOptions::kForbidAllDisabledFilters[] =
    "ForbidAllDisabledFilters";
const char RewriteOptions::kGoogleFontCssInlineMaxBytes[] =
    "GoogleFontCssInlineMaxBytes";
const char RewriteOptions::kHideRefererUsingMeta[] = "HideRefererUsingMeta";
const char RewriteOptions::kHttpCacheCompressionLevel[] =
    "HttpCacheCompressionLevel";
const char RewriteOptions::kHonorCsp[] = "HonorCsp";
const char RewriteOptions::kIdleFlushTimeMs[] = "IdleFlushTimeMs";
const char RewriteOptions::kImageInlineMaxBytes[] = "ImageInlineMaxBytes";
const char RewriteOptions::kImageJpegNumProgressiveScans[] =
    "ImageJpegNumProgressiveScans";
const char RewriteOptions::kImageJpegNumProgressiveScansForSmallScreens[] =
    "ImageJpegNumProgressiveScansForSmallScreens";
const char RewriteOptions::kImageJpegRecompressionQuality[] =
    "JpegRecompressionQuality";
const char RewriteOptions::kImageJpegRecompressionQualityForSmallScreens[] =
    "JpegRecompressionQualityForSmallScreens";
const char RewriteOptions::kImageJpegQualityForSaveData[] =
    "JpegQualityForSaveData";
const char RewriteOptions::kImageLimitOptimizedPercent[] =
    "ImageLimitOptimizedPercent";
const char RewriteOptions::kImageLimitRenderedAreaPercent[] =
    "ImageLimitRenderedAreaPercent";
const char RewriteOptions::kImageLimitResizeAreaPercent[] =
    "ImageLimitResizeAreaPercent";
const char RewriteOptions::kImageMaxRewritesAtOnce[] = "ImageMaxRewritesAtOnce";
const char RewriteOptions::kImagePreserveURLs[] = "ImagePreserveURLs";
const char RewriteOptions::kImageRecompressionQuality[] =
    "ImageRecompressionQuality";
const char RewriteOptions::kImageResolutionLimitBytes[] =
    "ImageResolutionLimitBytes";
const char RewriteOptions::kImageWebpRecompressionQuality[] =
    "WebpRecompressionQuality";
const char RewriteOptions::kImageWebpRecompressionQualityForSmallScreens[] =
    "WebpRecompressionQualityForSmallScreens";
const char RewriteOptions::kImageWebpAnimatedRecompressionQuality[] =
    "WebpAnimatedRecompressionQuality";
const char RewriteOptions::kImageWebpQualityForSaveData[] =
    "WebpQualityForSaveData";
const char RewriteOptions::kImageWebpTimeoutMs[] = "WebpTimeoutMs";
const char RewriteOptions::kImplicitCacheTtlMs[] = "ImplicitCacheTtlMs";
const char RewriteOptions::kInPlaceResourceOptimization[] =
    "InPlaceResourceOptimization";
const char RewriteOptions::kInPlaceSMaxAgeSec[] = "InPlaceSMaxAgeSec";
const char RewriteOptions::kInPlaceWaitForOptimized[] =
    "InPlaceWaitForOptimized";
const char RewriteOptions::kInPlacePreemptiveRewriteCss[] =
    "InPlacePreemptiveRewriteCss";
const char RewriteOptions::kInPlacePreemptiveRewriteCssImages[] =
    "InPlacePreemptiveRewriteCssImages";
const char RewriteOptions::kInPlacePreemptiveRewriteImages[] =
    "InPlacePreemptiveRewriteImages";
const char RewriteOptions::kInPlacePreemptiveRewriteJavascript[] =
    "InPlacePreemptiveRewriteJavascript";
const char RewriteOptions::kInPlaceRewriteDeadlineMs[] =
    "InPlaceRewriteDeadlineMs";
const char RewriteOptions::kIncreaseSpeedTracking[] = "IncreaseSpeedTracking";
const char RewriteOptions::kInlineOnlyCriticalImages[] =
    "InlineOnlyCriticalImages";
const char RewriteOptions::kJsInlineMaxBytes[] = "JsInlineMaxBytes";
const char RewriteOptions::kJsOutlineMinBytes[] = "JsOutlineMinBytes";
const char RewriteOptions::kJsPreserveURLs[] = "JsPreserveURLs";
const char RewriteOptions::kLazyloadImagesAfterOnload[] =
    "LazyloadImagesAfterOnload";
const char RewriteOptions::kLazyloadImagesBlankUrl[] = "LazyloadImagesBlankUrl";
const char RewriteOptions::kLoadFromFileCacheTtlMs[] = "LoadFromFileCacheTtlMs";
const char RewriteOptions::kLogBackgroundRewrite[] = "LogBackgroundRewrite";
const char RewriteOptions::kLogMobilizationSamples[] = "LogMobilizationSamples";
const char RewriteOptions::kLogRewriteTiming[] = "LogRewriteTiming";
const char RewriteOptions::kLogUrlIndices[] = "LogUrlIndices";
const char RewriteOptions::kLowercaseHtmlNames[] = "LowercaseHtmlNames";
const char RewriteOptions::kMaxCacheableResponseContentLength[] =
    "MaxCacheableContentLength";
const char RewriteOptions::kMaxCombinedCssBytes[] = "MaxCombinedCssBytes";
const char RewriteOptions::kMaxCombinedJsBytes[] = "MaxCombinedJsBytes";
const char RewriteOptions::kMaxHtmlCacheTimeMs[] = "MaxHtmlCacheTimeMs";
const char RewriteOptions::kMaxHtmlParseBytes[] = "MaxHtmlParseBytes";
const char RewriteOptions::kMaxImageSizeLowResolutionBytes[] =
    "MaxImageSizeLowResolutionBytes";
const char RewriteOptions::kMaxInlinedPreviewImagesIndex[] =
    "MaxInlinedPreviewImagesIndex";
const char RewriteOptions::kMaxLowResImageSizeBytes[] =
    "MaxLowResImageSizeBytes";
const char RewriteOptions::kMaxLowResToHighResImageSizePercentage[] =
    "MaxLowResToHighResImageSizePercentage";
const char RewriteOptions::kMaxRewriteInfoLogSize[] = "MaxRewriteInfoLogSize";
const char RewriteOptions::kMaxUrlSegmentSize[] = "MaxSegmentLength";
const char RewriteOptions::kMaxUrlSize[] = "MaxUrlSize";
const char RewriteOptions::kMetadataCacheStalenessThresholdMs[] =
    "MetadataCacheStalenessThresholdMs";
const char RewriteOptions::kMinImageSizeLowResolutionBytes[] =
    "MinImageSizeLowResolutionBytes";
const char RewriteOptions::kMinResourceCacheTimeToRewriteMs[] =
    "MinResourceCacheTimeToRewriteMs";
const char RewriteOptions::kModifyCachingHeaders[] = "ModifyCachingHeaders";
const char RewriteOptions::kNoop[] = "Noop";
const char RewriteOptions::kNoTransformOptimizedImages[] =
    "NoTransformOptimizedImages";
const char RewriteOptions::kNonCacheablesForCachePartialHtml[] =
    "NonCacheablesForCachePartialHtml";
const char RewriteOptions::kObliviousPagespeedUrls[] = "ObliviousPagespeedUrls";
const char RewriteOptions::kOptionCookiesDurationMs[] =
    "OptionCookiesDurationMs";
const char RewriteOptions::kOverrideCachingTtlMs[] = "OverrideCachingTtlMs";
const char RewriteOptions::kPreserveSubresourceHints[] =
    "PreserveSubresourceHints";
const char RewriteOptions::kPreserveUrlRelativity[] = "PreserveUrlRelativity";
const char RewriteOptions::kPrivateNotVaryForIE[] = "PrivateNotVaryForIE";
const char RewriteOptions::kPubliclyCacheMismatchedHashesExperimental[] =
    "PubliclyCacheMismatchedHashesExperimental";
const char RewriteOptions::kProactivelyFreshenUserFacingRequest[] =
    "ProactivelyFreshenUserFacingRequest";
const char RewriteOptions::kProactiveResourceFreshening[] =
    "ProactiveResourceFreshening";
const char RewriteOptions::kProgressiveJpegMinBytes[] =
    "ProgressiveJpegMinBytes";
const char RewriteOptions::kRejectBlacklisted[] = "RejectBlacklisted";
const char RewriteOptions::kRejectBlacklistedStatusCode[] =
    "RejectBlacklistedStatusCode";
const char RewriteOptions::kReportUnloadTime[] = "ReportUnloadTime";
const char RewriteOptions::kRespectVary[] = "RespectVary";
const char RewriteOptions::kRespectXForwardedProto[] = "RespectXForwardedProto";
const char RewriteOptions::kResponsiveImageDensities[] =
    "ResponsiveImageDensities";
const char RewriteOptions::kRewriteDeadlineMs[] = "RewriteDeadlinePerFlushMs";
const char RewriteOptions::kRewriteLevel[] = "RewriteLevel";
const char RewriteOptions::kRewriteRandomDropPercentage[] =
    "RewriteRandomDropPercentage";
const char RewriteOptions::kRewriteUncacheableResources[] =
    "RewriteUncacheableResources";
const char RewriteOptions::kRunningExperiment[] = "RunExperiment";
const char RewriteOptions::kServeStaleIfFetchError[] = "ServeStaleIfFetchError";
const char RewriteOptions::kServeStaleWhileRevalidateThresholdSec[] =
    "ServeStaleWhileRevalidateThresholdSec";
const char RewriteOptions::kServeXhrAccessControlHeaders[] =
    "ServeXhrAccessControlHeaders";
const char RewriteOptions::kStickyQueryParameters[] = "StickyQueryParameters";
const char RewriteOptions::kSupportNoScriptEnabled[] = "SupportNoScriptEnabled";
const char
    RewriteOptions::kTestOnlyPrioritizeCriticalCssDontApplyOriginalCss[] =
        "TestOnlyPrioritizeCriticalCssDontApplyOriginalCss";
const char RewriteOptions::kUseBlankImageForInlinePreview[] =
    "UseBlankImageForInlinePreview";
const char RewriteOptions::kUseExperimentalJsMinifier[] =
    "UseExperimentalJsMinifier";
const char RewriteOptions::kUseFallbackPropertyCacheValues[] =
    "UseFallbackPropertyCacheValues";
const char RewriteOptions::kUseImageScanlineApi[] = "UseImageScanlineApi";
const char RewriteOptions::kXModPagespeedHeaderValue[] = "XHeaderValue";
const char RewriteOptions::kXPsaBlockingRewrite[] = "BlockingRewriteKey";

const char RewriteOptions::kAllow[] = "Allow";
const char RewriteOptions::kAllowVaryOn[] = "AllowVaryOn";
const char RewriteOptions::kBlockingRewriteRefererUrls[] =
    "BlockingRewriteRefererUrls";
const char RewriteOptions::kDisableFilters[] = "DisableFilters";
const char RewriteOptions::kDisallow[] = "Disallow";
const char RewriteOptions::kDomain[] = "Domain";
const char RewriteOptions::kDownstreamCachePurgeLocationPrefix[] =
    "DownstreamCachePurgeLocationPrefix";
const char RewriteOptions::kEnableFilters[] = "EnableFilters";
const char RewriteOptions::kExperimentVariable[] = "ExperimentVariable";
const char RewriteOptions::kExperimentSpec[] = "ExperimentSpec";
const char RewriteOptions::kForbidFilters[] = "ForbidFilters";
const char RewriteOptions::kInlineResourcesWithoutExplicitAuthorization[] =
    "InlineResourcesWithoutExplicitAuthorization";
const char RewriteOptions::kRetainComment[] = "RetainComment";
const char RewriteOptions::kPermitIdsForCssCombining[] =
    "PermitIdsForCssCombining";
const char RewriteOptions::kAddResourceHeader[] = "AddResourceHeader";
const char RewriteOptions::kCustomFetchHeader[] = "CustomFetchHeader";
const char RewriteOptions::kLoadFromFile[] = "LoadFromFile";
const char RewriteOptions::kLoadFromFileMatch[] = "LoadFromFileMatch";
const char RewriteOptions::kLoadFromFileRule[] = "LoadFromFileRule";
const char RewriteOptions::kLoadFromFileRuleMatch[] = "LoadFromFileRuleMatch";
const char RewriteOptions::kMapOriginDomain[] = "MapOriginDomain";
const char RewriteOptions::kMapRewriteDomain[] = "MapRewriteDomain";
const char RewriteOptions::kMapProxyDomain[] = "MapProxyDomain";
const char RewriteOptions::kShardDomain[] = "ShardDomain";
const char RewriteOptions::kUrlValuedAttribute[] = "UrlValuedAttribute";
const char RewriteOptions::kLibrary[] = "Library";
const char RewriteOptions::kCacheFlushFilename[] = "CacheFlushFilename";
const char RewriteOptions::kCacheFlushPollIntervalSec[] =
    "CacheFlushPollIntervalSec";
const char RewriteOptions::kFetchHttps[] = "FetchHttps";
const char RewriteOptions::kFetcherTimeOutMs[] = "FetcherTimeOutMs";
const char RewriteOptions::kFileCacheCleanInodeLimit[] = "FileCacheInodeLimit";
const char RewriteOptions::kFileCacheCleanIntervalMs[] =
    "FileCacheCleanIntervalMs";
const char RewriteOptions::kFileCacheCleanSizeKb[] = "FileCacheSizeKb";
const char RewriteOptions::kFileCachePath[] = "FileCachePath";
const char RewriteOptions::kLogDir[] = "LogDir";
const char RewriteOptions::kLruCacheByteLimit[] = "LRUCacheByteLimit";
const char RewriteOptions::kLruCacheKbPerProcess[] = "LRUCacheKbPerProcess";
const char RewriteOptions::kMemcachedServers[] = "MemcachedServers";
const char RewriteOptions::kMemcachedThreads[] = "MemcachedThreads";
const char RewriteOptions::kMemcachedTimeoutUs[] = "MemcachedTimeoutUs";
const char RewriteOptions::kProxySuffix[] = "ProxySuffix";
const char RewriteOptions::kRateLimitBackgroundFetches[] =
    "RateLimitBackgroundFetches";
const char RewriteOptions::kRemoteConfigurationUrl[] = "RemoteConfigurationUrl";
const char RewriteOptions::kRemoteConfigurationTimeoutMs[] =
    "RemoteConfigurationTimeoutMs";
const char RewriteOptions::kRequestOptionOverride[] = "RequestOptionOverride";
const char RewriteOptions::kServeWebpToAnyAgent[] =
    "ServeRewrittenWebpUrlsToAnyAgent";
const char RewriteOptions::kSlurpDirectory[] = "SlurpDirectory";
const char RewriteOptions::kSlurpFlushLimit[] = "SlurpFlushLimit";
const char RewriteOptions::kSlurpReadOnly[] = "SlurpReadOnly";
const char RewriteOptions::kSslCertDirectory[] = "SslCertDirectory";
const char RewriteOptions::kSslCertFile[] = "SslCertFile";
const char RewriteOptions::kStatisticsEnabled[] = "Statistics";
const char RewriteOptions::kStatisticsLoggingChartsCSS[] =
    "StatisticsLoggingChartsCSS";
const char RewriteOptions::kStatisticsLoggingChartsJS[] =
    "StatisticsLoggingChartsJS";
const char RewriteOptions::kStatisticsLoggingEnabled[] = "StatisticsLogging";
const char RewriteOptions::kStatisticsLoggingIntervalMs[] =
    "StatisticsLoggingIntervalMs";
const char RewriteOptions::kStatisticsLoggingMaxFileSizeKb[] =
    "StatisticsLoggingMaxFileSizeKb";
const char RewriteOptions::kTestProxy[] = "TestProxy";
const char RewriteOptions::kTestProxySlurp[] = "TestProxySlurp";
const char RewriteOptions::kUrlSigningKey[] = "UrlSigningKey";
const char RewriteOptions::kUseAnalyticsJs[] = "UseAnalyticsJs";
const char RewriteOptions::kUseSharedMemLocking[] = "SharedMemoryLocks";
const char RewriteOptions::kNullOption[] = "";

// RewriteFilter prefixes
const char RewriteOptions::kCacheExtenderId[] = "ce";
const char RewriteOptions::kCssCombinerId[] = "cc";
const char RewriteOptions::kCssFilterId[] = "cf";
const char RewriteOptions::kCssImportFlattenerId[] = "if";
const char RewriteOptions::kCssInlineId[] = "ci";
const char RewriteOptions::kGoogleFontCssInlineId[] = "gf";
const char RewriteOptions::kImageCombineId[] = "is";
const char RewriteOptions::kImageCompressionId[] = "ic";
const char RewriteOptions::kInPlaceRewriteId[] = "aj";  // Comes from ajax.
const char RewriteOptions::kJavascriptCombinerId[] = "jc";
const char RewriteOptions::kJavascriptMinId[] = "jm";
const char RewriteOptions::kJavascriptMinSourceMapId[] = "sm";
const char RewriteOptions::kJavascriptInlineId[] = "ji";
const char RewriteOptions::kLocalStorageCacheId[] = "ls";
const char RewriteOptions::kPrioritizeCriticalCssId[] = "pr";

// TODO(jmarantz): consider merging this threshold with the image-inlining
// threshold, which is currently defaulting at 2000, so we have a single
// byte-count threshold, above which inlined resources get outlined, and
// below which outlined resources get inlined.
//
// TODO(jmarantz): user-agent-specific selection of inline threshold so that
// mobile phones are more prone to inlining.
//
// Further notes; jmaessen says:
//
// I suspect we do not want these bounds to match, and inlining for
// images is a bit more complicated because base64 encoding inflates
// the byte count of data: urls.  This is a non-issue for other
// resources (there may be some weirdness with iframes I haven't
// thought about...).
//
// jmarantz says:
//
// One thing we could do, if we believe they should be conceptually
// merged, is in image_rewrite_filter you could apply the
// base64-bloat-factor before comparing against the threshold.  Then
// we could use one number if we like that idea.
//
// jmaessen: For the moment, there's a separate threshold for image inline.
const int64 RewriteOptions::kDefaultCssInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultCssFlattenMaxBytes = 1024000;
const int64 RewriteOptions::kDefaultCssImageInlineMaxBytes = 0;
const int64 RewriteOptions::kDefaultCssOutlineMinBytes = 3000;
// 3K is bigger than Roboto loader for Chrome (2.2k)
const int64 RewriteOptions::kDefaultGoogleFontCssInlineMaxBytes = 3 * 1024;
const int64 RewriteOptions::kDefaultImageInlineMaxBytes = 3072;
const int64 RewriteOptions::kDefaultJsInlineMaxBytes = 2048;
const int64 RewriteOptions::kDefaultJsOutlineMinBytes = 3000;
const int64 RewriteOptions::kDefaultProgressiveJpegMinBytes = 10240;

const int64 RewriteOptions::kDefaultMaxHtmlCacheTimeMs = 0;
const int64 RewriteOptions::kDefaultMaxHtmlParseBytes = -1;
const int64 RewriteOptions::kDefaultMinResourceCacheTimeToRewriteMs = 0;

const int64 RewriteOptions::kDefaultFlushBufferLimitBytes = 100 * 1024;
const int64 RewriteOptions::kDefaultIdleFlushTimeMs = 10;
const int64 RewriteOptions::kDefaultImplicitCacheTtlMs = 5 * Timer::kMinuteMs;
const int64 RewriteOptions::kDefaultLoadFromFileCacheTtlMs =
    5 * Timer::kMinuteMs;
const int64 RewriteOptions::kDefaultMetadataInputErrorsCacheTtlMs =
    5 * Timer::kMinuteMs;

const int64 RewriteOptions::kDefaultPrioritizeVisibleContentCacheTimeMs =
    30 * Timer::kMinuteMs;  // 30 mins.

// Limit on concurrent ongoing image rewrites.
// TODO(jmaessen): Determine a sane default for this value.
const int RewriteOptions::kDefaultImageMaxRewritesAtOnce = 8;

// IE limits URL size overall to about 2k characters.  See
// http://support.microsoft.com/kb/208427/EN-US
const int RewriteOptions::kDefaultMaxUrlSize = 2083;

// Quality that needs to be used while recompressing any image type.
// If set to -1, we use source image quality parameters, and is lossless.
const int64 RewriteOptions::kDefaultImageRecompressQuality = 85;

// Jpeg quality that needs to be used while recompressing. If set to -1, we
// use the value of image_recompress_quality.
const int64 RewriteOptions::kDefaultImageJpegRecompressQuality = -1;
const int64 RewriteOptions::kDefaultImageJpegRecompressQualityForSmallScreens =
    70;
// TODO(huibao): Determine proper value for kDefaultImageJpegQualityForSaveData.
const int64 RewriteOptions::kDefaultImageJpegQualityForSaveData = 50;

// Number of scans to output for jpeg images when using progressive mode. If set
// to -1, we retain all scans of a progressive jpeg.
const int64 RewriteOptions::kDefaultImageJpegNumProgressiveScans = -1;

// Percentage savings in order to retain rewritten images; these default
// to 100% so that we always attempt to resize downsized images, and
// unconditionally retain images if they save any bytes at all.
const int RewriteOptions::kDefaultImageLimitOptimizedPercent = 100;
const int RewriteOptions::kDefaultImageLimitResizeAreaPercent = 100;

// Percentage limit on image wxh reduction for the rendered dimensions to be
// stored in the property cache. This is kept at default 95 after
// some experiments."
const int RewriteOptions::kDefaultImageLimitRenderedAreaPercent = 95;

// Sets limit for image optimization to 32MB.
const int64 RewriteOptions::kDefaultImageResolutionLimitBytes =
    32 * 1024 * 1024;

// WebP quality that needs to be used while recompressing. If set to -1, we
// use source image quality parameters.
const int64 RewriteOptions::kDefaultImageWebpRecompressQuality = 80;
const int64 RewriteOptions::kDefaultImageWebpRecompressQualityForSmallScreens =
    70;
const int64 RewriteOptions::kDefaultImageWebpAnimatedRecompressQuality = 70;
const int64 RewriteOptions::kDefaultImageWebpQualityForSaveData = 50;

// Timeout, in ms, for all WebP conversion attempts for each source
// image. If negative, does not time out.
const int64 RewriteOptions::kDefaultImageWebpTimeoutMs = -1;

const int64 RewriteOptions::kDefaultMaxCacheableResponseContentLength =
    16777216;  // 16 MB in bytes

// See http://github.com/apache/incubator-pagespeed-mod/issues/9.  By
// default, Apache evidently limits each URL path segment (between /)
// to about 256 characters.  This is not a fundamental URL limitation
// but is Apache specific.  Ben Noordhuis has provided a workaround
// of hooking map_to_storage to skip the directory-mapping phase in
// Apache.  See http://github.com/apache/incubator-pagespeed-mod/issues/176
const int RewriteOptions::kDefaultMaxUrlSegmentSize = 1024;

// Expiration limit for cookies that set PageSpeed options: 10 minutes.
const int64 RewriteOptions::kDefaultOptionCookiesDurationMs = 10 * 60 * 1000;

#ifdef NDEBUG
const int RewriteOptions::kDefaultRewriteDeadlineMs = 10;
#else
const int RewriteOptions::kDefaultRewriteDeadlineMs = 20;
#endif
const int kValgrindWaitForRewriteMs = 1000;
const int RewriteOptions::kDefaultPropertyCacheHttpStatusStabilityThreshold = 5;

const int RewriteOptions::kDefaultMaxRewriteInfoLogSize = 150;

const char RewriteOptions::kDefaultBeaconUrl[] = "/mod_pagespeed_beacon";

const int RewriteOptions::kDefaultMaxInlinedPreviewImagesIndex = -1;
const int64 RewriteOptions::kDefaultMinImageSizeLowResolutionBytes = 3 * 1024;
const int64 RewriteOptions::kDefaultMaxImageSizeLowResolutionBytes =
    1 * 1024 * 1024;  // 1 MB.

const int64 RewriteOptions::kDefaultMaxCombinedCssBytes = -1;  // No size limit
// Setting the limit on combined js resource to -1 will bypass the size check.
const int64 RewriteOptions::kDefaultMaxCombinedJsBytes = 90 * 1024;
const int64 RewriteOptions::kDefaultExperimentCookieDurationMs = Timer::kWeekMs;
const int64 RewriteOptions::kDefaultFinderPropertiesCacheExpirationTimeMs =
    2 * Timer::kHourMs;
const int64 RewriteOptions::kDefaultFinderPropertiesCacheRefreshTimeMs =
    (3 * Timer::kHourMs) / 2;
const int64 RewriteOptions::kDefaultMetadataCacheStalenessThresholdMs = 0;
const char RewriteOptions::kDefaultDownstreamCachePurgeMethod[] = "PURGE";
const int64
    RewriteOptions::kDefaultDownstreamCacheRewrittenPercentageThreshold = 95;
const int RewriteOptions::kDefaultExperimentTrafficPercent = 50;
const int RewriteOptions::kDefaultExperimentSlot = 1;

// An empty default key indicates that the blocking rewrite feature is disabled.
const char RewriteOptions::kDefaultBlockingRewriteKey[] = "";

const char RewriteOptions::kRejectedRequestUrlKeyName[] = "RejectedUrl";

// Allow all the declared shards.
const int RewriteOptions::kDefaultDomainShardCount = 0;

// By default, rebeacon every 5 seconds in high frequency mode. This will be
// multiplied by kLowFreqBeaconMult in critical_finder_support_util.h to
// determine the low frequency rebeacon time.
const int RewriteOptions::kDefaultBeaconReinstrumentTimeSec = 5;

// By default, all images are inline-previewed irrespective of size.
const int64 RewriteOptions::kDefaultMaxLowResImageSizeBytes = -1;

// By default, all images are inline-previewed, as long as the low-res size is
// lesser than the full-res size.
const int RewriteOptions::kDefaultMaxLowResToFullResImageSizePercentage = 100;

const double RewriteOptions::kDefaultResponsiveImageDensities[] = {1.5, 2.0,
                                                                   3.0};

const RewriteOptions::FilterEnumToIdAndNameEntry*
    RewriteOptions::filter_id_to_enum_array_[RewriteOptions::kEndOfFilters];

RewriteOptions::PropertyNameMap* RewriteOptions::option_name_to_property_map_ =
    nullptr;

const RewriteOptions::PropertyBase**
    RewriteOptions::option_id_to_property_array_ = nullptr;

RewriteOptions::Properties* RewriteOptions::properties_ = nullptr;
RewriteOptions::Properties* RewriteOptions::all_properties_ = nullptr;
RewriteOptions::Properties* RewriteOptions::deprecated_properties_ = nullptr;

const char RewriteOptions::AllowVaryOn::kNoneString[] = "None";
const char RewriteOptions::AllowVaryOn::kAutoString[] = "Auto";

namespace {

// When you change this, remember to update the documentation:
//    doc/en/speed/pagespeed/module/config_filters.html
// The documentation there includes the filter groups
// "rewrite_images", "extend_cache", and "rewrite_javascript", which
// expand to multiple filters, all of which need to be listed here.
// config_filters.html both includes lists of filters in each group
// and, redundantly, a table of all filters with one-liner
// documentation and which groups they are in.
const RewriteOptions::Filter kCoreFilterSet[] = {
    RewriteOptions::kAddHead,
    RewriteOptions::kCombineCss,
    RewriteOptions::kCombineJavascript,
    RewriteOptions::kConvertGifToPng,           // rewrite_images
    RewriteOptions::kConvertJpegToProgressive,  // rewrite_images
    RewriteOptions::kConvertJpegToWebp,         // rewrite_images
    RewriteOptions::kConvertMetaTags,
    RewriteOptions::kConvertPngToJpeg,       // rewrite_images
    RewriteOptions::kConvertToWebpLossless,  // rewrite_images
    RewriteOptions::kExtendCacheCss,         // extend_cache
    RewriteOptions::kExtendCacheImages,      // extend_cache
    RewriteOptions::kExtendCacheScripts,     // extend_cache
    RewriteOptions::kFallbackRewriteCssUrls,
    RewriteOptions::kFlattenCssImports,
    RewriteOptions::kInlineCss,
    RewriteOptions::kInlineImages,  // rewrite_images
    RewriteOptions::kInlineImportToLink,
    RewriteOptions::kInlineJavascript,
    RewriteOptions::kJpegSubsampling,  // rewrite_images
    RewriteOptions::kRecompressJpeg,   // rewrite_images
    RewriteOptions::kRecompressPng,    // rewrite_images
    RewriteOptions::kRecompressWebp,   // rewrite_images
    RewriteOptions::kResizeImages,     // rewrite_images
    RewriteOptions::kRewriteCss,
    RewriteOptions::kRewriteJavascriptExternal,  // rewrite_javascript
    RewriteOptions::kRewriteJavascriptInline,    // rewrite_javascript
    RewriteOptions::kRewriteStyleAttributesWithUrl,
    RewriteOptions::kStripImageColorProfile,  // rewrite_images
    RewriteOptions::kStripImageMetaData,      // rewrite_images
};

// The bandwidth-reduction filters exclude any filter that may modify
// URLs (combine, cache-extend, inline, outline).  Note also that turning
// on this level enables "preserve" mode which has the effect of
// making combine_css et al turn itself off.
//
// When you change this, remember to update the documentation:
//    doc/en/speed/pagespeed/module/config_filters.html
// The documentation there includes the filter groups "rewrite_images" and
// "extend_cache" which expand to multiple filters, all of which need to be
// listed here.  config_filters.html both includes lists of filters in each
// group and, redundantly, a table of all filters with one-liner documentation
// and  which groups they are in.
const RewriteOptions::Filter kOptimizeForBandwidthFilterSet[] = {
    RewriteOptions::kConvertGifToPng,           // rewrite_images
    RewriteOptions::kConvertJpegToProgressive,  // rewrite_images
    RewriteOptions::kConvertJpegToWebp,         // rewrite_images
    RewriteOptions::kConvertPngToJpeg,          // rewrite_images
    RewriteOptions::kInPlaceOptimizeForBrowser,
    RewriteOptions::kJpegSubsampling,  // rewrite_images
    RewriteOptions::kRecompressJpeg,   // rewrite_images
    RewriteOptions::kRecompressPng,    // rewrite_images
    RewriteOptions::kRecompressWebp,   // rewrite_images
    RewriteOptions::kRewriteCss,
    RewriteOptions::kRewriteJavascriptExternal,  // rewrite_javascript
    RewriteOptions::kRewriteJavascriptInline,    // rewrite_javascript
    RewriteOptions::kStripImageColorProfile,     // rewrite_images
    RewriteOptions::kStripImageMetaData,         // rewrite_images
};

// Note: all Core filters are Test filters as well.  For maintainability,
// this is managed in the c++ switch statement.
const RewriteOptions::Filter kTestFilterSet[] = {
    RewriteOptions::kConvertJpegToWebp,
    RewriteOptions::kDebug,
    RewriteOptions::kDeferIframe,
    RewriteOptions::kDeferJavascript,
    RewriteOptions::kDelayImages,  // AKA inline_preview_images
    RewriteOptions::kIncludeJsSourceMaps,
    RewriteOptions::kInsertAmpLink,
    RewriteOptions::kInsertGA,
    RewriteOptions::kInsertImageDimensions,
    RewriteOptions::kLazyloadImages,
    RewriteOptions::kLeftTrimUrls,
    RewriteOptions::kMakeGoogleAnalyticsAsync,
    RewriteOptions::kPrioritizeCriticalCss,
    RewriteOptions::kResizeToRenderedImageDimensions,
    RewriteOptions::kResponsiveImages,
    RewriteOptions::kRewriteDomains,
    RewriteOptions::kSpriteImages,
};

// Note: These filters should not be included even if the level is "All".
const RewriteOptions::Filter kDangerousFilterSet[] = {
    RewriteOptions::kCachePartialHtmlDeprecated,
    RewriteOptions::kCanonicalizeJavascriptLibraries,
    RewriteOptions::kComputeVisibleTextDeprecated,
    RewriteOptions::kDeterministicJs,  // used for measurement
    RewriteOptions::kDisableJavascript,
    RewriteOptions::kDivStructure,
    RewriteOptions::kExperimentCollectMobImageInfo,
    RewriteOptions::kExplicitCloseTags,
    RewriteOptions::kFixReflows,
    RewriteOptions::kMobilize,
    RewriteOptions::kMobilizePrecompute,      // TODO(jud): Unused, remove.
    RewriteOptions::kServeDeprecationNotice,  // internal.
    RewriteOptions::kStripScripts,
};

// List of filters whose correct behavior requires script execution.
// NOTE: Modify the
// SupportNoscriptFilter::IsAnyFilterRequiringScriptExecutionEnabled() method
// if you update this list.
const RewriteOptions::Filter kRequiresScriptExecutionFilterSet[] = {
    RewriteOptions::kCachePartialHtmlDeprecated,
    RewriteOptions::kDedupInlinedImages,
    RewriteOptions::kDeferIframe,
    RewriteOptions::kDeferJavascript,
    RewriteOptions::kDelayImages,
    RewriteOptions::kFlushSubresources,
    RewriteOptions::kLazyloadImages,
    RewriteOptions::kLocalStorageCache,
    RewriteOptions::kMobilize,
    // We do not include kPrioritizeVisibleContent since we do not want to
    // attach SupportNoscriptFilter in the case of blink pcache miss
    // pass-through, since this response will not have any custom script
    // inserted. Do the various critical css filters belong here?  Arguably not,
    // since even if we transform a page based on beacon results we'll enclose
    // the necessary in a noscript block and the page will still load / function
    // normally.
};

// List of filters that require a 'head' element to exist.
const RewriteOptions::Filter kAddHeadFilters[] = {
    RewriteOptions::kAddBaseTag,
    RewriteOptions::kAddHead,
    RewriteOptions::kAddInstrumentation,
    RewriteOptions::kCombineHeads,
    RewriteOptions::kDeterministicJs,
    RewriteOptions::kHandleNoscriptRedirect,
    RewriteOptions::kMakeGoogleAnalyticsAsync,
    RewriteOptions::kMobilize,
    RewriteOptions::kMoveCssAboveScripts,
    RewriteOptions::kMoveCssToHead,
};

// List of filters which are essential for mobilizing webpages, i.e., for
// making webpages designed for desktop computers look good on mobile devices.
//
// TODO(huibao): Once rewrite levels can be combined, move kRewriteCss and
// kRewriteDomains (and kDomainRewriteHyperlinks Option) into a proxy mode.
const RewriteOptions::Filter kMobilizeFilterSet[] = {
    RewriteOptions::kMobilize,
    // Turn on rewrite_css in order to rewrite hyper-links in CSS.
    RewriteOptions::kRewriteCss,
    RewriteOptions::kRewriteDomains,
};

// Array of mappings from Filter enum to corresponding filter id and name,
// used to map an enum value to id/name, and also used to initialize the
// reverse map from id to enum. Although the filter_enum field is not strictly
// necessary (because it equals the entry's index in the array), it is here so
// we can check during initialization that the array has been set up correctly.
//
// MUST be updated whenever a new Filter value is added and the new entry
// MUST be inserted in Filter enum order.
const RewriteOptions::FilterEnumToIdAndNameEntry
    kFilterVectorStaticInitializer[] = {
        {RewriteOptions::kAddBaseTag, "ab", "Add Base Tag"},
        {RewriteOptions::kAddHead, "ah", "Add Head"},
        {RewriteOptions::kAddIds, "ad", "Add Ids"},
        {RewriteOptions::kAddInstrumentation, "ai", "Add Instrumentation"},
        {RewriteOptions::kComputeStatistics, "ca", "Compute HTML statistics"},
        {RewriteOptions::kCachePartialHtmlDeprecated, "ct", "Deprecated."},
        {RewriteOptions::kCanonicalizeJavascriptLibraries, "ij",
         "Canonicalize Javascript library URLs"},
        {RewriteOptions::kCollapseWhitespace, "cw", "Collapse Whitespace"},
        {RewriteOptions::kCombineCss, RewriteOptions::kCssCombinerId,
         "Combine Css"},
        {RewriteOptions::kCombineHeads, "ch", "Combine Heads"},
        {RewriteOptions::kCombineJavascript,
         RewriteOptions::kJavascriptCombinerId, "Combine Javascript"},
        {RewriteOptions::kComputeCriticalCss, "bc",
         "Background Compute Critical css"},
        {RewriteOptions::kComputeVisibleTextDeprecated, "bp", "Deprecated."},
        {RewriteOptions::kConvertGifToPng, "gp", "Convert Gif to Png"},
        {RewriteOptions::kConvertJpegToProgressive, "jp",
         "Convert Jpeg to Progressive"},
        {RewriteOptions::kConvertJpegToWebp, "jw", "Convert Jpeg To Webp"},
        {RewriteOptions::kConvertMetaTags, "mc", "Convert Meta Tags"},
        {RewriteOptions::kConvertPngToJpeg, "pj", "Convert Png to Jpeg"},
        {RewriteOptions::kConvertToWebpAnimated, "wa",
         "Convert animated images to WebP"},
        {RewriteOptions::kConvertToWebpLossless, "ws",
         "When converting images to WebP, prefer lossless conversions"},
        {RewriteOptions::kDebug, "db", "Debug"},
        {RewriteOptions::kDecodeRewrittenUrls, "du", "Decode Rewritten URLs"},
        {RewriteOptions::kDedupInlinedImages, "dd", "Dedup Inlined Images"},
        {RewriteOptions::kDeferIframe, "df", "Defer Iframe"},
        {RewriteOptions::kDeferJavascript, "dj", "Defer Javascript"},
        {RewriteOptions::kDelayImages, "di", "Delay Images"},
        {RewriteOptions::kDeterministicJs, "mj", "Deterministic Js"},
        {RewriteOptions::kDisableJavascript, "jd",
         "Disables scripts by placing them inside noscript tags"},
        {RewriteOptions::kDivStructure, "ds", "Div Structure"},
        {RewriteOptions::kElideAttributes, "ea", "Elide Attributes"},
        {RewriteOptions::kExperimentCollectMobImageInfo, "xi",
         "Experiment: collect image info to help mobilization"},
        {RewriteOptions::kExperimentHttp2, "x2",
         "Experiment: http2-specific features in development"},
        {RewriteOptions::kExplicitCloseTags, "xc", "Explicit Close Tags"},
        {RewriteOptions::kExtendCacheCss, "ec", "Cache Extend Css"},
        {RewriteOptions::kExtendCacheImages, "ei", "Cache Extend Images"},
        {RewriteOptions::kExtendCachePdfs, "ep", "Cache Extend PDFs"},
        {RewriteOptions::kExtendCacheScripts, "es", "Cache Extend Scripts"},
        {RewriteOptions::kFallbackRewriteCssUrls, "fc",
         "Fallback Rewrite Css "},
        {RewriteOptions::kFixReflows, "fr", "Fix Reflows"},
        {RewriteOptions::kFlattenCssImports,
         RewriteOptions::kCssImportFlattenerId, "Flatten CSS Imports"},
        {RewriteOptions::kFlushSubresources, "fs", "Flush Subresources"},
        {RewriteOptions::kHandleNoscriptRedirect, "hn",
         "Handles Noscript Redirects"},
        {RewriteOptions::kHintPreloadSubresources, "hpsr",
         "Hint Preload of Subresources"},
        {RewriteOptions::kHtmlWriterFilter, "hw", "Flushes html"},
        {RewriteOptions::kIncludeJsSourceMaps,
         RewriteOptions::kJavascriptMinSourceMapId, "Include JS Source Maps"},
        {RewriteOptions::kInlineCss, RewriteOptions::kCssInlineId,
         "Inline Css"},
        {RewriteOptions::kInlineGoogleFontCss,
         RewriteOptions::kGoogleFontCssInlineId, "Inline Google Font CSS"},
        {RewriteOptions::kInlineImages, "ii", "Inline Images"},
        {RewriteOptions::kInlineImportToLink, "il", "Inline @import to Link"},
        {RewriteOptions::kInlineJavascript, RewriteOptions::kJavascriptInlineId,
         "Inline Javascript"},
        {RewriteOptions::kInPlaceOptimizeForBrowser, "io",
         "In-place optimize for browser"},
        {RewriteOptions::kInsertAmpLink, "ial", "Insert AMP link"},
        {RewriteOptions::kInsertDnsPrefetch, "idp", "Insert DNS Prefetch"},
        {RewriteOptions::kInsertGA, "ig", "Insert Google Analytics"},
        {RewriteOptions::kInsertImageDimensions, "id",
         "Insert Image Dimensions"},
        {RewriteOptions::kJpegSubsampling, "js", "Jpeg Subsampling"},
        {RewriteOptions::kLazyloadImages, "ll", "Lazyload Images"},
        {RewriteOptions::kLeftTrimUrls, "tu", "Left Trim Urls"},
        {RewriteOptions::kLocalStorageCache,
         RewriteOptions::kLocalStorageCacheId, "Local Storage Cache"},
        {RewriteOptions::kMakeGoogleAnalyticsAsync, "ga",
         "Make Google Analytics Async"},
        {RewriteOptions::kMakeShowAdsAsync, "gaa",
         "Convert showads.js use to async adsbygoogle.js"},
        {RewriteOptions::kMobilize, "mob", "Mobilize Webpage"},
        {RewriteOptions::kMobilizePrecompute, "mob_precompute", "Deprecated."},
        {RewriteOptions::kMoveCssAboveScripts, "cj", "Move Css Above Scripts"},
        {RewriteOptions::kMoveCssToHead, "cm", "Move Css To Head"},
        {RewriteOptions::kOutlineCss, "co", "Outline Css"},
        {RewriteOptions::kOutlineJavascript, "jo", "Outline Javascript"},
        {RewriteOptions::kPedantic, "pc", "Add pedantic types"},
        {RewriteOptions::kPrioritizeCriticalCss,
         RewriteOptions::kPrioritizeCriticalCssId, "Prioritize Critical Css"},
        {RewriteOptions::kRecompressJpeg, "rj", "Recompress Jpeg"},
        {RewriteOptions::kRecompressPng, "rp", "Recompress Png"},
        {RewriteOptions::kRecompressWebp, "rw", "Recompress Webp"},
        {RewriteOptions::kRemoveComments, "rc", "Remove Comments"},
        {RewriteOptions::kRemoveQuotes, "rq", "Remove Quotes"},
        {RewriteOptions::kResizeImages, "ri", "Resize Images"},
        {RewriteOptions::kResizeMobileImages, "rm", "Resize Mobile Images"},
        {RewriteOptions::kResizeToRenderedImageDimensions, "ir",
         "Resize to Rendered Image Dimensions"},
        {RewriteOptions::kResponsiveImages, "rx", "Responsive Images"},
        {RewriteOptions::kResponsiveImagesZoom, "rz", "Responsive Images Zoom"},
        {RewriteOptions::kRewriteCss, RewriteOptions::kCssFilterId,
         "Rewrite Css"},
        {RewriteOptions::kRewriteDomains, "rd", "Rewrite Domains"},
        {RewriteOptions::kRewriteJavascriptExternal,
         RewriteOptions::kJavascriptMinId, "Rewrite External Javascript"},
        {RewriteOptions::kRewriteJavascriptInline, "jj",
         "Rewrite Inline Javascript"},
        {RewriteOptions::kRewriteStyleAttributes, "cs",
         "Rewrite Style Attributes"},
        {RewriteOptions::kRewriteStyleAttributesWithUrl, "cu",
         "Rewrite Style Attributes With Url"},
        {RewriteOptions::kServeDeprecationNotice, "sd",
         "Serve Deprecation Notice"},
        {RewriteOptions::kSplitHtml, "sh", "Deprecated"},
        {RewriteOptions::kSplitHtmlHelper, "se", "Deprecated"},
        {RewriteOptions::kSpriteImages, RewriteOptions::kImageCombineId,
         "Sprite Images"},
        {RewriteOptions::kStripImageColorProfile, "cp",
         "Strip Image Color Profiles"},
        {RewriteOptions::kStripImageMetaData, "md", "Strip Image Meta Data"},
        {RewriteOptions::kStripScripts, "ss", "Strip Scripts"},
};

const RewriteOptions::Filter kImagePreserveUrlDisabledFilters[] = {
    // TODO(jkarlin): Remove kResizeImages from the forbid list and allow image
    // squashing prefetching in HTML path (but don't allow resizing based on
    // HTML attributes).
    RewriteOptions::kDelayImages,
    RewriteOptions::kExtendCacheImages,
    RewriteOptions::kInlineImages,
    RewriteOptions::kLazyloadImages,
    RewriteOptions::kResizeImages,
    RewriteOptions::kResizeToRenderedImageDimensions,
    RewriteOptions::kResponsiveImages,
    RewriteOptions::kSpriteImages};

const RewriteOptions::Filter kJsPreserveUrlDisabledFilters[] = {
    RewriteOptions::kCanonicalizeJavascriptLibraries,
    RewriteOptions::kCombineJavascript,
    RewriteOptions::kDeferJavascript,
    RewriteOptions::kExtendCacheScripts,
    RewriteOptions::kInlineJavascript,
    RewriteOptions::kOutlineJavascript};

const RewriteOptions::Filter kCssPreserveUrlDisabledFilters[] = {
    RewriteOptions::kCombineCss,         RewriteOptions::kExtendCacheCss,
    RewriteOptions::kInlineCss,          RewriteOptions::kInlineGoogleFontCss,
    RewriteOptions::kInlineImportToLink, RewriteOptions::kLeftTrimUrls,
    RewriteOptions::kOutlineCss};

#ifndef NDEBUG
void CheckFilterSetOrdering(const RewriteOptions::Filter* filters, int num) {
  for (int i = 1; i < num; ++i) {
    DCHECK_GT(filters[i], filters[i - 1]);
  }
}
#endif

// Table of properties for each filter to make it faster to check whether the
// a filter is a member of a rewrite level or needs to be disabled when a
// configuration is set to preserve resource URLs.  The table is initialized
// once in RewriteOptions::Initialize.
struct FilterProperties {
  uint8 level_core : 1;
  uint8 level_optimize_for_bandwidth : 1;
  uint8 level_mobilize : 1;
  uint8 level_test : 1;
  uint8 level_dangerous : 1;
  uint8 preserve_image_urls : 1;
  uint8 preserve_js_urls : 1;
  uint8 preserve_css_urls : 1;
};
FilterProperties filter_properties[RewriteOptions::kEndOfFilters];

bool IsInSet(const RewriteOptions::Filter* filters, int num,
             RewriteOptions::Filter filter) {
  const RewriteOptions::Filter* end = filters + num;
  return std::binary_search(filters, end, filter);
}

// Strips the "ets=" query param (if present) from the end of url and strips all
// query params from url and assigns to url_no_query_param.
void StripBeaconUrlQueryParam(GoogleString* url,
                              GoogleString* url_no_query_param) {
  if (StringPiece(*url).ends_with("ets=")) {
    // Strip the ? or & in front of ets= as well.
    int chars_to_strip = STATIC_STRLEN("ets=") + 1;
    url->resize(url->size() - chars_to_strip);
  }

  StringPieceVector url_split;
  SplitStringUsingSubstr(*url, "?", &url_split);
  url_split[0].CopyToString(url_no_query_param);
}

// Maps the deprecated options to the new names.
struct RenamedOptionMap {
  static bool LessThan(const RenamedOptionMap& option_map, StringPiece arg) {
    return StringCaseCompare(option_map.deprecated_option_name, arg) < 0;
  }

  const char* deprecated_option_name;
  const char* new_option_name;
};

const RenamedOptionMap kRenamedOptionNameData[] = {
    {"ImageWebpRecompressionQuality", "WebpRecompressionQuality"},
    {"ImageWebpRecompressionQualityForSmallScreens",
     "WebpRecompressionQualityForSmallScreens"}};

// Will be initialized to a sorted list of headers not allowed in
// AddResourceHeader.
const StringPieceVector* fixed_resource_headers = nullptr;
bool http_header_separators[256] = {false};

}  // namespace

const char* RewriteOptions::FilterName(Filter filter) {
  int i = static_cast<int>(filter);
  int n = arraysize(kFilterVectorStaticInitializer);
  if (i >= 0 && i < n) {
    return kFilterVectorStaticInitializer[i].filter_name;
  }
  LOG(DFATAL) << "Unknown filter: " << filter;
  return "Unknown Filter";
}

const char* RewriteOptions::FilterId(Filter filter) {
  int i = static_cast<int>(filter);
  int n = arraysize(kFilterVectorStaticInitializer);
  if (i >= 0 && i < n) {
    return kFilterVectorStaticInitializer[i].filter_id;
  }
  LOG(DFATAL) << "Unknown filter code: " << filter;
  return "UF";
}

int RewriteOptions::NumFilterIds() {
  return arraysize(kFilterVectorStaticInitializer);
}

bool RewriteOptions::ParseRewriteLevel(const StringPiece& in,
                                       RewriteLevel* out) {
  bool ret = false;
  if (!in.empty()) {
    if (StringCaseEqual(in, "CoreFilters")) {
      *out = kCoreFilters;
      ret = true;
    } else if (StringCaseEqual(in, "PassThrough")) {
      *out = kPassThrough;
      ret = true;
    } else if (StringCaseEqual(in, "OptimizeForBandwidth")) {
      *out = kOptimizeForBandwidth;
      ret = true;
    } else if (StringCaseEqual(in, "MobilizeFilters")) {
      *out = kMobilizeFilters;
      ret = true;
    } else if (StringCaseEqual(in, "TestingCoreFilters")) {
      *out = kTestingCoreFilters;
      ret = true;
    } else if (StringCaseEqual(in, "AllFilters")) {
      *out = kAllFilters;
      ret = true;
    }
  }
  return ret;
}

bool RewriteOptions::ParseInlineUnauthorizedResourceType(
    const StringPiece& in, ResourceCategorySet* out) {
  // Examples:
  // InlineResourcesWithoutExplicitAuthorization Script,Stylesheet
  // InlineResourcesWithoutExplicitAuthorization Stylesheet
  // InlineResourcesWithoutExplicitAuthorization off
  StringPieceVector resource_types;
  SplitStringPieceToVector(in, ",", &resource_types, true);
  for (int i = 0, n = resource_types.size(); i < n; ++i) {
    StringPiece resource_type = resource_types[i];
    semantic_type::Category category;
    if (StringCaseEqual(resource_type, "off")) {
      out->clear();
    } else if (!semantic_type::ParseCategory(resource_type, &category)) {
      // Invalid resource category.
      return false;
    } else {
      out->insert(category);
    }
  }
  return true;
}

bool RewriteOptions::ParseBeaconUrl(const StringPiece& in, BeaconUrl* out) {
  StringPieceVector urls;
  SplitStringPieceToVector(in, " ", &urls, true);

  if (urls.size() > 2 || urls.size() < 1) {
    return false;
  }
  urls[0].CopyToString(&out->http);
  if (urls.size() == 2) {
    urls[1].CopyToString(&out->https);
  } else if (urls[0].starts_with("http:")) {
    out->https.clear();
    StrAppend(&out->https, "https:", urls[0].substr(STATIC_STRLEN("http:")));
  } else {
    urls[0].CopyToString(&out->https);
  }

  // We used to require that the query param end with "ets=", but no longer
  // do, so strip it if it's present. We also assign http_in and https_in to the
  // beacon URL stripped of their query params, if any are present.
  StripBeaconUrlQueryParam(&out->http, &out->http_in);
  StripBeaconUrlQueryParam(&out->https, &out->https_in);

  return true;
}

bool RewriteOptions::ParseFromString(StringPiece in, Color* color) {
  // We just handle #aabbcc syntax.
  if (in.length() != 7 || in[0] != '#') {
    return false;
  }
  for (int i = 1; i < 7; ++i) {
    if (!IsHexDigit(in[i])) {
      return false;
    }
  }
  uint32 r = 0, g = 0, b = 0;
  AccumulateHexValue(in[1], &r);
  AccumulateHexValue(in[2], &r);
  AccumulateHexValue(in[3], &g);
  AccumulateHexValue(in[4], &g);
  AccumulateHexValue(in[5], &b);
  AccumulateHexValue(in[6], &b);
  color->r = static_cast<unsigned char>(r);
  color->g = static_cast<unsigned char>(g);
  color->b = static_cast<unsigned char>(b);
  return true;
}

bool RewriteOptions::ParseFromString(StringPiece in, MobTheme* theme) {
  StringPieceVector args;
  SplitStringPieceToVector(in, " ", &args, true);
  if (args.size() != 2 && args.size() != 3) {
    return false;
  }

  if (!ParseFromString(args[0], &theme->background_color) ||
      !ParseFromString(args[1], &theme->foreground_color)) {
    return false;
  }
  if (args.size() == 3) {
    args[2].CopyToString(&theme->logo_url);
  }
  return true;
}

bool RewriteOptions::ImageOptimizationEnabled() const {
  return (this->Enabled(RewriteOptions::kRecompressJpeg) ||
          this->Enabled(RewriteOptions::kRecompressPng) ||
          this->Enabled(RewriteOptions::kRecompressWebp) ||
          this->Enabled(RewriteOptions::kConvertGifToPng) ||
          this->Enabled(RewriteOptions::kConvertJpegToProgressive) ||
          this->Enabled(RewriteOptions::kConvertPngToJpeg) ||
          this->Enabled(RewriteOptions::kConvertJpegToWebp) ||
          this->Enabled(RewriteOptions::kConvertToWebpAnimated) ||
          this->Enabled(RewriteOptions::kConvertToWebpLossless));
}

RewriteOptions::RewriteOptions(ThreadSystem* thread_system)
    : modified_(false),
      frozen_(false),
      purge_set_(PurgeSet(kCachePurgeBytes)),
      initialized_options_(0),
      options_uniqueness_checked_(false),
      need_to_store_experiment_data_(false),
      experiment_id_(experiment::kExperimentNotSet),
      experiment_percent_(0),
      signature_(),
      hasher_(kHashBytes),
      thread_system_(thread_system) {
  cache_purge_mutex_ = std::make_unique<NullRWLock>();

  DCHECK(properties_ != nullptr)
      << "Call RewriteOptions::Initialize() before construction";

  // Sanity-checks -- will be active only when compiled for debug.
#ifndef NDEBUG
  CheckFilterSetOrdering(kCoreFilterSet, arraysize(kCoreFilterSet));
  CheckFilterSetOrdering(kTestFilterSet, arraysize(kTestFilterSet));
  CheckFilterSetOrdering(kDangerousFilterSet, arraysize(kDangerousFilterSet));
  CheckFilterSetOrdering(kImagePreserveUrlDisabledFilters,
                         arraysize(kImagePreserveUrlDisabledFilters));
  CheckFilterSetOrdering(kJsPreserveUrlDisabledFilters,
                         arraysize(kJsPreserveUrlDisabledFilters));
  CheckFilterSetOrdering(kCssPreserveUrlDisabledFilters,
                         arraysize(kCssPreserveUrlDisabledFilters));
  CheckFilterSetOrdering(kAddHeadFilters, arraysize(kAddHeadFilters));

  // Ensure that all filters have unique IDs.
  StringSet id_set;
  for (int i = 0; i < static_cast<int>(kEndOfFilters); ++i) {
    Filter filter = static_cast<Filter>(i);
    const char* id = FilterId(filter);
    std::pair<StringSet::iterator, bool> insertion = id_set.insert(id);
    DCHECK(insertion.second) << "Duplicate RewriteOption filter id: " << id;
  }

  // We can't check options uniqueness until additional extra
  // options are added by subclasses.  We could do this in the
  // destructor I suppose, but we defer it till ComputeSignature.
#endif

  // TODO(jmarantz): make rewrite_deadline changeable from the Factory based on
  // the requirements of the testing system and the platform. This might also
  // want to change based on how many Flushes there are, as each Flush can
  // potentially add this much more latency.
  if (RunningOnValgrind()) {
    set_rewrite_deadline_ms(kValgrindWaitForRewriteMs);
    set_in_place_rewrite_deadline_ms(kValgrindWaitForRewriteMs);
    modified_ = false;
#ifndef NDEBUG
    last_thread_id_.reset();
#endif
  }

  InitializeOptions(properties_);

  // Enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

// static
void RewriteOptions::AddProperties() {
  // TODO(jmarantz): move the help text to #defines or maybe const char[] in
  // .h file so that rewrite_gflags.cc can reference the same strings in
  // DEFINE_xxx directives.
  //
  //
  // Note: there are two functions used for registering properties here,
  // AddBaseProperty() and AddRequestProperty().  AddRequestProperty()
  // is kind of a hack for stuffing request-specific data into the RewriteOption
  // object.  Those options should probably be changed to be fields in the
  // recently-added RequestContext.
  //
  // AddBaseProperty() is for user-settable options.  The last argument
  // is a help-string.  The presence of a help-string enables the option
  // for mod_pagespeed, and serves as the error message if there is a
  // syntax error specifying the option in pagespeed.conf.
  //
  // There are four sorts of options which pass in NULL for the help-string
  // 1. Options that should be enabled in mod_pagespeed but we haven't
  //    written the help-string or added HTML documentation yet.  These
  //    will be flagged with:
  //    // TODO(jmarantz): write help & doc for mod_pagespeed.
  // 2. Options which are experimental and temporary and are not ready for
  //    permanent support in mod_pagespeed.  These will be marked:
  //    // TODO(jmarantz): eliminate experiment or document.
  // 3. Options which are not applicable to mod_pagespeed, e.g. those that
  //    support features not yet in mod_pagespeed, or have an alternate solution
  //    (populating the cache invalidation timestamp).
  //    These are marked as:
  //    // Not applicable for mod_pagespeed.
  // 4. Options which should be in mod_pagespeed but need a bit more
  //    implementation before they are ready.  Marked as:
  //    // TODO(jmarantz): implement for mod_pagespeed.
  AddBaseProperty(kPassThrough, &RewriteOptions::level_, "l", kRewriteLevel,
                  kDirectoryScope,
                  "Base level of rewriting (PassThrough, CoreFilters)", true);
  AddBaseProperty(
      kDefaultCssFlattenMaxBytes, &RewriteOptions::css_flatten_max_bytes_, "cf",
      kCssFlattenMaxBytes, kQueryScope,
      "Number of bytes below which stylesheets will be flattened.", true);
  AddBaseProperty(kDefaultCssImageInlineMaxBytes,
                  &RewriteOptions::css_image_inline_max_bytes_, "cii",
                  kCssImageInlineMaxBytes, kQueryScope,
                  "Number of bytes below which CSS images will be inlined.",
                  true);
  AddBaseProperty(
      kDefaultCssInlineMaxBytes, &RewriteOptions::css_inline_max_bytes_, "ci",
      kCssInlineMaxBytes, kQueryScope,
      "Number of bytes below which stylesheets will be inlined.", true);
  AddBaseProperty(
      kDefaultGoogleFontCssInlineMaxBytes,
      &RewriteOptions::google_font_css_inline_max_bytes_, "gfci",
      kGoogleFontCssInlineMaxBytes, kQueryScope,
      "Number of bytes below which Google Font stylesheets will be inlined.",
      true);
  AddBaseProperty(kDefaultCssOutlineMinBytes,
                  &RewriteOptions::css_outline_min_bytes_, "co",
                  kCssOutlineMinBytes, kDirectoryScope,
                  "Number of bytes above which inline CSS resources will be "
                  "outlined.",
                  true);
  AddBaseProperty(kDefaultImageInlineMaxBytes,
                  &RewriteOptions::image_inline_max_bytes_, "ii",
                  kImageInlineMaxBytes, kQueryScope,
                  "Number of bytes below which images will be inlined.", true);
  AddBaseProperty(
      kDefaultJsInlineMaxBytes, &RewriteOptions::js_inline_max_bytes_, "ji",
      kJsInlineMaxBytes, kQueryScope,
      "Number of bytes below which javascript will be inlined.", true);
  AddBaseProperty(kDefaultJsOutlineMinBytes,
                  &RewriteOptions::js_outline_min_bytes_, "jo",
                  kJsOutlineMinBytes, kDirectoryScope,
                  "Number of bytes above which inline Javascript resources will"
                  "be outlined.",
                  true);
  AddBaseProperty(kDefaultProgressiveJpegMinBytes,
                  &RewriteOptions::progressive_jpeg_min_bytes_, "jp",
                  kProgressiveJpegMinBytes, kDirectoryScope,
                  "Minimum size in bytes for converting a jpeg to progressive",
                  true);
  AddBaseProperty(kDefaultMaxCacheableResponseContentLength,
                  &RewriteOptions::max_cacheable_response_content_length_,
                  "rcl", kMaxCacheableResponseContentLength, kServerScope,
                  "Maximum length of a cacheable response content. "
                  "To remove this limit, use -1.",
                  true);
  AddBaseProperty(kDefaultMaxHtmlCacheTimeMs,
                  &RewriteOptions::max_html_cache_time_ms_, "hc",
                  kMaxHtmlCacheTimeMs, kDirectoryScope, nullptr, true);
  AddBaseProperty(
      kDefaultMaxHtmlParseBytes, &RewriteOptions::max_html_parse_bytes_, "hpb",
      kMaxHtmlParseBytes,
      kDirectoryScope,  // TODO(jmarantz): switch to kProcessScopeStrict?
      "Maximum number of bytes of HTML that we parse, before "
      "redirecting to ?ModPagespeed=off",
      true);
  AddBaseProperty(kDefaultMinResourceCacheTimeToRewriteMs,
                  &RewriteOptions::min_resource_cache_time_to_rewrite_ms_, "rc",
                  kMinResourceCacheTimeToRewriteMs, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): remove this or document it.
  AddBaseProperty(false, &RewriteOptions::oblivious_pagespeed_urls_, "opu",
                  kObliviousPagespeedUrls, kDirectoryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(
      false, &RewriteOptions::rewrite_uncacheable_resources_, "rur",
      kRewriteUncacheableResources, kServerScope,
      "Allow optimization of uncacheable resources in the in-place rewriting"
      " mode.",
      true);
  AddBaseProperty(kDefaultIdleFlushTimeMs, &RewriteOptions::idle_flush_time_ms_,
                  "if", kIdleFlushTimeMs, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): implement for mod_pagespeed.
  AddBaseProperty(kDefaultFlushBufferLimitBytes,
                  &RewriteOptions::flush_buffer_limit_bytes_, "fbl",
                  kFlushBufferLimitBytes, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): implement for mod_pagespeed.
  AddBaseProperty(
      kDefaultImplicitCacheTtlMs, &RewriteOptions::implicit_cache_ttl_ms_,
      "ict", kImplicitCacheTtlMs, kDirectoryScope,
      "Time in milliseconds to cache resources that lack an Expires or "
      "Cache-Control header",
      true);
  AddBaseProperty(
      kDefaultLoadFromFileCacheTtlMs,
      &RewriteOptions::load_from_file_cache_ttl_ms_, "lfct",
      kLoadFromFileCacheTtlMs, kDirectoryScope,
      "Time in milliseconds to cache resources loaded from file that lack an "
      "Expires or Cache-Control header. If not explicitly set, defaults to "
      "using the value set by implicit_cache_ttl_ms",
      true);
  AddBaseProperty(kDefaultImageMaxRewritesAtOnce,
                  &RewriteOptions::image_max_rewrites_at_once_, "im",
                  kImageMaxRewritesAtOnce, kLegacyProcessScope,
                  "Set bound on number of images being rewritten at one time "
                  "(0 = unbounded).",
                  true);
  AddBaseProperty(kDefaultMaxUrlSegmentSize,
                  &RewriteOptions::max_url_segment_size_, "uss",
                  kMaxUrlSegmentSize, kDirectoryScope,
                  "Maximum size of a URL segment.", true);
  AddBaseProperty(kDefaultMaxUrlSize, &RewriteOptions::max_url_size_, "us",
                  kMaxUrlSize, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(false, &RewriteOptions::forbid_all_disabled_filters_, "fadf",
                  kForbidAllDisabledFilters, kDirectoryScope,
                  "Prevents the use of disabled filters", true);
  AddBaseProperty(
      kDefaultRewriteDeadlineMs, &RewriteOptions::rewrite_deadline_ms_, "rdm",
      kRewriteDeadlineMs, kDirectoryScope,
      "Time to wait for resource optimization (per flush window) before"
      "falling back to the original resource for the request.",
      true);
  AddBaseProperty(kEnabledOn, &RewriteOptions::enabled_, "e", kEnabled,
                  kDirectoryScope, nullptr,
                  true);  // initialized explicitly in mod_instaweb.cc.
  AddBaseProperty(
      false, &RewriteOptions::add_options_to_urls_, "aou", kAddOptionsToUrls,
      kDirectoryScope,
      "Add query-params with configuration adjustments to rewritten "
      "URLs.",
      true);

  // TODO(jmarantz): consider whether to document this option -- it
  // potentially can hide problems in configuration or bugs.
  AddBaseProperty(
      false, &RewriteOptions::publicly_cache_mismatched_hashes_experimental_,
      "pcmh", kPubliclyCacheMismatchedHashesExperimental, kDirectoryScope,
      "When serving a request for a .pagespeed. URL with the wrong hash, allow "
      "public caching based on the origin TTL.",
      false);

  AddBaseProperty(true, &RewriteOptions::in_place_rewriting_enabled_, "ipro",
                  kInPlaceResourceOptimization, kDirectoryScope,
                  "Allow rewriting resources even when they are "
                  "fetched over non-pagespeed URLs.",
                  true);
  AddBaseProperty(false, &RewriteOptions::in_place_wait_for_optimized_, "ipwo",
                  kInPlaceWaitForOptimized, kDirectoryScope,
                  "Wait for optimizations to complete",
                  true);  // TODO(jmarantz): Add doc.
  AddBaseProperty(10, &RewriteOptions::in_place_s_maxage_sec_, "ipsm",
                  kInPlaceSMaxAgeSec, kServerScope,
                  "What to set s-maxage to on not-yet-optimized ipro resources",
                  true);
  AddBaseProperty(kDefaultRewriteDeadlineMs,
                  &RewriteOptions::in_place_rewrite_deadline_ms_, "iprdm",
                  kInPlaceRewriteDeadlineMs, kDirectoryScope,
                  "Time to wait for an in-place resource optimization before"
                  "falling back to the original resource for the request.",
                  true);
  AddBaseProperty(
      true, &RewriteOptions::in_place_preemptive_rewrite_css_, "ipprc",
      kInPlacePreemptiveRewriteCss, kDirectoryScope,
      "If set, issue preemptive rewrites of CSS on the HTML path when "
      "configured to use IPRO.",
      true);
  AddBaseProperty(true,
                  &RewriteOptions::in_place_preemptive_rewrite_css_images_,
                  "ipprci", kInPlacePreemptiveRewriteCssImages, kDirectoryScope,
                  "If set, issue preemptive rewrites of CSS images on the IPRO "
                  "serving path.",
                  true);
  AddBaseProperty(
      true, &RewriteOptions::in_place_preemptive_rewrite_images_, "ippri",
      kInPlacePreemptiveRewriteImages, kDirectoryScope,
      "If set, issue preemptive rewrites of images on the HTML path "
      "when configured to use IPRO.",
      true);
  AddBaseProperty(
      true, &RewriteOptions::in_place_preemptive_rewrite_javascript_, "ipprj",
      kInPlacePreemptiveRewriteJavascript, kDirectoryScope,
      "If set, issue preemptive rewrites of JS on the HTML path when "
      "configured to use IPRO.",
      true);
  AddBaseProperty(
      true, &RewriteOptions::private_not_vary_for_ie_, "pnvie",
      kPrivateNotVaryForIE, kDirectoryScope,
      "If set, serve in-place optimized resources as Cache-Control: private "
      "rather than Vary: Accept.  Avoids an extra fetch on cache hit, but "
      "prevents proxy caching of these resources.  Only relevant if your "
      "proxy caches Vary: Accept",
      true);
  AddBaseProperty(true, &RewriteOptions::combine_across_paths_, "cp",
                  kCombineAcrossPaths, kDirectoryScope,
                  "Allow combining resources from different paths", true);
  AddBaseProperty(true, &RewriteOptions::critical_images_beacon_enabled_,
                  "cibe", kCriticalImagesBeaconEnabled, kDirectoryScope,
                  "Enable insertion of client-side critical "
                  "image detection js for image optimization filters.",
                  true);
  AddBaseProperty(
      false,
      &RewriteOptions::
          test_only_prioritize_critical_css_dont_apply_original_css_,
      "dlacae", kTestOnlyPrioritizeCriticalCssDontApplyOriginalCss,
      kDirectoryScope,
      "Stops the prioritize_critical_css filter from invoking its JavaScript "
      "that applies all the 'hidden' CSS at onload. Intended for testing.",
      false);
  AddBaseProperty(kDefaultBeaconReinstrumentTimeSec,
                  &RewriteOptions::beacon_reinstrument_time_sec_, "brts",
                  kBeaconReinstrumentTimeSec, kDirectoryScope,
                  "How often (in seconds) to reinstrument pages with beacons. "
                  "This is used for both critical image beaconing, and for the "
                  "prioritize_critical_css filter.",
                  true);
  AddBaseProperty(false, &RewriteOptions::log_background_rewrites_, "lbr",
                  kLogBackgroundRewrite, kServerScope, nullptr,
                  false);  // TODO(huibao): write help & doc for mod_pagespeed.
  AddBaseProperty(false, &RewriteOptions::log_mobilization_samples_, "lms",
                  kLogMobilizationSamples, kDirectoryScope,
                  "Verbose debugging of all sample data"
                  " generated by mobilization_label_filter.",
                  false);
  AddBaseProperty(
      false, &RewriteOptions::log_rewrite_timing_, "lr", kLogRewriteTiming,
      kDirectoryScope,
      "Whether or not to report timing information about HtmlParse.", false);
  AddBaseProperty(
      false, &RewriteOptions::log_url_indices_, "lui", kLogUrlIndices,
      kDirectoryScope,
      "Whether or not to log URL indices for rewriter applications.", false);
  AddBaseProperty(false, &RewriteOptions::lowercase_html_names_, "lh",
                  kLowercaseHtmlNames, kDirectoryScope,
                  "Lowercase tag and attribute names for HTML.", true);
  AddBaseProperty(false, &RewriteOptions::always_rewrite_css_, "arc",
                  kAlwaysRewriteCss, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(false, &RewriteOptions::respect_vary_, "rv", kRespectVary,
                  kDirectoryScope,
                  "Whether to respect Vary headers for resources. "
                  "Vary is always respected for HTML.",
                  true);
  AddBaseProperty(
      false, &RewriteOptions::respect_x_forwarded_proto_, "rxfp",
      kRespectXForwardedProto,
      // Note: We mark this as kDirectoryScope because we mistakenly used to.
      // It does not actually work in directory-scope and is documented to
      // only work on server-scope.
      // Note: We must check this option to get the proper URL, but the proper
      // URL is needed to get directory-specific options, so allowing this in
      // directory-scope would be a circular dependency.
      kDirectoryScope, "Whether to respect the X-Forwarded-Proto header.",
      true);
  AddBaseProperty(
      false, &RewriteOptions::flush_html_, "fh", kFlushHtml, kServerScope,
      "Enable auto-flush heuristics for HTML in full proxy mode", true);
  AddBaseProperty(false, &RewriteOptions::css_preserve_urls_, "cpu",
                  kCssPreserveURLs, kDirectoryScope,
                  "Disable the rewriting of CSS URLs.", true);
  AddBaseProperty(false, &RewriteOptions::image_preserve_urls_, "ipu",
                  kImagePreserveURLs, kDirectoryScope,
                  "Disable the rewriting of Image URLs.", true);
  AddBaseProperty(false, &RewriteOptions::js_preserve_urls_, "jpu",
                  kJsPreserveURLs, kDirectoryScope,
                  "Disable the rewriting of Javascript URLs.", true);
  AddBaseProperty(true, &RewriteOptions::serve_stale_if_fetch_error_, "ss",
                  kServeStaleIfFetchError, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(
      false, &RewriteOptions::proactively_freshen_user_facing_request_, "pfur",
      kProactivelyFreshenUserFacingRequest, kDirectoryScope, nullptr, true);
  AddBaseProperty(
      0, &RewriteOptions::serve_stale_while_revalidate_threshold_sec_, "sswrt",
      kServeStaleWhileRevalidateThresholdSec, kDirectoryScope,
      "Threshold for serving serving stale responses while revalidating in "
      "background. 0 means don't serve stale content."
      "Note: Stale response will be served only for non-html requests.",
      true);
  AddBaseProperty(
      true, &RewriteOptions::follow_flushes_, "ff", kFollowFlushes,
      kDirectoryScope,
      "Attempt to mirror incoming flushes for html streams in the output "
      "when ProxyFetch is used.",
      true);
  AddBaseProperty(false, &RewriteOptions::enable_defer_js_experimental_, "edje",
                  kEnableDeferJsExperimental, kDirectoryScope,
                  "Enable experimental options in defer javascript.", true);
  AddBaseProperty(false, &RewriteOptions::disable_background_fetches_for_bots_,
                  "dbfb", kDisableBackgroundFetchesForBots, kDirectoryScope,
                  "Disable pre-emptive background fetches on bot requests.",
                  true);
  AddBaseProperty(
      true,  // By default, don't optimize resource if no-transform is set.
      &RewriteOptions::disable_rewrite_on_no_transform_, "drnt",
      kDisableRewriteOnNoTransform, kDirectoryScope,
      "If false, resource is rewritten even if no-transform header is set",
      true);
  AddBaseProperty(
      false, &RewriteOptions::enable_cache_purge_, "euci", kEnableCachePurge,
      kServerScope,
      "Allows individual resources to be flushed; adding some overhead to "
      "the metadata cache",
      true);
  AddBaseProperty(
      false, &RewriteOptions::proactive_resource_freshening_, "prf",
      kProactiveResourceFreshening, kServerScope,
      "If true, allows proactive freshening of inputs to the resource when "
      "they are close to expiry.",
      true);  // TODO(mpalem): write end user doc in
              // net/instaweb/doc/en/speed/pagespeed/module/system.html
  AddBaseProperty(false, &RewriteOptions::lazyload_highres_images_, "elhr",
                  kEnableLazyLoadHighResImages, kDirectoryScope, nullptr, true);
  AddBaseProperty(false, &RewriteOptions::default_cache_html_, "dch",
                  kDefaultCacheHtml, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): implement for mod_pagespeed.
  AddBaseProperty(kDefaultDomainShardCount,
                  &RewriteOptions::domain_shard_count_, "dsc",
                  kDomainShardCount, kQueryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(
      true, &RewriteOptions::modify_caching_headers_, "mch",
      kModifyCachingHeaders, kDirectoryScope,
      "Set to false to disallow mod_pagespeed from editing HTML "
      "Cache-Control headers. This is not safe in general and can cause "
      "the incorrect versions of HTML to be served to users.",
      true);

  // This is not Plain Old Data, so we initialize it here.
  const RewriteOptions::BeaconUrl kDefaultBeaconUrls = {
      kDefaultBeaconUrl, kDefaultBeaconUrl, kDefaultBeaconUrl,
      kDefaultBeaconUrl};
  AddBaseProperty(kDefaultBeaconUrls, &RewriteOptions::beacon_url_, "bu",
                  kBeaconUrl, kDirectoryScope,
                  "URL for beacon callback injected by add_instrumentation.",
                  false);

  // lazyload_images_after_onload_ is especially important for mobile,
  // where the recommendation is that you prefetch all the
  // necessary assets (burst your data), and then shutoff the radio to
  // preserve battery. Further, if the radio has been idle, and then
  // you scroll, then you'll have to incur the RRC upgrade cost, which
  // can be anywhere from 100ms-2.5s, which makes the site appear very
  // slowly.. and even worse if that triggers reflows.
  //
  // The problem on mobile is that everytime you wake up the radio, no
  // matter the size of the transfer, it then has to cycle through
  // the intermediate power states.. so even a tiny transfers results
  // in radio consuming power for 10s+.  So you incur unnecessary
  // latency, burn battery, etc.
  //
  // http://developer.android.com/training/efficient-downloads/efficient-network-access.html#PrefetchData
  AddBaseProperty(true, &RewriteOptions::lazyload_images_after_onload_, "llio",
                  kLazyloadImagesAfterOnload, kDirectoryScope,
                  "Wait until page onload before loading lazy images", true);

  AddBaseProperty("", &RewriteOptions::request_option_override_, "roo",
                  kRequestOptionOverride, kDirectoryScope,
                  "Token passed in URL to enable pagespeed options in params.",
                  false);
  AddBaseProperty("", &RewriteOptions::url_signing_key_, "usk", kUrlSigningKey,
                  kServerScope,
                  "Key used for signing .pagespeed resource URLs.", false);
  AddBaseProperty(false, &RewriteOptions::accept_invalid_signatures_, "ais",
                  kAcceptInvalidSignatures, kServerScope,
                  "Accept resources with invalid signatures.", false);
  AddBaseProperty(Timer::kSecondMs,
                  &RewriteOptions::remote_configuration_timeout_ms_, "rcfgt",
                  kRemoteConfigurationTimeoutMs, kServerScope,
                  "Timeout for fetch of remote configuration file.", true);
  AddBaseProperty("", &RewriteOptions::remote_configuration_url_, "rcfgu",
                  kRemoteConfigurationUrl, kDirectoryScope,
                  "URL of site from which to pull remote configuration files",
                  true);
  AddBaseProperty(
      9, &RewriteOptions::http_cache_compression_level_, "hccl",
      kHttpCacheCompressionLevel, kServerScope,
      "Compression level for HTTPCache. [-1-9] where 0 is off, 1 is minimum"
      "compression, and 9 (the default) is maximum compression.",
      true);
  AddBaseProperty(
      "", &RewriteOptions::lazyload_images_blank_url_, "llbu",
      kLazyloadImagesBlankUrl, kDirectoryScope,
      "URL of image used to display prior to loading the lazy image. "
      "Empty means use a site-local copy.",
      true);
  AddBaseProperty(false, &RewriteOptions::use_blank_image_for_inline_preview_,
                  "biip", kUseBlankImageForInlinePreview, kDirectoryScope,
                  "Use a blank image for inline preview", true);
  AddBaseProperty(true, &RewriteOptions::inline_only_critical_images_, "ioci",
                  kInlineOnlyCriticalImages, kDirectoryScope,
                  "Inline only critical images", true);
  AddBaseProperty(
      ResourceCategorySet(),
      &RewriteOptions::inline_unauthorized_resource_types_, "irwea",
      kInlineResourcesWithoutExplicitAuthorization, kDirectoryScope,
      "Specifies the resource types that can be inlined into HTML even if "
      "they do not belong to explicitly authorized domains.",
      true);
  AddBaseProperty(
      false, &RewriteOptions::domain_rewrite_cookies_, "drc",
      kDomainRewriteCookies, kDirectoryScope,
      "Allow rewrite_domains to rewrite domains in Set-Cookie headers.", true);
  AddBaseProperty(
      false, &RewriteOptions::domain_rewrite_hyperlinks_, "drh",
      kDomainRewriteHyperlinks, kDirectoryScope,
      "Allow rewrite_domains to rewrite <form> and <a> tags in addition "
      "to resource tags.",
      true);
  AddBaseProperty(false, &RewriteOptions::client_domain_rewrite_, "cdr",
                  kClientDomainRewrite, kDirectoryScope,
                  "Allow rewrite_domains to rewrite urls on the client side.",
                  true);
  AddBaseProperty(
      kDefaultImageJpegRecompressQuality,
      &RewriteOptions::image_jpeg_recompress_quality_, "iq",
      kImageJpegRecompressionQuality, kQueryScope,
      "Set quality parameter for recompressing jpeg images [-1,100], "
      "100 is lossless, -1 uses ImageRecompressionQuality",
      true);
  // Use kDefaultImageJpegRecompressQuality as default.
  AddBaseProperty(
      kDefaultImageJpegRecompressQualityForSmallScreens,
      &RewriteOptions::image_jpeg_recompress_quality_for_small_screens_, "iqss",
      kImageJpegRecompressionQualityForSmallScreens, kQueryScope,
      "Set quality parameter for recompressing jpeg images for small "
      "screens. [-1,100], 100 refers to best quality, -1 falls back to "
      "ImageJpegRecompressionQuality.",
      true);
  AddBaseProperty(
      kDefaultImageJpegQualityForSaveData,
      &RewriteOptions::image_jpeg_quality_for_save_data_, "iqsd",
      kImageJpegQualityForSaveData, kQueryScope,
      "Set quality for the images which will be optimized to JPEG format in "
      "the Save-Data mode. Use a value in [0,100] to explicitly set the "
      "quality. Use -1 to ignore the Save-Data header.",
      true);
  AddBaseProperty(
      kDefaultImageRecompressQuality,
      &RewriteOptions::image_recompress_quality_, "irq",
      kImageRecompressionQuality, kQueryScope,
      "Set quality parameter for recompressing images [-1,100], "
      "100 refers to best quality, -1 disables lossy compression. "
      "JpegRecompressionQuality and WebpRecompressionQuality override "
      "this.",
      true);
  AddBaseProperty(
      kDefaultImageLimitOptimizedPercent,
      &RewriteOptions::image_limit_optimized_percent_, "ip",
      kImageLimitOptimizedPercent, kDirectoryScope,
      "Replace images whose size after recompression is less than the "
      "given percent of original image size; 100 means replace if "
      "smaller.",
      true);
  AddBaseProperty(
      kDefaultImageLimitRenderedAreaPercent,
      &RewriteOptions::image_limit_rendered_area_percent_, "ira",
      kImageLimitRenderedAreaPercent, kDirectoryScope,
      "Limit on percentage of rendered image wxh to the original "
      "image wxh that should be stored in the property cache. This is to "
      "avoid corner cases where rounding off decreases the rendered "
      "image size by a few pixels.",
      true);
  AddBaseProperty(
      kDefaultImageLimitResizeAreaPercent,
      &RewriteOptions::image_limit_resize_area_percent_, "ia",
      kImageLimitResizeAreaPercent, kDirectoryScope,
      "Consider resizing images whose area in pixels is less than the "
      "given percent of original image area; 100 means replace if "
      "smaller.",
      true);
  AddBaseProperty(
      kDefaultImageWebpRecompressQuality,
      &RewriteOptions::image_webp_recompress_quality_, "iw",
      kImageWebpRecompressionQuality, kQueryScope,
      "Quality for rewritten webp images [-1,100], 100 refers to best quality, "
      "-1 uses ImageRecompressionQuality.",
      true);
  // Use kDefaultImageWebpRecompressQuality as default.
  AddBaseProperty(
      kDefaultImageWebpRecompressQualityForSmallScreens,
      &RewriteOptions::image_webp_recompress_quality_for_small_screens_, "iwss",
      kImageWebpRecompressionQualityForSmallScreens, kQueryScope,
      "Quality for rewritten webp images for small screens. [-1,100], "
      "100 refers to best quality, -1 falls back to "
      "WebpRecompressionQuality.",
      true);
  AddBaseProperty(
      kDefaultImageWebpAnimatedRecompressQuality,
      &RewriteOptions::image_webp_animated_recompress_quality_, "iwa",
      kImageWebpAnimatedRecompressionQuality, kQueryScope,
      "Quality for rewritten animated webp images [-1,100], "
      "100 refers to best quality, -1 uses ImageRecompressionQuality.",
      true);
  AddBaseProperty(
      kDefaultImageWebpQualityForSaveData,
      &RewriteOptions::image_webp_quality_for_save_data_, "iwsd",
      kImageWebpQualityForSaveData, kQueryScope,
      "Set quality for the images which will be optimized to lossy WebP "
      "format in the Save-Data mode. Use a value in [0,100] to explicitly set "
      "the quality. Use -1 to ignore the Save-Data header.",
      true);
  AddBaseProperty(kDefaultImageWebpTimeoutMs,
                  &RewriteOptions::image_webp_timeout_ms_, "wt",
                  kImageWebpTimeoutMs, kLegacyProcessScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(
      kDefaultMaxInlinedPreviewImagesIndex,
      &RewriteOptions::max_inlined_preview_images_index_, "mdii",
      kMaxInlinedPreviewImagesIndex, kDirectoryScope,
      "Number of first N images for which low resolution image is "
      "generated. Negative values result in generation for all images.",
      true);
  AddBaseProperty(kDefaultMinImageSizeLowResolutionBytes,
                  &RewriteOptions::min_image_size_low_resolution_bytes_,
                  "nislr", kMinImageSizeLowResolutionBytes, kDirectoryScope,
                  "Minimum image size above which low resolution image is "
                  "generated.",
                  true);
  AddBaseProperty(kDefaultMaxImageSizeLowResolutionBytes,
                  &RewriteOptions::max_image_size_low_resolution_bytes_,
                  "xislr", kMaxImageSizeLowResolutionBytes, kDirectoryScope,
                  "Maximum image size below which low resolution image is "
                  "generated.",
                  true);
  AddBaseProperty(
      kDefaultFinderPropertiesCacheExpirationTimeMs,
      &RewriteOptions::finder_properties_cache_expiration_time_ms_, "fpce",
      kFinderPropertiesCacheExpirationTimeMs, kDirectoryScope,
      "Number of ms that beacon results for the critical selector finders "
      "should be considered valid.",
      true);
  AddBaseProperty(kDefaultFinderPropertiesCacheRefreshTimeMs,
                  &RewriteOptions::finder_properties_cache_refresh_time_ms_,
                  "fpcr", kFinderPropertiesCacheRefreshTimeMs, kDirectoryScope,
                  nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(kDefaultExperimentCookieDurationMs,
                  &RewriteOptions::experiment_cookie_duration_ms_, "fcd",
                  kExperimentCookieDurationMs, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(
      kDefaultImageJpegNumProgressiveScans,
      &RewriteOptions::image_jpeg_num_progressive_scans_, "ijps",
      kImageJpegNumProgressiveScans, kDirectoryScope,
      "Number of progressive scans [1,10] to emit when rewriting images as "
      "ten-scan progressive jpegs. "
      "A value of -1 outputs all progressive scans.",
      true);
  // Use kDefaultImageJpegNumProgressiveScans as default.
  AddBaseProperty(
      kDefaultImageJpegNumProgressiveScans,
      &RewriteOptions::image_jpeg_num_progressive_scans_for_small_screens_,
      "ijpst", kImageJpegNumProgressiveScansForSmallScreens, kDirectoryScope,
      "Number of progressive scans [1,10] to emit when rewriting images as"
      "ten-scan progressive jpegs for small screens. A value of -1 falls "
      "back to kImageJpegNumProgressiveScans.",
      true);
  AddBaseProperty(false, &RewriteOptions::cache_small_images_unrewritten_,
                  "csiu", kCacheSmallImagesUnrewritten, kDirectoryScope,
                  nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(kDefaultImageResolutionLimitBytes,
                  &RewriteOptions::image_resolution_limit_bytes_, "irlb",
                  kImageResolutionLimitBytes, kDirectoryScope,
                  "Maximum byte size of an image for optimization", true);
  AddBaseProperty(
      0, &RewriteOptions::rewrite_random_drop_percentage_, "rrdp",
      kRewriteRandomDropPercentage, kDirectoryScope,
      "The percentage of time that pagespeed should randomly drop an "
      "opportunity to optimize an image.  The value should be an integer "
      "between 0 and 100 inclusive.",
      true);
  AddBaseProperty("", &RewriteOptions::ga_id_, "ig", kAnalyticsID,
                  kDirectoryScope, "Google Analytics ID to use on site.", true);
  AddBaseProperty("", &RewriteOptions::content_experiment_id_, "cxid",
                  kContentExperimentID, kDirectoryScope,
                  "Which Google Analytics content experiment to log to.", true);
  AddBaseProperty(
      "", &RewriteOptions::content_experiment_variant_id_, "cxvid",
      kContentExperimentVariantID, kDirectoryScope,
      "Which Google Analytics content experiment variant to log to.", true);
  AddBaseProperty(true, &RewriteOptions::use_analytics_js_, "uajs",
                  kUseAnalyticsJs, kQueryScope,
                  "Log to analytics.js instead of ga.js with insert_ga.", true);
  AddBaseProperty(true, &RewriteOptions::increase_speed_tracking_, "st",
                  kIncreaseSpeedTracking, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(false, &RewriteOptions::running_experiment_, "fur",
                  kRunningExperiment, kDirectoryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(kDefaultExperimentSlot, &RewriteOptions::experiment_ga_slot_,
                  "fga", kExperimentSlot, kDirectoryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(experiment::kForceNoExperiment,
                  &RewriteOptions::enroll_experiment_id_, "eeid",
                  kEnrollExperiment, kQueryScope,
                  "Assign users to a specific experiment setting.", true);
  AddBaseProperty(false, &RewriteOptions::report_unload_time_, "rut",
                  kReportUnloadTime, kDirectoryScope,
                  "If set reports optional page unload time.", true);
  AddBaseProperty("", &RewriteOptions::x_header_value_, "xhv",
                  kXModPagespeedHeaderValue, kDirectoryScope,
                  "Set the value for the X-Mod-Pagespeed HTTP header", true);
  AddBaseProperty(
      true, &RewriteOptions::avoid_renaming_introspective_javascript_, "aris",
      kAvoidRenamingIntrospectiveJavascript, kDirectoryScope,
      "Don't combine, inline, cache extend, or otherwise modify "
      "javascript in ways that require changing the URL if we see "
      "introspection in the form of "
      "document.getElementsByTagName('script').",
      true);
  AddBaseProperty(false, &RewriteOptions::reject_blacklisted_, "rbl",
                  kRejectBlacklisted, kDirectoryScope, nullptr,
                  false);  // Not applicable for mod_pagespeed.
  AddBaseProperty(HttpStatus::kForbidden,
                  &RewriteOptions::reject_blacklisted_status_code_, "rbls",
                  kRejectBlacklistedStatusCode, kDirectoryScope, nullptr,
                  false);  // Not applicable for mod_pagespeed.
  AddBaseProperty(
      kDefaultBlockingRewriteKey, &RewriteOptions::blocking_rewrite_key_,
      "blrw", kXPsaBlockingRewrite, kServerScope,
      "If the X-PSA-Pagespeed-Blocking-Rewrite header is present, and "
      "its value matches the configured value, ensure that all "
      "rewrites are completed before sending the response to the "
      "client.",
      false);
  AddBaseProperty(
      false, &RewriteOptions::use_fallback_property_cache_values_, "fbcv",
      kUseFallbackPropertyCacheValues, kServerScope,
      "If this is set to true, fallback values will be used from property "
      "cache if actual value is not present. Here fallback values means "
      "properties which are shared across all requests which have same url "
      "if query paramaters are removed. Example: http://www.test.com?a=1 and "
      "http://www.test.com?a=2 share same fallback properties though they "
      "are two different urls.",
      true);
  AddBaseProperty(false, &RewriteOptions::await_pcache_lookup_, "wpcl",
                  kAwaitPcacheLookup, kServerScope, nullptr, true);
  AddBaseProperty(true, &RewriteOptions::support_noscript_enabled_, "snse",
                  kSupportNoScriptEnabled, kDirectoryScope,
                  "Support for clients with no script support, in filters that "
                  "insert new javascript.",
                  true);
  AddBaseProperty(
      false, &RewriteOptions::enable_extended_instrumentation_, "eei",
      kEnableExtendedInstrumentation, kDirectoryScope,
      "If set to true, addition instrumentation js is added to that page that "
      "the beacon can collect more information.",
      true);
  AddBaseProperty(
      true, &RewriteOptions::use_experimental_js_minifier_, "uejsm",
      kUseExperimentalJsMinifier, kDirectoryScope,
      "If set to false, uses the old legacy::MinifyJs-based minifier. "
      "This option will be deprecated once we do a successful release with the "
      "new minifier.",
      true);
  AddBaseProperty(kDefaultMaxCombinedCssBytes,
                  &RewriteOptions::max_combined_css_bytes_, "xcc",
                  kMaxCombinedCssBytes, kQueryScope,
                  "Maximum size allowed for the combined CSS resource.", true);
  AddBaseProperty(
      kDefaultMaxCombinedJsBytes, &RewriteOptions::max_combined_js_bytes_,
      "xcj", kMaxCombinedJsBytes, kDirectoryScope,
      "Maximum size allowed for the combined JavaScript resource.", true);
  // Currently not applicable for mod_pagespeed.
  AddBaseProperty(-1, &RewriteOptions::override_caching_ttl_ms_, "octm",
                  kOverrideCachingTtlMs, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(5 * Timer::kSecondMs,
                  &RewriteOptions::blocking_fetch_timeout_ms_, "bfto",
                  RewriteOptions::kFetcherTimeOutMs, kDirectoryScope, nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(false, &RewriteOptions::enable_prioritizing_scripts_, "eps",
                  kEnablePrioritizingScripts, kDirectoryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddRequestProperty("", &RewriteOptions::pre_connect_url_, "pcu", true);
  AddRequestProperty(
      kDefaultPropertyCacheHttpStatusStabilityThreshold,
      &RewriteOptions::property_cache_http_status_stability_threshold_,
      "pchsst", false);
  AddBaseProperty(kDefaultMaxRewriteInfoLogSize,
                  &RewriteOptions::max_rewrite_info_log_size_, "mrils",
                  kMaxRewriteInfoLogSize, kDirectoryScope, nullptr,
                  false);  // Not applicable for mod_pagespeed.
  AddBaseProperty(kDefaultMetadataCacheStalenessThresholdMs,
                  &RewriteOptions::metadata_cache_staleness_threshold_ms_,
                  "mcst", kMetadataCacheStalenessThresholdMs, kDirectoryScope,
                  nullptr,
                  true);  // TODO(jmarantz): write help & doc for mod_pagespeed.
  AddBaseProperty(
      kDefaultDownstreamCachePurgeMethod,
      &RewriteOptions::downstream_cache_purge_method_, "dcpm",
      kDownstreamCachePurgeMethod, kDirectoryScope,
      "Method to be used for purging responses from the downstream cache",
      false);
  AddBaseProperty(
      "", &RewriteOptions::downstream_cache_rebeaconing_key_, "dcrk",
      kDownstreamCacheRebeaconingKey, kDirectoryScope,
      "The key used to authenticate rebeaconing requests from downstream "
      "caches. The value specified for this key in the pagespeed server "
      "config should be used in the caching layer configuration also.",
      false);
  AddBaseProperty(
      kDefaultDownstreamCacheRewrittenPercentageThreshold,
      &RewriteOptions::downstream_cache_rewritten_percentage_threshold_,
      "dcrpt", kDownstreamCacheRewrittenPercentageThreshold, kDirectoryScope,
      "Threshold for percentage of rewriting to be finished before the "
      "response is served out and simultaneously stored in the downstream "
      "cache, beyond which the response will not be purged from the cache even"
      "if more rewriting is possible now",
      true);
  AddRequestProperty(kDefaultMetadataInputErrorsCacheTtlMs,
                     &RewriteOptions::metadata_input_errors_cache_ttl_ms_,
                     "mect", true);
  AddRequestProperty(false, &RewriteOptions::override_ie_document_mode_, "oidm",
                     true);
  AddBaseProperty("", &RewriteOptions::amp_link_pattern_, "alp",
                  kAmpLinkPattern, kDirectoryScope, nullptr,
                  true);  // Not applicable for mod_pagespeed.
  AddBaseProperty(true, &RewriteOptions::honor_csp_, "hcsp", kHonorCsp,
                  kServerScope,
                  "Controls whether PageSpeed should pay attention to "
                  "Content-Security-Policy directives",
                  false);

  // Note: defer_javascript and defer_iframe were previously not
  // trusted on mobile user-agents, but have now matured to the point
  // where we should trust them by default.  The mod_pagespeed
  // config-file setting "ModPagespeedEnableAggressiveRewritersForMobile"
  // will work, but we will omit it from the documentation because we
  // are enabling it by default.
  AddBaseProperty(
      true, &RewriteOptions::enable_aggressive_rewriters_for_mobile_, "earm",
      kEnableAggressiveRewritersForMobile, kDirectoryScope,
      "Allows defer_javascript and defer_iframe for mobile browsers", true);

  AddBaseProperty(false, &RewriteOptions::serve_xhr_access_control_headers_,
                  "shach", kServeXhrAccessControlHeaders, kDirectoryScope,
                  "Serve access control headers with response headers", false);

  AddBaseProperty(
      "", &RewriteOptions::access_control_allow_origins_, "acao",
      kAccessControlAllowOrigins, kDirectoryScope,
      "Comma separated list of origins that are allowed to make cross-origin "
      "requests",
      false);

  AddBaseProperty(false, &RewriteOptions::hide_referer_using_meta_, "hrum",
                  kHideRefererUsingMeta, kDirectoryScope,
                  "Hides the referer by adding meta tag to the HTML", true);

  AddBaseProperty(false, &RewriteOptions::preserve_subresource_hints_, "psrh",
                  kPreserveSubresourceHints, kQueryScope,
                  "Keep original subresource hints in place.", true);

  AddBaseProperty(
      true, &RewriteOptions::preserve_url_relativity_, "pur",
      kPreserveUrlRelativity, kDirectoryScope,
      "Keep rewritten URLs as relative as the original resource URL was.",
      true);

  AddBaseProperty(false, &RewriteOptions::allow_logging_urls_in_log_record_,
                  "alulr", kAllowLoggingUrlsInLogRecord, kDirectoryScope,
                  nullptr,
                  false);  // Not applicable for mod_pagespeed.

  AddBaseProperty(
      true, &RewriteOptions::allow_options_to_be_set_by_cookies_, "aotbsbc",
      kAllowOptionsToBeSetByCookies, kDirectoryScope,
      "Allow options to be set by cookies in addition to query parameters "
      "and request headers.",
      true);

  AddBaseProperty("", &RewriteOptions::non_cacheables_for_cache_partial_html_,
                  "nccp", kNonCacheablesForCachePartialHtml, kDirectoryScope,
                  nullptr,
                  false);  // Not applicable for mod_pagespeed.

  AddBaseProperty(
      false, &RewriteOptions::no_transform_optimized_images_, "ntoi",
      kNoTransformOptimizedImages, kDirectoryScope,
      "Add no-transform header to cache-control for optimized images", true);

  AddBaseProperty(
      kDefaultMaxLowResImageSizeBytes,
      &RewriteOptions::max_low_res_image_size_bytes_, "lris",
      kMaxLowResImageSizeBytes, kDirectoryScope, nullptr,
      true);  // TODO(bharathbhushan): write help & doc for mod_pagespeed.

  AddBaseProperty(
      kDefaultMaxLowResToFullResImageSizePercentage,
      &RewriteOptions::max_low_res_to_full_res_image_size_percentage_, "lrhrs",
      kMaxLowResToHighResImageSizePercentage, kDirectoryScope, nullptr,
      true);  // TODO(bharathbhushan): write help & doc for mod_pagespeed.

  AddBaseProperty(true,
                  &RewriteOptions::serve_rewritten_webp_urls_to_any_agent_,
                  "swaa", kServeWebpToAnyAgent, kDirectoryScope,
                  "Serve rewritten .webp images to any user-agent", true);

  AddBaseProperty(
      "", &RewriteOptions::cache_fragment_, "ckp", kCacheFragment,
      kDirectoryScope,
      "Set a cache fragment to allow servers with different hostnames to "
      "share a cache.  Allowed: letters, numbers, underscores, and hyphens.",
      false);

  AddBaseProperty(
      "", &RewriteOptions::sticky_query_parameters_, "sqp",
      kStickyQueryParameters, kDirectoryScope,
      "The token that must be set by the PageSpeedStickyQueryParameters query "
      "parameter/header in a request to enable the setting of cookies for all "
      "other PageSpeed query parameters/headers in the request. Blank means "
      "it is disabled.",
      false);
  AddBaseProperty(kDefaultOptionCookiesDurationMs,
                  &RewriteOptions::option_cookies_duration_ms_, "ocd",
                  kOptionCookiesDurationMs, kDirectoryScope,
                  "The max-age in ms of cookies that set PageSpeed options.",
                  true);

  ResponsiveDensities default_densities;
  default_densities.assign(kDefaultResponsiveImageDensities,
                           kDefaultResponsiveImageDensities +
                               arraysize(kDefaultResponsiveImageDensities));
  AddBaseProperty(default_densities,
                  &RewriteOptions::responsive_image_densities_, "rid",
                  kResponsiveImageDensities, kDirectoryScope,
                  "Comma separated list of screen densities to target with "
                  "ResponsiveImageFilter srcsets.",
                  true);

  AllowVaryOn default_allow_vary_on;
  ParseFromString(AllowVaryOn::kAutoString, &default_allow_vary_on);
  AddBaseProperty(
      default_allow_vary_on, &RewriteOptions::allow_vary_on_, "avo",
      kAllowVaryOn, kQueryScope,
      "\"Auto\", \"None\", or comma separated list of strings chosen from "
      "\"Save-Data\", \"User-Agent\", and \"Accept\".",
      true);

  // Test-only, so no enum.
  AddRequestProperty(false,
                     &RewriteOptions::test_instant_fetch_rewrite_deadline_,
                     "tifrwd", false);
  // We need to exclude this test-only option from signature, since we may need
  // to change it in the middle of tests.
  properties_->property(properties_->size() - 1)
      ->set_do_not_use_for_signature_computation(true);

  AddBaseProperty(
      0, &RewriteOptions::noop_, "noop", kNoop, kQueryScope,
      "Meaningless integer option for browser cache-busting in query-params",
      true);
  properties_->property(properties_->size() - 1)
      ->set_do_not_use_for_signature_computation(true);

  // Some options are removed, but we recognize their names for backwards
  // compatibility with config files that still have them.
  AddDeprecatedProperty("MaxPrefetchJsElements", kDirectoryScope);
  AddDeprecatedProperty("DistributeFetches", kServerScope);
  AddDeprecatedProperty("DistributedRewriteKey", kServerScope);
  AddDeprecatedProperty("DistributedRewriteServers", kServerScope);
  AddDeprecatedProperty("DistributedRewriteTimeoutMs", kServerScope);
  // No need for DistributableFilters, since nothing actually registered it with
  // the hosting server.

  // Recently sriharis@ excluded a variety of options from
  // signature-computation which makes sense from the perspective
  // of metadata cache, however it makes Signature() useless for
  // determining equivalence of RewriteOptions.  This equivalence
  // is needed in ServerContext::NewRewriteDriver to determine
  // whether the drivers in the freelist are still applicable, or
  // whether options have changed.
  //
  // So we need to either compute two signatures: one for equivalence
  // and one for metadata cache key, or just use the more comprehensive
  // one for metadata_cache.  We should determine whether we are getting
  // spurious cache fragmentation before investing in computing two
  // signatures.
  //
  // Commenting these out for now.
  //
  // In particular, ProxyInterfaceTest.AjaxRewritingForCss will fail
  // if we don't let in_place_rewriting_enabled_ affect the signature.
  //
  // TODO(jmarantz): consider whether there's any measurable benefit
  // from excluding these options from the signature.  If there is,
  // make 2 signatures: one for equivalence & one for metadata cache
  // keys.  If not, just remove the DoNotUseForSignatureComputation
  // infrastructure.
  //
  // in_place_rewriting_enabled_.DoNotUseForSignatureComputation();
  // log_background_rewrites_.DoNotUseForSignatureComputation();
  // log_rewrite_timing_.DoNotUseForSignatureComputation();
  // log_url_indices_.DoNotUseForSignatureComputation();
  // serve_stale_if_fetch_error_.DoNotUseForSignatureComputation();
  // enable_defer_js_experimental_.DoNotUseForSignatureComputation();
  // default_cache_html_.DoNotUseForSignatureComputation();
  // lazyload_images_after_onload_.DoNotUseForSignatureComputation();
  // ga_id_.DoNotUseForSignatureComputation();
  // increase_speed_tracking_.DoNotUseForSignatureComputation();
  // running_experiment_.DoNotUseForSignatureComputation();
  // x_header_value_.DoNotUseForSignatureComputation();
  // blocking_fetch_timeout_ms_.DoNotUseForSignatureComputation();
}  // NOLINT  (large function)

RewriteOptions::~RewriteOptions() {
  STLDeleteElements(&resource_headers_);
  STLDeleteElements(&custom_fetch_headers_);
  STLDeleteElements(&experiment_specs_);
  STLDeleteElements(&url_cache_invalidation_entries_);
  STLDeleteValues(&rejected_request_map_);
}  // NOLINT

void RewriteOptions::InitializeOptions(const Properties* properties) {
  all_options_.resize(all_properties_->size());

  // Note that we reserve space in all_options_ for all RewriteOptions
  // and subclass properties, but we initialize only the options
  // corresponding to the ones passed into this method, whether from
  // RewriteOptions or a subclass.
  //
  // This is because the member variables for the subclass properties
  // have not been constructed yet, so copying default values into
  // them would crash (at least the strings).  So we rely on subclass
  // constructors to initialize their own options by calling
  // InitializeOptions on their own property sets as well.
  for (int i = 0, n = properties->size(); i < n; ++i) {
    const PropertyBase* property = properties->property(i);
    property->InitializeOption(this);
  }
  initialized_options_ += properties->size();
}

RewriteOptions::OptionBase::~OptionBase() {}

RewriteOptions::Properties::Properties()
    : initialization_count_(1), owns_properties_(true) {}

RewriteOptions::Properties::~Properties() {
  if (owns_properties_) {
    STLDeleteElements(&property_vector_);
  }
}

RewriteOptions::PropertyBase::~PropertyBase() {}

bool RewriteOptions::Properties::Initialize(Properties** properties_handle) {
  Properties* properties = *properties_handle;
  if (properties == nullptr) {
    *properties_handle = new Properties;
    return true;
  }
  ++(properties->initialization_count_);
  return false;
}

void RewriteOptions::Properties::Merge(Properties* properties) {
  // We merge all subclass properties up into RewriteOptions::all_properties_.
  //   RewriteOptions::properties_.owns_properties_ is true.
  //   RewriteOptions::all_properties_.owns_properties_ is false.
  DCHECK(properties->owns_properties_);
  owns_properties_ = false;
  property_vector_.reserve(size() + properties->size());
  property_vector_.insert(property_vector_.end(),
                          properties->property_vector_.begin(),
                          properties->property_vector_.end());
  std::sort(property_vector_.begin(), property_vector_.end(),
            RewriteOptions::PropertyLessThanByOptionName);
  for (int i = 0, n = property_vector_.size(); i < n; ++i) {
    property_vector_[i]->set_index(i);
  }
}

bool RewriteOptions::Properties::Terminate(Properties** properties_handle) {
  Properties* properties = *properties_handle;
  DCHECK_GT(properties->initialization_count_, 0);
  if (--(properties->initialization_count_) == 0) {
    delete properties;
    *properties_handle = nullptr;
    return true;
  }
  return false;
}

bool RewriteOptions::Initialize() {
  if (Properties::Initialize(&properties_)) {
    Properties::Initialize(&all_properties_);
    Properties::Initialize(&deprecated_properties_);
    AddProperties();
    InitFilterIdToEnumArray();
    all_properties_->Merge(properties_);
    InitOptionIdToPropertyArray();
    InitOptionNameToPropertyArray();
    InitFixedResourceHeaders();

    for (int f = 0; f < static_cast<int>(RewriteOptions::kEndOfFilters); ++f) {
      RewriteOptions::Filter filter = static_cast<RewriteOptions::Filter>(f);
      FilterProperties* property = &filter_properties[f];
      property->level_core =
          IsInSet(kCoreFilterSet, arraysize(kCoreFilterSet), filter);
      property->level_optimize_for_bandwidth =
          IsInSet(kOptimizeForBandwidthFilterSet,
                  arraysize(kOptimizeForBandwidthFilterSet), filter);
      property->level_mobilize =
          IsInSet(kMobilizeFilterSet, arraysize(kMobilizeFilterSet), filter);
      property->level_test =
          IsInSet(kTestFilterSet, arraysize(kTestFilterSet), filter);
      property->level_dangerous =
          IsInSet(kDangerousFilterSet, arraysize(kDangerousFilterSet), filter);
      property->preserve_js_urls =
          IsInSet(kJsPreserveUrlDisabledFilters,
                  arraysize(kJsPreserveUrlDisabledFilters), filter);
      property->preserve_css_urls =
          IsInSet(kCssPreserveUrlDisabledFilters,
                  arraysize(kCssPreserveUrlDisabledFilters), filter);
      property->preserve_image_urls =
          IsInSet(kImagePreserveUrlDisabledFilters,
                  arraysize(kImagePreserveUrlDisabledFilters), filter);
    }

    return true;
  }
  return false;
}

void RewriteOptions::InitFilterIdToEnumArray() {
  // Sanity-checks -- will be active only when compiled for debug.
#ifndef NDEBUG
  // The forward map must have an entry for every Filter enum value except
  // the sentinel (kEndOfFilters) and they must be in order.
  DCHECK_EQ(arraysize(kFilterVectorStaticInitializer),
            static_cast<size_t>(kEndOfFilters));
  for (int i = 0, n = arraysize(kFilterVectorStaticInitializer); i < n; ++i) {
    DCHECK_EQ(i,
              static_cast<int>(kFilterVectorStaticInitializer[i].filter_enum));
  }
  // The reverse map must have the same number of elements as the forward map.
  DCHECK_EQ(arraysize(kFilterVectorStaticInitializer),
            arraysize(filter_id_to_enum_array_));
#endif
  // Initialize the reverse map.
  for (int i = 0, n = arraysize(kFilterVectorStaticInitializer); i < n; ++i) {
    filter_id_to_enum_array_[i] = &kFilterVectorStaticInitializer[i];
  }
  std::sort(filter_id_to_enum_array_,
            filter_id_to_enum_array_ + arraysize(filter_id_to_enum_array_),
            RewriteOptions::FilterEnumToIdAndNameEntryLessThanById);
}

struct RewriteOptions::OptionIdCompare {
  bool operator()(const PropertyBase* a, StringPiece b) const {
    return StringCaseCompare(a->id(), b) < 0;
  }
  bool operator()(StringPiece a, const PropertyBase* b) const {
    return StringCaseCompare(a, b->id()) < 0;
  }
  bool operator()(const PropertyBase* a, const PropertyBase* b) const {
    return StringCaseCompare(a->id(), b->id()) < 0;
  }
};

void RewriteOptions::InitOptionIdToPropertyArray() {
  // This method is called first by Initialize, when base properties are
  // added, then zero or more times when subclass properties are added by
  // MergeSubclassProperties (e.g. by ApacheConfig::AddProperties).
  delete[] option_id_to_property_array_;
  option_id_to_property_array_ =
      new const PropertyBase*[all_properties_->size()];
  for (int i = 0, n = all_properties_->size(); i < n; ++i) {
    option_id_to_property_array_[i] = all_properties_->property(i);
  }
  std::sort(option_id_to_property_array_,
            option_id_to_property_array_ + all_properties_->size(),
            OptionIdCompare());
}

void RewriteOptions::InitOptionNameToPropertyArray() {
  // This method is called first by Initialize, when base properties are
  // added, then zero or more times when subclass properties are added by
  // MergeSubclassProperties (e.g. by ApacheConfig::AddProperties).
  delete option_name_to_property_map_;
  option_name_to_property_map_ = new PropertyNameMap;
  for (int i = 0, n = all_properties_->size(); i < n; ++i) {
    PropertyBase* prop = all_properties_->property(i);
    StringPiece name(prop->option_name());
    if (!name.empty()) {
      option_name_to_property_map_->insert(
          PropertyNameMap::value_type(name, prop));
    }
  }
}

void RewriteOptions::InitFixedResourceHeaders() {
  StringPieceVector* tmp = new StringPieceVector();
  tmp->push_back(HttpAttributes::kAcceptRanges);
  tmp->push_back(HttpAttributes::kCacheControl);
  tmp->push_back(HttpAttributes::kContentEncoding);
  tmp->push_back(HttpAttributes::kContentLength);
  tmp->push_back(HttpAttributes::kContentType);
  tmp->push_back(HttpAttributes::kDate);
  tmp->push_back(HttpAttributes::kEtag);
  tmp->push_back(HttpAttributes::kExpires);
  tmp->push_back(HttpAttributes::kLastModified);
  tmp->push_back(HttpAttributes::kLink);
  tmp->push_back(HttpAttributes::kServer);
  tmp->push_back(HttpAttributes::kVary);
  fixed_resource_headers = tmp;

  // From <https://www.ietf.org/rfc/rfc2616.txt>
  // token          = 1*<any CHAR except CTLs or separators>
  // separators     = "(" | ")" | "<" | ">" | "@"
  //                      | "," | ";" | ":" | "\" | <">
  //                      | "/" | "[" | "]" | "?" | "="
  //                      | "{" | "}" | SP | HT
  const GoogleString separators("()<>@,;:\\\"/[]?={} \t");
  for (int i = 0, n = separators.size(); i < n; ++i) {
    http_header_separators[static_cast<unsigned int>(separators.at(i))] = true;
  }
}

bool RewriteOptions::Terminate() {
  if (fixed_resource_headers != nullptr) {
    delete fixed_resource_headers;
    fixed_resource_headers = nullptr;
  }
  if (Properties::Terminate(&properties_)) {
    DCHECK(option_id_to_property_array_ != nullptr);
    delete[] option_id_to_property_array_;
    option_id_to_property_array_ = nullptr;
    DCHECK(option_name_to_property_map_ != nullptr);
    option_name_to_property_map_->clear();
    delete option_name_to_property_map_;
    option_name_to_property_map_ = nullptr;
    Properties::Terminate(&all_properties_);
    Properties::Terminate(&deprecated_properties_);
    return true;
  }
  return false;
}

void RewriteOptions::MergeSubclassProperties(Properties* properties) {
  all_properties_->Merge(properties);
  InitOptionIdToPropertyArray();
  InitOptionNameToPropertyArray();
}

bool RewriteOptions::SetExperimentState(int id) {
  experiment_id_ = id;
  return SetupExperimentRewriters();
}

void RewriteOptions::SetExperimentStateStr(
    const StringPiece& experiment_index) {
  if (experiment_index.length() == 1) {
    int index = experiment_index[0] - 'a';
    int n_experiment_specs = experiment_specs_.size();
    if (0 <= index && index < n_experiment_specs) {
      SetExperimentState(experiment_specs_[index]->id());
    }
  }
  // Ignore any calls with an invalid index-string.  When experiments are ended
  // a previously valid index string may become invalid.  For example, if a
  // webmaster were running an a/b/c test and now is running an a/b test, a
  // visitor refreshing an old image opened in a separate tab on the 'c' branch
  // of the experiment needs to get some version of that image and not an error.
  // Perhaps more commonly, a webmaster might manually copy a url from pagespeed
  // output to somewhere else on their site at a time an experiment was active,
  // and it would be bad to break that resource link when the experiment ended.
}

GoogleString RewriteOptions::GetExperimentStateStr() const {
  // Don't look at more than 26 experiment_specs because we use lowercase a-z.
  // While this is an arbitrary limit, it's much higher than webmasters are
  // likely to run into in practice.  Most of the time people will be running
  // a/b or a/b/c tests, and an a/b/c/d/.../y/z test would be unwieldy and
  // difficult to interpret.  If this does turn out to be needed we can switch
  // to base64 to get 64-way tests, and more than one character experiment index
  // strings would also be possible.
  for (int i = 0, n = experiment_specs_.size(); i < n && i < 26; ++i) {
    if (experiment_specs_[i]->id() == experiment_id_) {
      return GoogleString(1, static_cast<char>('a' + i));
    }
  }
  return "";
}

void RewriteOptions::DisallowTroublesomeResources() {
  // http://github.com/apache/incubator-pagespeed-mod/issues/38
  Disallow("*js_tinyMCE*");  // js_tinyMCE.js
  // Official tinyMCE URLs: tiny_mce.js, tiny_mce_src.js, tiny_mce_gzip.php, ...
  Disallow("*tiny_mce*");
  // I've also seen tinymce.js
  Disallow("*tinymce*");

  // http://github.com/apache/incubator-pagespeed-mod/issues/352
  Disallow("*scriptaculous.js*");

  // http://github.com/apache/incubator-pagespeed-mod/issues/186
  // ckeditor.js, ckeditor_basic.js, ckeditor_basic_source.js, ...
  Disallow("*ckeditor*");

  // https://github.com/apache/incubator-pagespeed-mod/issues/1405
  Disallow("*/wp-admin/*");

  // http://github.com/apache/incubator-pagespeed-mod/issues/207
  // jquery-ui-1.8.2.custom.min.js, jquery-1.4.4.min.js, jquery.fancybox-...
  //
  // TODO(sligocki): Is jquery actually a problem? Perhaps specific
  // jquery libraries (like tiny MCE). Investigate before disabling.
  // Disallow("*jquery*");

  // http://github.com/apache/incubator-pagespeed-mod/issues/216
  // Appears to be an issue with old version of jsminify.
  // Disallow("*swfobject*");  // swfobject.js

  // TODO(sligocki): Add disallow for the JS broken in:
  // http://github.com/apache/incubator-pagespeed-mod/issues/142
  // Not clear which JS file is broken and proxying is not working correctly.

  // Disable lazyload_images if there is another known lazyloader present.
  DisableLazyloadForClassName("*dfcg*");
  DisableLazyloadForClassName("*lazy*");
  DisableLazyloadForClassName("*nivo*");
  DisableLazyloadForClassName("*slider*");
}

// Note: this is not called by default in mod_pagespeed.
void RewriteOptions::DisallowResourcesForProxy() {
  Disallow("*://l.yimg.com/*");
  Disallow("*store.yahoo.net/*");

  // Changing the url breaks the simpleviewer flash-based slideshow gallery due
  // to cross domain policy violations.
  Disallow("*simpleviewer.js*");

  // Disable resources that are already being shared across multiple sites and
  // have strong CDN support (ie they are already cheap to fetch and are also
  // very likely to reside in the browser cache from visits to another site).
  // We keep these patterns as specific as possible while avoiding internal
  // wildcards.  Note that all of these urls have query parameters in long-tail
  // requests.
  // Do allow these to be inlined; if they're small enough it can be better to
  // inline them then fetch them from cache, and they're not always in cache.
  // TODO(jmaessen): Consider setting up the blacklist by domain name and using
  // regexps only after a match has been found.  Alternatively, since we're
  // setting up a binary choice here, consider using RE2 to make the yes/no
  // decision.
  AllowOnlyWhenInlining("*//ajax.googleapis.com/ajax/libs/*.js*");
  AllowOnlyWhenInlining("*//pagead2.googlesyndication.com/pagead/show_ads.js*");
  AllowOnlyWhenInlining(
      "*//partner.googleadservices.com/gampad/google_service.js*");
  AllowOnlyWhenInlining("*//platform.twitter.com/widgets.js*");
  AllowOnlyWhenInlining("*//s7.addthis.com/js/250/addthis_widget.js*");
  AllowOnlyWhenInlining("*//www.google.com/coop/cse/brand*");
  AllowOnlyWhenInlining("*//www.google-analytics.com/urchin.js*");
  AllowOnlyWhenInlining("*//www.googleadservices.com/pagead/conversion.js*");
  AllowOnlyWhenInlining("*connect.facebook.net/*");
}

bool RewriteOptions::EnableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(filters, &enabled_filters_,
                                               handler);
}

bool RewriteOptions::DisableFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(filters, &disabled_filters_,
                                               handler);
}

bool RewriteOptions::ForbidFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  return AddCommaSeparatedListToFilterSetState(filters, &forbidden_filters_,
                                               handler);
}

void RewriteOptions::DisableAllFilters() {
  DCHECK(!frozen_);
  modified_ = true;
  enabled_filters_.clear();
  SetRewriteLevel(RewriteOptions::kPassThrough);
  disabled_filters_.SetAll();
}

void RewriteOptions::DisableAllFiltersNotExplicitlyEnabled() {
  modified_ |= disabled_filters_.MergeInverted(enabled_filters_);
}

void RewriteOptions::EnableFilter(Filter filter) {
  DCHECK(!frozen_);
  modified_ |= enabled_filters_.Insert(filter);
}

void RewriteOptions::SoftEnableFilterForTesting(Filter filter) {
  // If we're already in 'all filters mode', then just enable the specified
  // filter.
  if (level_.value() == RewriteOptions::kAllFilters) {
    disabled_filters_.Erase(filter);
    forbidden_filters_.Erase(filter);
  } else {
    // Keep track of any filters that were enabled already.
    RewriteOptions::FilterSet already_enabled;
    already_enabled.Insert(filter);
    for (int i = 0; i < RewriteOptions::kEndOfFilters; ++i) {
      RewriteOptions::Filter filter = static_cast<RewriteOptions::Filter>(i);
      if (Enabled(filter)) {
        already_enabled.Insert(filter);
      }
    }

    SetRewriteLevel(RewriteOptions::kAllFilters);
    for (int i = 0; i < RewriteOptions::kEndOfFilters; ++i) {
      RewriteOptions::Filter filter = static_cast<RewriteOptions::Filter>(i);
      if (!already_enabled.IsSet(filter)) {
        DisableFilter(filter);
      }
    }
  }
}

void RewriteOptions::ForceEnableFilter(Filter filter) {
  DCHECK(!frozen_);

  // insert into set of enabled filters.
  modified_ |= enabled_filters_.Insert(filter);

  // remove from set of disabled filters.
  modified_ |= disabled_filters_.Erase(filter);

  // remove from set of forbidden filters.
  modified_ |= forbidden_filters_.Erase(filter);
}

void RewriteOptions::EnableExtendCacheFilters() {
  EnableFilter(kExtendCacheCss);
  EnableFilter(kExtendCacheImages);
  EnableFilter(kExtendCacheScripts);
  // Doesn't enable kExtendCachePdfs.
}

void RewriteOptions::DisableFilter(Filter filter) {
  DCHECK(!frozen_);
  modified_ |= disabled_filters_.Insert(filter);
}

void RewriteOptions::ForbidFilter(Filter filter) {
  DCHECK(!frozen_);
  modified_ |= forbidden_filters_.Insert(filter);
}

void RewriteOptions::EnableFilters(
    const RewriteOptions::FilterSet& filter_set) {
  modified_ |= enabled_filters_.Merge(filter_set);
}

void RewriteOptions::DisableFilters(
    const RewriteOptions::FilterSet& filter_set) {
  modified_ |= disabled_filters_.Merge(filter_set);
}

void RewriteOptions::ForbidFilters(
    const RewriteOptions::FilterSet& filter_set) {
  modified_ |= forbidden_filters_.Merge(filter_set);
}

void RewriteOptions::ClearFilters() {
  DCHECK(!frozen_);
  modified_ = true;
  enabled_filters_.clear();
  disabled_filters_.clear();
  forbidden_filters_.clear();

  // Re-enable HtmlWriterFilter by default.
  EnableFilter(kHtmlWriterFilter);
}

bool RewriteOptions::AddCommaSeparatedListToFilterSetState(
    const StringPiece& filters, FilterSet* set, MessageHandler* handler) {
  DCHECK(!frozen_);
  size_t prev_set_size = set->size();
  bool ret = AddCommaSeparatedListToFilterSet(filters, set, handler);
  modified_ |= (set->size() != prev_set_size);
  return ret;
}

bool RewriteOptions::AddCommaSeparatedListToFilterSet(
    const StringPiece& filters, FilterSet* set, MessageHandler* handler) {
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  for (int i = 0, n = names.size(); i < n; ++i) {
    ret = AddByNameToFilterSet(names[i], set, handler);
  }
  return ret;
}

bool RewriteOptions::AdjustFiltersByCommaSeparatedList(
    const StringPiece& filters, MessageHandler* handler) {
  DCHECK(!frozen_);
  StringPieceVector names;
  SplitStringPieceToVector(filters, ",", &names, true);
  bool ret = true;
  size_t sets_size_sum_before =
      (enabled_filters_.size() + disabled_filters_.size());

  // Default to false unless no filters are specified.
  // "PageSpeedFilters=" -> disable all filters.
  bool non_incremental = names.empty();
  for (int i = 0, n = names.size(); i < n; ++i) {
    StringPiece& option = names[i];
    TrimWhitespace(&option);
    if (!option.empty()) {
      if (option[0] == '-') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], &disabled_filters_, handler);
      } else if (option[0] == '+') {
        option.remove_prefix(1);
        ret = AddByNameToFilterSet(names[i], &enabled_filters_, handler);
      } else {
        // No prefix means: reset to pass-through mode prior to
        // applying any of the filters.  +a,-b,+c" will just add
        // a and c and remove b to current default config, but
        // "+a,-b,+c,d" will just run with filters a, c and d.
        ret = AddByNameToFilterSet(names[i], &enabled_filters_, handler);
        non_incremental = true;
      }
    }
  }

  if (non_incremental) {
    SetRewriteLevel(RewriteOptions::kPassThrough);
    DisableAllFiltersNotExplicitlyEnabled();
    modified_ = true;
  } else {
    // TODO(jmarantz): this modified_ computation for query-params doesn't
    // work as we'd like in RewriteQueryTest.NoChangesShouldNotModify.  See
    // a more detailed TODO there.
    size_t sets_size_sum_after =
        (enabled_filters_.size() + disabled_filters_.size());
    modified_ |= (sets_size_sum_before != sets_size_sum_after);
  }
  return ret;
}

bool RewriteOptions::AddByNameToFilterSet(const StringPiece& option,
                                          FilterSet* set,
                                          MessageHandler* handler) {
  bool ret = true;
  Filter filter = LookupFilter(option);
  if (filter == kEndOfFilters) {
    // Handle a compound filter name.  This is much less common, so we don't
    // have any special infrastructure for it; just code.
    // WARNING: Be careful if you add things here; the filters you add
    // here will be invokable by outside people, so they better not crash
    // if that happens!
    if (option == "rewrite_images") {
      // Every filter here needs to be listed in kCoreFilterSet as well.
      set->Insert(kConvertGifToPng);
      set->Insert(kConvertJpegToProgressive);
      set->Insert(kConvertJpegToWebp);
      set->Insert(kConvertPngToJpeg);
      set->Insert(kConvertToWebpLossless);
      set->Insert(kInlineImages);
      set->Insert(kJpegSubsampling);
      set->Insert(kRecompressJpeg);
      set->Insert(kRecompressPng);
      set->Insert(kRecompressWebp);
      set->Insert(kResizeImages);
      set->Insert(kStripImageColorProfile);
      set->Insert(kStripImageMetaData);
    } else if (option == "recompress_images") {
      // Every filter here needs to be listed under "rewrite_images" as well.
      set->Insert(kConvertGifToPng);
      set->Insert(kConvertJpegToProgressive);
      set->Insert(kConvertJpegToWebp);
      set->Insert(kConvertPngToJpeg);
      set->Insert(kJpegSubsampling);
      set->Insert(kRecompressJpeg);
      set->Insert(kRecompressPng);
      set->Insert(kRecompressWebp);
      set->Insert(kStripImageColorProfile);
      set->Insert(kStripImageMetaData);
    } else if (option == "extend_cache") {
      // Every filter here needs to be listed in kCoreFilterSet as well.
      set->Insert(kExtendCacheCss);
      set->Insert(kExtendCacheImages);
      set->Insert(kExtendCacheScripts);
    } else if (option == "rewrite_javascript") {
      // Every filter here needs to be listed in kCoreFilterSet and
      // kOptimizeForBandwidthFilterSet.  Note that kRewriteJavascriptExternal
      // makes sense in OptimizeForBandwidth because we start rewriting
      // external JS files when we parse them in HTML, so that they are ready
      // in cache for the IPRO request, even though we will not mutate the
      // URLs in HTML.
      set->Insert(kRewriteJavascriptExternal);
      set->Insert(kRewriteJavascriptInline);
    } else if (option == "testing") {
      for (int i = 0, n = arraysize(kTestFilterSet); i < n; ++i) {
        set->Insert(kTestFilterSet[i]);
      }
      for (int i = 0, n = arraysize(kCoreFilterSet); i < n; ++i) {
        set->Insert(kCoreFilterSet[i]);
      }
    } else if (option == "core") {
      for (int i = 0, n = arraysize(kCoreFilterSet); i < n; ++i) {
        set->Insert(kCoreFilterSet[i]);
      }
    } else {
      if (handler != nullptr) {
        handler->Message(kWarning, "Invalid filter name: %s",
                         option.as_string().c_str());
      }
      ret = false;
    }
  } else {
    set->Insert(filter);
    // kResizeMobileImages requires kDelayImages.
    if (filter == kResizeMobileImages) {
      set->Insert(kDelayImages);
    }
  }
  return ret;
}

bool RewriteOptions::AddCommaSeparatedListToOptionSet(
    const StringPiece& options, OptionSet* set, MessageHandler* handler) {
  StringPieceVector option_vector;
  bool ret = true;
  SplitStringPieceToVector(options, ",", &option_vector, true);
  for (int i = 0, n = option_vector.size(); i < n; ++i) {
    StringPieceVector single_option_and_value;
    SplitStringPieceToVector(option_vector[i], "=", &single_option_and_value,
                             true);
    if (single_option_and_value.size() == 2) {
      set->insert(OptionStringPair(single_option_and_value[0].as_string(),
                                   single_option_and_value[1].as_string()));
    } else {
      ret = false;
    }
  }
  return ret;
}

RewriteOptions::Filter RewriteOptions::LookupFilterById(
    const StringPiece& filter_id) {
  GoogleString key(filter_id.data(), filter_id.size());

  FilterEnumToIdAndNameEntry entry;
  entry.filter_enum = kEndOfFilters;
  entry.filter_id = key.c_str();
  entry.filter_name = "";
  const FilterEnumToIdAndNameEntry** it = std::lower_bound(
      filter_id_to_enum_array_,
      filter_id_to_enum_array_ + arraysize(filter_id_to_enum_array_), &entry,
      RewriteOptions::FilterEnumToIdAndNameEntryLessThanById);
  // We use lower_bound because it's O(log n) so relatively efficient. It
  // returns a pointer to the entry whose id is >= filter_id; if filter_id is
  // higher than all ids then 'it' will point past the end, otherwise we have
  // to check that the ids actually match.
  if (it == filter_id_to_enum_array_ + arraysize(filter_id_to_enum_array_) ||
      filter_id != (*it)->filter_id) {
    return kEndOfFilters;
  }
  return (*it)->filter_enum;
}

const RewriteOptions::PropertyBase* RewriteOptions::LookupOptionById(
    StringPiece option_id) {
  const PropertyBase** end =
      option_id_to_property_array_ + all_properties_->size();
  const PropertyBase** it = std::lower_bound(option_id_to_property_array_, end,
                                             option_id, OptionIdCompare());
  // We use lower_bound because it's O(log n) so relatively efficient, but
  // we must double-check its result as it doesn't guarantee an exact match.
  // Note that std::binary_search provides an exact match but only a bool
  // result and not the actual object we were searching for.
  return ((it == end || option_id != (*it)->id()) ? nullptr : *it);
}

const RewriteOptions::PropertyBase* RewriteOptions::LookupOptionByName(
    StringPiece option_name) {
  // There are many options without a name, and it doesn't make sense to
  // find "the one" with an empty name, so short-circuit that early.
  if (option_name.empty()) {
    return nullptr;
  }
  PropertyNameMap::iterator end = option_name_to_property_map_->end(),
                            pos = option_name_to_property_map_->find(
                                GetEffectiveOptionName(option_name));
  return (pos == end ? NULL : pos->second);
}

const StringPiece RewriteOptions::LookupOptionNameById(StringPiece option_id) {
  const PropertyBase* option = LookupOptionById(option_id);
  return (option == nullptr ? StringPiece() : option->option_name());
}

bool RewriteOptions::IsValidOptionName(StringPiece name) {
  return (LookupOptionByName(name) != nullptr);
}

bool RewriteOptions::IsDeprecatedOptionName(StringPiece option_name) {
  // If this ever becomes hot, we should make a proper index, rather than
  // using a Properties object to store these.
  for (int i = 0, n = deprecated_properties_->size(); i < n; ++i) {
    if (StringCaseEqual(option_name,
                        deprecated_properties_->property(i)->option_name())) {
      return true;
    }
  }
  return false;
}

bool RewriteOptions::SetOptionsFromName(const OptionSet& option_set,
                                        MessageHandler* handler) {
  bool ret = true;
  for (RewriteOptions::OptionSet::const_iterator iter = option_set.begin();
       iter != option_set.end(); ++iter) {
    GoogleString msg;
    OptionSettingResult result =
        SetOptionFromName(iter->first, iter->second, &msg);
    if (result != kOptionOk) {
      handler->Message(kWarning, "Failed to set %s to %s (%s)",
                       iter->first.c_str(), iter->second.c_str(), msg.c_str());
      ret = false;
    }
  }
  return ret;
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromName(
    StringPiece name, StringPiece value, GoogleString* msg) {
  GoogleString error_detail;
  OptionSettingResult result = SetOptionFromNameInternal(
      name, value, RewriteOptions::kProcessScopeStrict /* max_scope*/,
      &error_detail);
  return FormatSetOptionMessage(result, name, value, error_detail, msg);
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromName(
    StringPiece name, StringPiece value) {
  GoogleString error_detail;
  return SetOptionFromNameInternal(
      name, value, RewriteOptions::kProcessScopeStrict /* max_scope */,
      &error_detail);
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromQuery(
    StringPiece name, StringPiece value) {
  GoogleString error_detail;
  return SetOptionFromNameInternal(
      name, value, RewriteOptions::kQueryScope /* max_scope */, &error_detail);
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromRemoteConfig(
    StringPiece name, StringPiece value) {
  GoogleString error_detail;
  return SetOptionFromNameInternal(name, value, RewriteOptions::kDirectoryScope,
                                   &error_detail);
}

RewriteOptions::OptionSettingResult RewriteOptions::FormatSetOptionMessage(
    OptionSettingResult result, StringPiece name, StringPiece value,
    StringPiece error_detail, GoogleString* msg) {
  if (!IsValidOptionName(name)) {
    // Not a mapped option.
    absl::StrAppendFormat(msg, "Option %s not mapped.",
                          name.as_string().c_str());
    return kOptionNameUnknown;
  }
  switch (result) {
    case kOptionNameUnknown:
      absl::StrAppendFormat(msg, "Option %s not found.",
                            name.as_string().c_str());
      break;
    case kOptionValueInvalid:
      absl::StrAppendFormat(msg, "Cannot set option %s to %s. %s",
                            name.as_string().c_str(), value.as_string().c_str(),
                            error_detail.as_string().c_str());
      break;
    default:
      break;
  }
  return result;
}

RewriteOptions::OptionSettingResult RewriteOptions::ParseAndSetOptionFromName1(
    StringPiece name, StringPiece arg, GoogleString* msg,
    MessageHandler* handler) {
  // Parse and set with the equvalent of "query = false".
  return ParseAndSetOptionFromNameWithScope(
      name, arg, RewriteOptions::kProcessScopeStrict, msg, handler);
}

RewriteOptions::OptionSettingResult
RewriteOptions::ParseAndSetOptionFromNameWithScope(
    StringPiece name, StringPiece arg, RewriteOptions::OptionScope max_scope,
    GoogleString* msg, MessageHandler* handler) {
  GoogleString error_detail;
  OptionSettingResult result =
      SetOptionFromNameInternal(name, arg, max_scope, &error_detail);
  if (result != RewriteOptions::kOptionNameUnknown) {
    return FormatSetOptionMessage(result, name, arg, error_detail, msg);
  }

  // Assume all goes well; if not, set result accordingly.
  result = RewriteOptions::kOptionOk;

  // Avoid adding more options like this.  All new options should be set via
  // RewriteOptions::AddProperty.  If you have to add one here, also add it to
  // compile_scripts in ngx_rewrite_options.cc.
  // TODO(matterbury): use a hash map for faster lookup/switching.
  if (StringCaseEqual(name, kAllow)) {
    Allow(arg);
  } else if (StringCaseEqual(name, kDisableFilters)) {
    if (!DisableFiltersByCommaSeparatedList(arg, handler)) {
      *msg = "Failed to disable some filters.";
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kDisallow)) {
    Disallow(arg);
  } else if (StringCaseEqual(name, kDomain)) {
    WriteableDomainLawyer()->AddDomain(arg, handler);
  } else if (StringCaseEqual(name, kProxySuffix)) {
    WriteableDomainLawyer()->set_proxy_suffix(arg.as_string());
  } else if (StringCaseEqual(name, kDownstreamCachePurgeLocationPrefix)) {
    GoogleUrl gurl(arg);
    if (gurl.IsWebValid()) {
      // The host:port location where purge requests are to be sent should
      // be made "known" to the DomainLawyer so that when the
      // LoopbackRouteFetcher tries to send the request, it does not consider
      // this an invalid domain.
      WriteableDomainLawyer()->AddKnownDomain(gurl.HostAndPort(), handler);
      set_downstream_cache_purge_location_prefix(arg);
    } else {
      *msg = "Downstream cache purge location prefix is invalid.";
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kEnableFilters)) {
    if (!EnableFiltersByCommaSeparatedList(arg, handler)) {
      *msg = "Failed to enable some filters.";
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kExperimentVariable)) {
    int slot;
    if (!StringToInt(arg, &slot) || slot < 1 || slot > 5) {
      *msg = "must be an integer between 1 and 5";
      result = RewriteOptions::kOptionValueInvalid;
    } else {
      set_experiment_ga_slot(slot);
    }
  } else if (StringCaseEqual(name, kExperimentSpec)) {
    ExperimentSpec* spec = AddExperimentSpec(arg, handler);
    if (spec == nullptr) {
      *msg = "not a valid experiment spec";
      result = RewriteOptions::kOptionValueInvalid;
    } else {
      // To test the validity of options in the experiment spec we have to apply
      // them to a RewriteOptions.  Try to apply them now, so if there are
      // configuration errors we can report them early instead of on each
      // request.
      std::unique_ptr<RewriteOptions> clone(Clone());
      if (!clone->SetOptionsFromName(spec->filter_options(), handler)) {
        *msg = "experiment spec has invalid options= component";
        result = RewriteOptions::kOptionValueInvalid;
      }
    }
  } else if (StringCaseEqual(name, kForbidFilters)) {
    if (!ForbidFiltersByCommaSeparatedList(arg, handler)) {
      *msg = "Failed to forbid some filters.";
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kRetainComment)) {
    RetainComment(arg);
  } else if (StringCaseEqual(name, kBlockingRewriteRefererUrls)) {
    EnableBlockingRewriteForRefererUrlPattern(arg);
  } else if (StringCaseEqual(name, kPermitIdsForCssCombining)) {
    AddCssCombiningWildcard(arg);
  } else {
    result = RewriteOptions::kOptionNameUnknown;
  }
  return result;
}

RewriteOptions::OptionSettingResult RewriteOptions::ParseAndSetOptionFromName2(
    StringPiece name, StringPiece arg1, StringPiece arg2, GoogleString* msg,
    MessageHandler* handler) {
  // Assume all goes well; if not, set result accordingly.
  OptionSettingResult result = RewriteOptions::kOptionOk;

  // TODO(matterbury): use a hash map for faster lookup/switching.
  if (StringCaseEqual(name, kAddResourceHeader)) {
    if (!ValidateAndAddResourceHeader(arg1, arg2, msg)) {
      return RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kCustomFetchHeader)) {
    AddCustomFetchHeader(arg1, arg2);
  } else if (StringCaseEqual(name, kLoadFromFile)) {
    file_load_policy()->Associate(arg1, arg2);
  } else if (StringCaseEqual(name, kLoadFromFileMatch)) {
    if (!file_load_policy()->AssociateRegexp(arg1, arg2, msg)) {
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kLoadFromFileRule) ||
             StringCaseEqual(name, kLoadFromFileRuleMatch)) {
    bool is_regexp = (name == kLoadFromFileRuleMatch);
    bool allow;
    if (StringCaseEqual(arg1, "Allow")) {
      allow = true;
    } else if (StringCaseEqual(arg1, "Disallow")) {
      allow = false;
    } else {
      *msg = "Argument 1 must be either 'Allow' or 'Disallow'";
      return RewriteOptions::kOptionValueInvalid;
    }
    if (!file_load_policy()->AddRule(arg2.as_string(), is_regexp, allow, msg)) {
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kMapOriginDomain)) {
    WriteableDomainLawyer()->AddOriginDomainMapping(arg1, arg2, "", handler);
  } else if (StringCaseEqual(name, kMapProxyDomain)) {
    WriteableDomainLawyer()->AddProxyDomainMapping(arg1, arg2, "", handler);
  } else if (StringCaseEqual(name, kMapRewriteDomain)) {
    WriteableDomainLawyer()->AddRewriteDomainMapping(arg1, arg2, handler);
  } else if (StringCaseEqual(name, kShardDomain)) {
    if (!arg2.empty()) {
      // We allow people to put:
      //   pagespeed ShardDomain domain_to_shard "";
      // because we want people to be able to use script variables in nginx to
      // disable domain sharding with spdy/http2.
      // See pagespeed/module/https_support#h2_configuration_nginx
      WriteableDomainLawyer()->AddShard(arg1, arg2, handler);
    }
  } else {
    result = RewriteOptions::kOptionNameUnknown;
  }
  return result;
}

RewriteOptions::OptionSettingResult RewriteOptions::ParseAndSetOptionFromName3(
    StringPiece name, StringPiece arg1, StringPiece arg2, StringPiece arg3,
    GoogleString* msg, MessageHandler* handler) {
  // Assume all goes well; if not, set result accordingly.
  OptionSettingResult result = RewriteOptions::kOptionOk;
  if (StringCaseEqual(name, kUrlValuedAttribute)) {
    // Examples:
    //   UrlValuedAttribute span src Hyperlink
    //     - <span src=...> indicates a hyperlink
    //   UrlValuedAttribute hr imgsrc Image
    //     - <hr image=...> indicates an image resource
    semantic_type::Category category;
    if (!semantic_type::ParseCategory(arg3, &category)) {
      *msg = StrCat("Invalid resource category: ", arg3);
      result = RewriteOptions::kOptionValueInvalid;
    } else {
      AddUrlValuedAttribute(arg1, arg2, category);
    }
  } else if (StringCaseEqual(name, kLibrary)) {
    // Library bytes md5 canonical_url
    // Examples:
    //   Library 43567 5giEj_jl-Ag5G8 http://www.example.com/url.js
    int64 bytes;
    if (!StringToInt64(arg1, &bytes) || bytes < 0) {
      *msg = "Library size must be a positive 64-bit integer";
      result = RewriteOptions::kOptionValueInvalid;
    } else if (!RegisterLibrary(bytes, arg2, arg3)) {
      *msg = StrCat("Format is size md5 url; bad md5 ", arg2, " or URL ", arg3);
      result = RewriteOptions::kOptionValueInvalid;
    }
  } else if (StringCaseEqual(name, kMapOriginDomain)) {
    WriteableDomainLawyer()->AddOriginDomainMapping(arg1, arg2, arg3, handler);
  } else if (StringCaseEqual(name, kMapProxyDomain)) {
    WriteableDomainLawyer()->AddProxyDomainMapping(arg1, arg2, arg3, handler);
  } else {
    result = RewriteOptions::kOptionNameUnknown;
  }
  return result;
}

StringPiece RewriteOptions::GetEffectiveOptionName(StringPiece name) {
  StringPiece effective_name = name;
  const RenamedOptionMap* end =
      kRenamedOptionNameData + arraysize(kRenamedOptionNameData);
  const RenamedOptionMap* entry = std::lower_bound(
      kRenamedOptionNameData, end, name, &RenamedOptionMap::LessThan);
  if ((entry != end) && StringCaseEqual(name, entry->deprecated_option_name)) {
    effective_name = entry->new_option_name;
  }
  return effective_name;
}

RewriteOptions::OptionSettingResult RewriteOptions::SetOptionFromNameInternal(
    StringPiece name, StringPiece value, RewriteOptions::OptionScope max_scope,
    GoogleString* error_detail) {
  if (!IsValidOptionName(name)) {
    return kOptionNameUnknown;
  }
  StringPiece effective_name = GetEffectiveOptionName(name);
  OptionBaseVector::iterator it =
      std::lower_bound(all_options_.begin(), all_options_.end(), effective_name,
                       RewriteOptions::OptionNameLessThanArg);
  if (it != all_options_.end()) {
    OptionBase* option = *it;
    if (StringCaseEqual(effective_name, option->option_name())) {
      if (option->scope() > max_scope) {
        StrAppend(error_detail, "Option ", name,
                  " cannot be set. Maximum allowed scope is ",
                  ScopeEnumToString(max_scope));
        return kOptionNameUnknown;
      } else if (!option->SetFromString(value, error_detail)) {
        return kOptionValueInvalid;
      } else {
        return kOptionOk;
      }
    }
  }
  return kOptionNameUnknown;
}

bool RewriteOptions::OptionValue(StringPiece name, const char** id,
                                 bool* was_set, GoogleString* value) const {
  OptionBaseVector::const_iterator it =
      std::lower_bound(all_options_.begin(), all_options_.end(), name,
                       RewriteOptions::OptionNameLessThanArg);
  if (it != all_options_.end()) {
    OptionBase* option = *it;
    if (StringCaseEqual(name, option->option_name())) {
      *value = option->ToString();
      *id = option->id();
      *was_set = option->was_set();
      return true;
    }
  }
  return false;
}

bool RewriteOptions::SetOptionFromNameAndLog(StringPiece name,
                                             StringPiece value,
                                             MessageHandler* handler) {
  GoogleString msg;
  OptionSettingResult result = SetOptionFromName(name, value, &msg);
  if (result == kOptionOk) {
    return true;
  } else {
    handler->MessageS(kWarning, msg);
    return false;
  }
}

bool RewriteOptions::ParseFromString(StringPiece value_string, bool* value) {
  // How are bools passed in the string?  I am assuming "true"/"false" or
  // "on"/"off".
  if (StringCaseEqual(value_string, "true") ||
      StringCaseEqual(value_string, "on")) {
    *value = true;
  } else if (StringCaseEqual(value_string, "false") ||
             StringCaseEqual(value_string, "off")) {
    *value = false;
  } else {
    // value_string is not "true"/"false" or "on"/"off".  Return a parse
    // error.
    return false;
  }
  return true;
}

bool RewriteOptions::ParseFromString(StringPiece value_string,
                                     EnabledEnum* value) {
  bool bool_value;
  if (ParseFromString(value_string, &bool_value)) {
    *value = bool_value ? kEnabledOn : kEnabledOff;
  } else if (StringCaseEqual(value_string, "unplugged")) {
    *value = kEnabledUnplugged;
  } else if (StringCaseEqual(value_string, "standby")) {
    *value = kEnabledStandby;
  } else {
    // value_string is not "true"/"false" or "on"/"off"/"unplugged".
    // Return a parse error.
    return false;
  }
  return true;
}

bool RewriteOptions::ParseFromString(StringPiece value_string,
                                     ResponsiveDensities* value) {
  // Temp vector so that we don't return any densities if there's an error.
  std::vector<double> ret;
  StringPieceVector density_strs;
  // Ignores empty strings.
  SplitStringUsingSubstr(value_string, ",", &density_strs);
  if (density_strs.size() == 0) {
    // TODO(sligocki): Return error_message instead of directly logging message.
    LOG(ERROR) << "ResponsiveImageDensities: Must not be empty list.";
    return false;
  }
  for (size_t i = 0, n = density_strs.size(); i < n; ++i) {
    double density;
    if (!ParseFromString(density_strs[i], &density)) {
      LOG(ERROR) << "ResponsiveImageDensities: Cannot parse number: "
                 << density_strs[i];
      return false;
    } else if (density <= 0) {
      LOG(ERROR) << "ResponsiveImageDensities: Must be > 0. Invalid number: "
                 << density_strs[i];
      return false;
    } else {  // Valid
      ret.push_back(density);
    }
  }
  value->swap(ret);
  std::sort(value->begin(), value->end());
  return true;
}

bool RewriteOptions::ParseFromString(StringPiece value_string,
                                     protobuf::MessageLite* proto) {
  return ParseProtoFromStringPiece(value_string, proto);
}

bool RewriteOptions::ParseFromString(StringPiece value_string,
                                     AllowVaryOn* allow_vary_on) {
  AllowVaryOn allow;
  TrimWhitespace(&value_string);
  if (StringCaseEqual(value_string, AllowVaryOn::kNoneString)) {
    // "allow" has already been initialized to all false; nothing to do.
  } else if (StringCaseEqual(value_string, AllowVaryOn::kAutoString)) {
    allow.set_allow_auto(true);
  } else {
    StringPieceVector value_vector;
    SplitStringPieceToVector(value_string, ",", &value_vector,
                             false /* omit_empty_strings */);
    // When "value_string" is empty, "value_vector" has only one element
    // which is an empty string.
    for (size_t i = 0, n = value_vector.size(); i < n; ++i) {
      StringPiece value = value_vector[i];
      TrimWhitespace(&value);
      if (StringCaseEqual(value, HttpAttributes::kAccept)) {
        allow.set_allow_accept(true);
      } else if (StringCaseEqual(value, HttpAttributes::kSaveData)) {
        allow.set_allow_save_data(true);
      } else if (StringCaseEqual(value, HttpAttributes::kUserAgent)) {
        allow.set_allow_user_agent(true);
      } else {
        return false;
      }
    }
  }
  *allow_vary_on = allow;
  return true;
}

bool RewriteOptions::Enabled(Filter filter) const {
  // Enforce a hierarchy of configuration precedence:
  // a. Explicit forbid is permanent all the way down the hierarchy and
  //    cannot be overridden
  // b. "lower level" configs (vhost, query-params, subdirectories) override
  //    higher level -- this takes place in Merge.
  // c. explicit filter setting overrides preserve
  // d. preserve overrides rewrite-level
  //
  // TODO(jmarantz): add doc explaining this.

  // Explicitly disabled filters always lose, independent of level & preserve.
  if (disabled_filters_.IsSet(filter) || forbidden_filters_.IsSet(filter)) {
    return false;
  }

  // Explicitly enabled filters always win, independent of preserve.
  if (enabled_filters_.IsSet(filter)) {
    return true;
  }

  FilterProperties properties = filter_properties[filter];
  if (css_preserve_urls() && properties.preserve_css_urls) {
    return false;
  }
  if (js_preserve_urls() && properties.preserve_js_urls) {
    return false;
  }
  if (image_preserve_urls() && properties.preserve_image_urls) {
    return false;
  }

  switch (level_.value()) {
    case kTestingCoreFilters:
      if (properties.level_test) {
        return true;
      }
      FALLTHROUGH_INTENDED;
    case kCoreFilters:
      if (properties.level_core) {
        return true;
      }
      break;
    case kOptimizeForBandwidth:
      if (properties.level_optimize_for_bandwidth) {
        return true;
      }
      break;
    case kMobilizeFilters:
      if (properties.level_mobilize) {
        return true;
      }
      break;
    case kAllFilters:
      if (!properties.level_dangerous) {
        return true;
      }
      break;
    case kPassThrough:
      break;
  }
  return false;
}

bool RewriteOptions::Forbidden(Filter filter) const {
  return (forbidden_filters_.IsSet(filter) ||
          (forbid_all_disabled_filters() && disabled_filters_.IsSet(filter)));
}

bool RewriteOptions::Forbidden(StringPiece filter_id) const {
  // It's forbidden if it's expressly forbidden or if it's disabled and all
  //  disabled filters are forbidden.
  RewriteOptions::Filter filter = RewriteOptions::LookupFilterById(filter_id);
  // TODO(jmarantz): handle "ce" which is not indexed as a single filter.
  return ((filter != kEndOfFilters) && Forbidden(filter));
}

bool RewriteOptions::HasRejectedHeader(
    const StringPiece& header_name,
    const RequestHeaders* request_headers) const {
  ConstStringStarVector header_values;
  if (request_headers->Lookup(header_name, &header_values)) {
    for (int i = 0, n = header_values.size(); i < n; ++i) {
      if (IsRejectedRequest(header_name, *header_values[i])) {
        return true;
      }
    }
  }
  return false;
}

bool RewriteOptions::IsRequestDeclined(
    const GoogleString& url, const RequestHeaders* request_headers) const {
  if (IsRejectedUrl(url) ||
      HasRejectedHeader(HttpAttributes::kUserAgent, request_headers) ||
      HasRejectedHeader(HttpAttributes::kXForwardedFor, request_headers)) {
    return true;
  }

  return false;
}

int64 RewriteOptions::ImageInlineMaxBytes() const {
  if (Enabled(kInlineImages)) {
    return image_inline_max_bytes_.value();
  } else {
    return 0;
  }
}

void RewriteOptions::set_image_inline_max_bytes(int64 x) {
  set_option(x, &image_inline_max_bytes_);
  if (!css_image_inline_max_bytes_.was_set() &&
      x > css_image_inline_max_bytes_.value()) {
    // Make sure css_image_inline_max_bytes is at least image_inline_max_bytes
    // if it has not been explicitly configured.
    css_image_inline_max_bytes_.set(x);
  }
}

int64 RewriteOptions::CssImageInlineMaxBytes() const {
  if (Enabled(kInlineImages)) {
    return css_image_inline_max_bytes_.value();
  } else {
    return 0;
  }
}

int64 RewriteOptions::MaxImageInlineMaxBytes() const {
  return std::max(ImageInlineMaxBytes(), CssImageInlineMaxBytes());
}

void RewriteOptions::GetEnabledFiltersRequiringScriptExecution(
    RewriteOptions::FilterVector* filters) const {
  for (RewriteOptions::Filter filter : kRequiresScriptExecutionFilterSet) {
    if (Enabled(filter)) {
      filters->push_back(filter);
    }
  }
}

void RewriteOptions::DisableFiltersRequiringScriptExecution() {
  for (RewriteOptions::Filter filter : kRequiresScriptExecutionFilterSet) {
    DisableFilter(filter);
  }
}

void RewriteOptions::DisableFiltersThatCantRunInAjax() {
  DisableFiltersRequiringScriptExecution();
  for (RewriteOptions::Filter filter : kAddHeadFilters) {
    DisableFilter(filter);
  }

  // Note that kPrioritizeCriticalCss does not require script execution for
  // correct behavior because it adds its own <noscript> block.  This is better
  // than a noscript-redirect, which is what we need to do for filters that
  // require JS to run for the page to render correctly.  But we still need
  // to disable it for ajax requests because it's fundamentally a whole-page
  // optimization.
  DisableFilter(RewriteOptions::kPrioritizeCriticalCss);

  // Don't modify URLs for ajax requests, in case they are relative.
  //
  // TODO(jmarantz): would be better to enable rewriting of absolute URLs but
  // disable rewriting of relative URLs.
  set_css_preserve_urls(true);
  set_image_preserve_urls(true);
  set_js_preserve_urls(true);
}

bool RewriteOptions::RequiresAddHead() const {
  for (RewriteOptions::Filter filter : kAddHeadFilters) {
    if (Enabled(filter)) {
      return true;
    }
  }
  return false;
}

bool RewriteOptions::UsePerOriginPropertyCachePage() const {
  return Enabled(kMobilize);
}

DomainLawyer* RewriteOptions::WriteableDomainLawyer() {
  Modify();
  return domain_lawyer_.MakeWriteable();
}

JavascriptLibraryIdentification*
RewriteOptions::WriteableJavascriptLibraryIdentification() {
  Modify();
  return javascript_library_identification_.MakeWriteable();
}

void RewriteOptions::Merge(const RewriteOptions& src) {
  DCHECK(!frozen_);
#ifndef NDEBUG
  CHECK(src.MergeOK());  // DCHECK outside of the #ifndef does not link.
#endif

  bool modify = src.modified_;

  DCHECK_EQ(all_options_.size(), src.all_options_.size());
  DCHECK_EQ(initialized_options_, src.initialized_options_);
  DCHECK_EQ(initialized_options_, all_options_.size());

  // In the case of conflicts between extend_cache and preserve, remember
  // which one should win before we merge the individual options and filters.
  MergeOverride override_css = ComputeMergeOverride(
      kExtendCacheCss, src.css_preserve_urls_, css_preserve_urls_, src);
  MergeOverride override_images = ComputeMergeOverride(
      kExtendCacheImages, src.image_preserve_urls_, image_preserve_urls_, src);
  MergeOverride override_scripts = ComputeMergeOverride(
      kExtendCacheScripts, src.js_preserve_urls_, js_preserve_urls_, src);

  // If this.forbid_all_disabled_filters() is true
  // but src.forbid_all_disabled_filters() is false,
  // the default merging logic will set it false in the result, but we need
  // to toggle the value: once it's set it has to stay set.
  bool new_forbid_all_disabled =
      (forbid_all_disabled_filters() || src.forbid_all_disabled_filters());

  // If ForbidAllDisabledFilters is turned on, it means no-one can enable a
  // filter that isn't already enabled, meaning the filters enabled in 'src'
  // cannot be enabled in 'this'.
  if (!forbid_all_disabled_filters()) {
    // Enabled filters in src override disabled filters in this.
    disabled_filters_.EraseSet(src.enabled_filters_);
  }

  modify |= enabled_filters_.Merge(src.enabled_filters_);
  modify |= disabled_filters_.Merge(src.disabled_filters_);

  // Clean up enabled filters list to make debugging easier.
  enabled_filters_.EraseSet(disabled_filters_);

  // Forbidden filters strictly merge, with no exclusions.  E.g. You can never
  // enable a filter in an .htaccess file that was forbidden above.
  modify |= forbidden_filters_.Merge(src.forbidden_filters_);

  enabled_filters_.EraseSet(forbidden_filters_);

  experiment_id_ = src.experiment_id_;
  for (int i = 0, n = src.experiment_specs_.size(); i < n; ++i) {
    ExperimentSpec* spec = src.experiment_specs_[i]->Clone();
    InsertExperimentSpecInVector(spec);
  }

  if (src.downstream_cache_purge_location_prefix_.was_set()) {
    set_downstream_cache_purge_location_prefix(
        src.downstream_cache_purge_location_prefix());
  }
  for (int i = 0, n = src.resource_headers_.size(); i < n; ++i) {
    NameValue* nv = src.resource_headers_[i];
    AddResourceHeader(nv->name, nv->value);
  }
  for (int i = 0, n = src.custom_fetch_headers_.size(); i < n; ++i) {
    NameValue* nv = src.custom_fetch_headers_[i];
    AddCustomFetchHeader(nv->name, nv->value);
  }

  for (int i = 0, n = src.num_url_valued_attributes(); i < n; ++i) {
    StringPiece element;
    StringPiece attribute;
    semantic_type::Category category;
    src.UrlValuedAttribute(i, &element, &attribute, &category);
    AddUrlValuedAttribute(element, attribute, category);
  }

  // Note that from the perspective of this class, we can be merging
  // RewriteOptions subclasses & superclasses, so don't read anything
  // that doesn't exist.  However this is almost certainly the wrong
  // thing to do -- we should ensure that within a system all the
  // RewriteOptions that are instantiated are the same sublcass, so
  // DCHECK that they have the same number of options.
  DCHECK_EQ(all_options_.size(), src.all_options_.size());
  size_t options_to_merge =
      std::min(all_options_.size(), src.all_options_.size());
  for (size_t i = 0; i < options_to_merge; ++i) {
    all_options_[i]->Merge(src.all_options_[i]);
  }

  FastWildcardGroupMap::const_iterator it = src.rejected_request_map_.begin();
  for (; it != src.rejected_request_map_.end(); ++it) {
    std::pair<FastWildcardGroupMap::iterator, bool> insert_result =
        rejected_request_map_.insert(std::make_pair(
            it->first, static_cast<FastWildcardGroup*>(nullptr)));
    if (insert_result.second) {
      insert_result.first->second = new FastWildcardGroup;
    }
    insert_result.first->second->AppendFrom(*it->second);
  }

  domain_lawyer_.MergeOrShare(src.domain_lawyer_);
  javascript_library_identification_.MergeOrShare(
      src.javascript_library_identification_);
  {
    ScopedMutex this_lock(cache_purge_mutex_.get());
    ScopedMutex src_lock(src.cache_purge_mutex_.get());
    purge_set_.MergeOrShare(src.purge_set_);
  }

  file_load_policy_.Merge(src.file_load_policy_);
  allow_resources_.MergeOrShare(src.allow_resources_);
  allow_when_inlining_resources_.MergeOrShare(
      src.allow_when_inlining_resources_);
  retain_comments_.MergeOrShare(src.retain_comments_);
  lazyload_enabled_classes_.MergeOrShare(src.lazyload_enabled_classes_);
  css_combining_permitted_ids_.MergeOrShare(src.css_combining_permitted_ids_);
  blocking_rewrite_referer_urls_.MergeOrShare(
      src.blocking_rewrite_referer_urls_);
  override_caching_wildcard_.MergeOrShare(src.override_caching_wildcard_);

  // Merge url_cache_invalidation_entries_ so that increasing order of timestamp
  // is preserved (assuming this.url_cache_invalidation_entries_ and
  // src.url_cache_invalidation_entries_ are both ordered).
  int original_size = url_cache_invalidation_entries_.size();
  // Append copies of src's url cache invalidation entries to this.
  for (int i = 0, n = src.url_cache_invalidation_entries_.size(); i < n; ++i) {
    url_cache_invalidation_entries_.push_back(
        src.url_cache_invalidation_entries_[i]->Clone());
  }
  // Now url_cache_invalidation_entries_ consists of two ordered ranges: [begin,
  // begin+original_size) and [begin+original_size, end).  Hence we can use
  // inplace_merge.
  std::inplace_merge(url_cache_invalidation_entries_.begin(),
                     url_cache_invalidation_entries_.begin() + original_size,
                     url_cache_invalidation_entries_.end(),
                     RewriteOptions::CompareUrlCacheInvalidationEntry);

  // If either side has forbidden all disabled filters then the result must
  // too. This is required to prevent subdirectories from turning it off when
  // a parent directory has turned it on (by mod_instaweb.cc/merge_dir_config).
  if (forbid_all_disabled_filters_.was_set() ||
      src.forbid_all_disabled_filters_.was_set()) {
    set_forbid_all_disabled_filters(new_forbid_all_disabled);
  }

  ApplyMergeOverride(override_css, kExtendCacheCss, &css_preserve_urls_);
  ApplyMergeOverride(override_images, kExtendCacheImages,
                     &image_preserve_urls_);
  ApplyMergeOverride(override_scripts, kExtendCacheScripts, &js_preserve_urls_);

  if (modify) {
    Modify();
  }
}

void RewriteOptions::MergeOnlyProcessScopeOptions(const RewriteOptions& src) {
  DCHECK(!frozen_);
#ifndef NDEBUG  // MergeOK is only around in CHECK-enabled builds.
  CHECK(src.MergeOK());
#endif

  DCHECK_EQ(all_options_.size(), src.all_options_.size());
  DCHECK_EQ(initialized_options_, src.initialized_options_);
  DCHECK_EQ(initialized_options_, all_options_.size());

  size_t options_to_merge =
      std::min(all_options_.size(), src.all_options_.size());
  for (size_t i = 0; i < options_to_merge; ++i) {
    OptionScope scope = all_options_[i]->scope();
    if (scope == kLegacyProcessScope || scope == kProcessScopeStrict) {
      all_options_[i]->Merge(src.all_options_[i]);
    }
  }

  Modify();
}

RewriteOptions* RewriteOptions::Clone() const {
  RewriteOptions* options = NewOptions();
  options->Merge(*this);
  options->frozen_ = false;
  options->modified_ = false;
  return options;
}

RewriteOptions* RewriteOptions::NewOptions() const {
  return new RewriteOptions(thread_system_);
}

GoogleString RewriteOptions::OptionSignature(const GoogleString& x,
                                             const Hasher* hasher) {
  return hasher->Hash(x);
}

GoogleString RewriteOptions::OptionSignature(ResourceCategorySet x,
                                             const Hasher* hasher) {
  return hasher->Hash(ToString(x));
}

GoogleString RewriteOptions::OptionSignature(RewriteLevel level,
                                             const Hasher* hasher) {
  switch (level) {
    case kPassThrough:
      return "p";
    case kCoreFilters:
      return "c";
    case kOptimizeForBandwidth:
      return "b";
    case kMobilizeFilters:
      return "m";
    case kTestingCoreFilters:
      return "t";
    case kAllFilters:
      return "a";
  }
  return "?";
}

GoogleString RewriteOptions::OptionSignature(const BeaconUrl& beacon_url,
                                             const Hasher* hasher) {
  return hasher->Hash(ToString(beacon_url));
}

GoogleString RewriteOptions::OptionSignature(const MobTheme& theme,
                                             const Hasher* hasher) {
  GoogleString to_hash;
  to_hash.push_back(theme.background_color.r);
  to_hash.push_back(theme.background_color.g);
  to_hash.push_back(theme.background_color.b);
  to_hash.push_back(theme.foreground_color.r);
  to_hash.push_back(theme.foreground_color.g);
  to_hash.push_back(theme.foreground_color.b);
  to_hash.append(theme.logo_url);
  return hasher->Hash(to_hash);
}

GoogleString RewriteOptions::OptionSignature(
    const ResponsiveDensities& densities, const Hasher* hasher) {
  return hasher->Hash(ToString(densities));
}

GoogleString RewriteOptions::OptionSignature(const protobuf::MessageLite& proto,
                                             const Hasher* hasher) {
  return hasher->Hash(ToString(proto));
}

GoogleString RewriteOptions::OptionSignature(const AllowVaryOn& allow_vary_on,
                                             const Hasher* hasher) {
  GoogleString out;
  char mask =
      (allow_vary_on.allow_auto() | (allow_vary_on.allow_accept() << 1) |
       (allow_vary_on.allow_save_data() << 2) |
       (allow_vary_on.allow_user_agent() << 3));
  Web64Encode(GoogleString(&mask, 1), &out);
  return out;
}

void RewriteOptions::DisableIfNotExplictlyEnabled(Filter filter) {
  if (!enabled_filters_.IsSet(filter)) {
    disabled_filters_.Insert(filter);
  }
}

RewriteOptions::MergeOverride RewriteOptions::ComputeMergeOverride(
    Filter filter, const Option<bool>& src_preserve_option,
    const Option<bool>& preserve_option, const RewriteOptions& src) {
  // Note: the order of the if and else-if matter. if both this and
  // src have filter enabled and preserve_options set, then the filter
  // would actually be disabled.
  if (src.Enabled(filter) && preserve_option.value()) {
    return kDisablePreserve;
  } else if (Enabled(filter) && src_preserve_option.value()) {
    return kDisableFilter;
  }
  return kNoAction;
}

void RewriteOptions::ApplyMergeOverride(MergeOverride merge_override,
                                        Filter filter,
                                        Option<bool>* preserve_option) {
  switch (merge_override) {
    case kNoAction:
      break;
    case kDisablePreserve:
      if (preserve_option->was_set()) {
        preserve_option->set(false);
      }
      break;
    case kDisableFilter:
      enabled_filters_.Erase(filter);
      disabled_filters_.Insert(filter);
      break;
  }
}

void RewriteOptions::Freeze() {
  if (!frozen_) {
    frozen_ = true;
    signature_.clear();
  }
}

void RewriteOptions::ComputeSignature() {
  ThreadSystem::ScopedReader read_lock(cache_purge_mutex_.get());
  ComputeSignatureLockHeld();
}

void RewriteOptions::ComputeSignatureLockHeld() {
  if (frozen_) {
    return;
  }
#ifndef NDEBUG
  if (!options_uniqueness_checked_) {
    options_uniqueness_checked_ = true;
    StringSet id_set;
    for (int i = 0, n = all_options_.size(); i < n; ++i) {
      const char* id = all_options_[i]->id();
      std::pair<StringSet::iterator, bool> insertion = id_set.insert(id);
      DCHECK(insertion.second) << "Duplicate RewriteOption option id: " << id;
    }
  }
#endif

  signature_ = IntegerToString(kOptionsVersion);
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    // Ignore the debug filter when computing signatures.  Note that we still
    // must have kDebug be considered in IsEqual though.
    if ((filter != kDebug) && Enabled(filter)) {
      StrAppend(&signature_, "_", FilterId(filter));
    }
  }
  signature_ += "O";
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Keep the signature relatively short by only including options
    // with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->is_used_for_signature_computation() && option->was_set()) {
      StrAppend(&signature_, option->id(), ":", option->Signature(hasher()),
                "_");
    }
  }
  if (javascript_library_identification() != nullptr) {
    StrAppend(&signature_, "LI:");
    javascript_library_identification()->AppendSignature(&signature_);
    StrAppend(&signature_, "_");
  }
  StrAppend(&signature_, domain_lawyer_->Signature(), "_");
  StrAppend(&signature_, "AR:", allow_resources_->Signature(), "_");
  StrAppend(&signature_, "AWIR:", allow_when_inlining_resources_->Signature(),
            "_");
  StrAppend(&signature_, "RC:", retain_comments_->Signature(), "_");
  StrAppend(&signature_, "LDC:", lazyload_enabled_classes_->Signature(), "_");
  StrAppend(&signature_, "CCPI:", css_combining_permitted_ids_->Signature(),
            "_");
  StrAppend(&signature_, "BRRU:", blocking_rewrite_referer_urls_->Signature(),
            "_");
  StrAppend(&signature_, "UCI:");
  for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n; ++i) {
    const UrlCacheInvalidationEntry& entry =
        *url_cache_invalidation_entries_[i];
    if (!entry.ignores_metadata_and_pcache) {
      StrAppend(&signature_, entry.ComputeSignature(), "|");
    }
  }

  // We do not include the PurgeSet signature, but that is included in
  // RewriteOptions::IsEqual.
  //
  // TODO(jmarantz): Remove the global invalidation timestamp from the
  // signature and add explicit timestamp checking where needed, such
  // as pcache lookups.  Note that it is already included in HTTPCache
  // lookups.
  StrAppend(&signature_, "GTS:",
            Integer64ToString(purge_set_->global_invalidation_timestamp_ms()),
            "_");

  // rejected_request_map_ is not added to rewrite options signature as this
  // should not affect rewriting and metadata or property cache lookups.
  StrAppend(&signature_, "OC:", override_caching_wildcard_->Signature(), "_");

  StrAppend(&signature_, SubclassSignatureLockHeld());

  frozen_ = true;

  // TODO(jmarantz): Incorporate signature from file_load_policy.  However, the
  // changes made here make our system strictly more correct than it was before,
  // using an ad-hoc signature in css_filter.cc.
}

bool RewriteOptions::ClearSignatureWithCaution() {
  bool recompute_signature = frozen_;
  frozen_ = false;
#ifndef NDEBUG
  last_thread_id_.reset();
#endif
  signature_.clear();
  return recompute_signature;
}

bool RewriteOptions::IsEqual(const RewriteOptions& that) const {
  DCHECK(frozen_);
  DCHECK(that.frozen_);
  if (signature() != that.signature()) {
    return false;
  }

  // kDebug is excluded from the signature but we better not exclude it
  // from IsEqual.
  if (Enabled(kDebug) != that.Enabled(kDebug)) {
    return false;
  }

  // TODO(jmarantz): move more stuff out of the signature() and into the
  // IsEqual function.  We might also want to make a second signature so
  // that IsEqual is not too slow.
  //
  // TODO(jmarantz): consider making a second signature for the
  // PurgeSet and other stuff that we exclude for
  // the RewriteOptions::signature.
  {
    ThreadSystem::ScopedReader read_lock(cache_purge_mutex_.get());
    ThreadSystem::ScopedReader read_lock2(that.cache_purge_mutex_.get());
    return purge_set_->Equals(*that.purge_set_);
  }
}

GoogleString RewriteOptions::ToString(const ResourceCategorySet& x) {
  GoogleString result = "";
  const char* delim = "";
  for (ResourceCategorySet::const_iterator entry = x.begin(); entry != x.end();
       ++entry) {
    StrAppend(&result, delim, semantic_type::GetCategoryString(*entry));
    delim = ",";
  }
  return result;
}

GoogleString RewriteOptions::ToString(RewriteLevel level) {
  switch (level) {
    case kPassThrough:
      return "Pass Through";
    case kOptimizeForBandwidth:
      return "Optimize For Bandwidth";
    case kCoreFilters:
      return "Core Filters";
    case kMobilizeFilters:
      return "Mobilize Filters";
    case kTestingCoreFilters:
      return "Testing Core Filters";
    case kAllFilters:
      return "All Filters";
  }
  return "?";
}

GoogleString RewriteOptions::ToString(const BeaconUrl& beacon_url) {
  GoogleString result = beacon_url.http;
  if (beacon_url.http != beacon_url.https) {
    StrAppend(&result, " ", beacon_url.https);
  }
  return result;
}

GoogleString RewriteOptions::ToString(const MobTheme& theme) {
  return StrCat(ToString(theme.background_color), " ",
                ToString(theme.foreground_color), " ", theme.logo_url);
}

GoogleString RewriteOptions::ToString(const Color& color) {
  return absl::StrFormat("#%02x%02x%02x", static_cast<int>(color.r),
                         static_cast<int>(color.g), static_cast<int>(color.b));
}

GoogleString RewriteOptions::ToString(const ResponsiveDensities& densities) {
  GoogleString result = "";
  const char* delim = "";
  for (ResponsiveDensities::const_iterator iter = densities.begin();
       iter != densities.end(); ++iter) {
    // 4 digits of precision seems like plenty. Note: hashing doubles is
    // generally not a good idea. But in this case we are only parsing the
    // doubles from config and never performing any arithmetic, so hashing
    // should be alright.
    StrAppend(&result, delim, absl::StrFormat("%.4g", *iter));
    delim = ",";
  }
  return result;
}

GoogleString RewriteOptions::ToString(const protobuf::MessageLite& proto) {
  return proto.SerializeAsString();
}

GoogleString RewriteOptions::FilterSetToString(
    const FilterSet& filter_set) const {
  GoogleString output;
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (filter_set.IsSet(filter)) {
      StrAppend(&output, FilterId(filter), "\t", FilterName(filter), "\n");
    }
  }
  return output;
}

GoogleString RewriteOptions::AllowVaryOn::ToString() const {
  GoogleString result = "";
  const char* delim = "";
  if (allow_auto()) {
    result = kAutoString;
    // Make sure all other options have been set correctly.
    DCHECK(!allow_accept());
    DCHECK(!allow_user_agent());
    DCHECK(allow_save_data());
  } else {
    if (allow_accept()) {
      StrAppend(&result, delim, HttpAttributes::kAccept);
      delim = ",";
    }
    if (allow_save_data()) {
      StrAppend(&result, delim, HttpAttributes::kSaveData);
      delim = ",";
    }
    if (allow_user_agent()) {
      StrAppend(&result, delim, HttpAttributes::kUserAgent);
    }
    if (result.empty()) {
      result = kNoneString;
    }
  }
  return result;
}

GoogleString RewriteOptions::ToString(const AllowVaryOn& allow_vary_on) {
  return allow_vary_on.ToString();
}

GoogleString RewriteOptions::EnabledFiltersToString() const {
  GoogleString output;
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (Enabled(filter)) {
      StrAppend(&output, FilterId(filter), "\t", FilterName(filter), "\n");
    }
  }
  return output;
}

GoogleString RewriteOptions::SafeEnabledOptionsToString() const {
  GoogleString output;
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    OptionBase* option = all_options_[i];
    if (option->was_set() && option->property()->safe_to_print()) {
      GoogleString name_and_id =
          StrCat(option->option_name(), " (", option->id(), ") ");
      StrAppend(&output, name_and_id, option->ToString(), "\n");
    }
  }
  return output;
}

GoogleString RewriteOptions::OptionsToString() const {
  GoogleString output;
  StrAppend(&output, "Version: ", IntegerToString(kOptionsVersion), ": ");

  switch (enabled_.value()) {
    case kEnabledOff:
      StrAppend(&output, "off\n\n");
      break;
    case kEnabledOn:
      StrAppend(&output, "on\n\n");
      break;
    case kEnabledUnplugged:
      StrAppend(&output, "unplugged\n\n");
      break;
    case kEnabledStandby:
      StrAppend(&output, "standby\n\n");
      break;
  }
  output += "Filters\n";
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (Enabled(filter)) {
      StrAppend(&output, FilterId(filter), "\t", FilterName(filter), "\n");
    }
  }

  // Print the options.  Use two passes so we can line up the values, given that
  // the names have different widths.
  output += "\nOptions\n";
  StringVector names, values;
  int max_width = 0;
  for (int i = 0, n = all_options_.size(); i < n; ++i) {
    // Only including options with values overridden from the default.
    OptionBase* option = all_options_[i];
    if (option->was_set()) {
      GoogleString name_and_id =
          StrCat(option->option_name(), " (", option->id(), ")");
      max_width = std::max(max_width, static_cast<int>(name_and_id.size()));
      names.push_back(name_and_id);
      values.push_back(option->ToString());
    }
  }
  for (int i = 0, n = values.size(); i < n; ++i) {
    GoogleString spaces(max_width - names[i].size() + 2, ' ');
    StrAppend(&output, "  ", names[i], spaces, values[i], "\n");
  }

  output += "\nDomain Lawyer\n";
  StrAppend(&output, domain_lawyer_->ToString("  "));
  // TODO(mmohabey): Incorporate ToString() from the file_load_policy,
  // allow_resources, and retain_comments.

  if (!url_cache_invalidation_entries_.empty()) {
    StrAppend(&output, "\nURL cache invalidation entries\n");
    for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n; ++i) {
      StrAppend(&output, "  ", url_cache_invalidation_entries_[i]->ToString(),
                "\n");
    }
  }

  if (rejected_request_map_.size() > 0) {
    StrAppend(&output, "\nRejected request map\n");
    FastWildcardGroupMap::const_iterator it = rejected_request_map_.begin();
    for (; it != rejected_request_map_.end(); ++it) {
      StrAppend(&output, " ", it->first, " ", it->second->Signature(), "\n");
    }
  }
  GoogleString override_caching_wildcard_string(
      override_caching_wildcard_->Signature());
  if (!override_caching_wildcard_string.empty()) {
    StrAppend(&output, "\nOverride caching wildcards\n",
              override_caching_wildcard_string);
  }

  for (int i = 0, n = experiment_specs_.size(); i < n; ++i) {
    RewriteOptions::ExperimentSpec* spec = experiment_specs_[i];
    StrAppend(&output, "Experiment ", spec->ToString(), "\n");
  }

  {
    ThreadSystem::ScopedReader read_lock(cache_purge_mutex_.get());
    if (has_cache_invalidation_timestamp_ms()) {
      int64 cache_invalidation_ms = cache_invalidation_timestamp();
      GoogleString time_string;
      if ((cache_invalidation_ms > 0) &&
          ConvertTimeToString(cache_invalidation_ms, &time_string)) {
        StrAppend(&output, "\nInvalidation Timestamp: ", time_string, " (",
                  Integer64ToString(cache_invalidation_ms), ")\n");
      }
    } else {
      StrAppend(&output, "\nInvalidation Timestamp: (none)");
    }
  }

  return output;
}

GoogleString RewriteOptions::ExperimentSpec::QuoteHostPort(
    const GoogleString& in) {
  if (in.find(":") != GoogleString::npos) {
    return StrCat("\"", in, "\"");
  }
  return in;
}

GoogleString RewriteOptions::ExperimentSpec::ToString() const {
  GoogleString out;
  StrAppend(&out, "id=", IntegerToString(id_));
  if (ga_variable_slot_ != kDefaultExperimentSlot) {
    StrAppend(&out, "slot=", IntegerToString(ga_variable_slot_));
  }
  if (!ga_id_.empty()) {
    StrAppend(&out, ";ga=", ga_id_);
  }
  StrAppend(&out, ";percent=", IntegerToString(percent_));
  if (rewrite_level_ != kPassThrough) {
    StrAppend(&out, ";level=", RewriteOptions::ToString(rewrite_level_));
  }

  if (use_default_) {
    StrAppend(&out, ";default");
  }

  // TODO(jefftk): Put these in the form "rewrite_images" instead of "ri".
  const char* sep = ";enabled=";
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (enabled_filters_.IsSet(filter)) {
      StrAppend(&out, sep, FilterId(filter));
      sep = ",";
    }
  }

  sep = ";disabled=";
  for (int i = kFirstFilter; i != kEndOfFilters; ++i) {
    Filter filter = static_cast<Filter>(i);
    if (disabled_filters_.IsSet(filter)) {
      StrAppend(&out, sep, FilterId(filter));
      sep = ",";
    }
  }

  sep = ";options=";
  for (RewriteOptions::OptionSet::const_iterator p = filter_options_.begin(),
                                                 e = filter_options_.end();
       p != e; ++p) {
    StrAppend(&out, sep, p->first, "=", p->second);
    sep = ",";
  }

  if (matches_device_types_.get() != nullptr) {
    StrAppend(&out, ";matches_device_type=");
    sep = "";
    if ((*matches_device_types_)[UserAgentMatcher::kDesktop]) {
      StrAppend(&out, sep, "desktop");
      sep = ",";
    }
    if ((*matches_device_types_)[UserAgentMatcher::kTablet]) {
      StrAppend(&out, sep, "tablet");
      sep = ",";
    }
    if ((*matches_device_types_)[UserAgentMatcher::kMobile]) {
      StrAppend(&out, sep, "mobile");
      sep = ",";
    }
  }

  for (AlternateOriginDomains::const_iterator i =
           alternate_origin_domains_.begin();
       i != alternate_origin_domains_.end(); ++i) {
    const AlternateOriginDomainSpec& spec = *i;

    StringVector quoted_serving_domains = spec.serving_domains;
    for (StringVector::iterator i = quoted_serving_domains.begin();
         i != quoted_serving_domains.end(); ++i) {
      *i = QuoteHostPort(*i);
    }

    StrAppend(&out, ";alternate_origin_domain=",
              JoinCollection(quoted_serving_domains, ","), ":",
              QuoteHostPort(spec.origin_domain));

    if (!spec.host_header.empty()) {
      StrAppend(&out, ":", QuoteHostPort(spec.host_header));
    }
  }

  return out;
}

GoogleString RewriteOptions::ToExperimentString() const {
  // Only add the experiment id if we're running this experiment.
  if (GetExperimentSpec(experiment_id_) != nullptr) {
    return absl::StrFormat("Experiment: %d", experiment_id_);
  }
  return GoogleString();
}

GoogleString RewriteOptions::ToExperimentDebugString() const {
  GoogleString output = ToExperimentString();
  if (!output.empty()) {
    output += "; ";
  }
  if (!running_experiment()) {
    output += "off; ";
  } else if (experiment_id_ == experiment::kExperimentNotSet) {
    output += "not set; ";
  } else if (experiment_id_ == experiment::kNoExperiment) {
    output += "no experiment; ";
  } else {
    ExperimentSpec* spec = GetExperimentSpec(experiment_id_);
    if (spec != nullptr) {
      output += spec->ToString();
    }
  }
  return output;
}

void RewriteOptions::Modify() {
  DCHECK(!frozen_);
  modified_ = true;

  // The data in last_thread_id_ is currently only examined in DCHECKs so
  // there's no need to pay the cost of populating it in production.
#ifndef NDEBUG
  if (thread_system_ != nullptr) {
    if (last_thread_id_.get() == nullptr) {
      last_thread_id_.reset(thread_system_->GetThreadId());
    } else {
      DCHECK(ModificationOK());
    }
  }
#endif
}

// These method implementations are only in debug builds for asserting that
// the usage patterns are safe.  In fact we don't even have last_thread_id_
// compiled into the class in non-debug compiles.
#ifndef NDEBUG
bool RewriteOptions::ModificationOK() const {
  return ((last_thread_id_.get() == nullptr) ||
          (last_thread_id_->IsCurrentThread()));
}

bool RewriteOptions::MergeOK() const {
  return frozen_ || (last_thread_id_.get() == nullptr) ||
         last_thread_id_->IsCurrentThread();
}
#endif

bool RewriteOptions::ValidateConfiguredHttpHeader(const GoogleString& name,
                                                  const GoogleString& value,
                                                  GoogleString* error_message) {
  DCHECK(error_message != nullptr);

  if (error_message == nullptr) {
    return false;
  }

  // Reject empty field names. Empty field-values are acceptable.
  // Impose a max length of 1024 on both field-name and field-value.
  if (name.size() == 0) {
    *error_message = "Empty field name not allowed";
    return false;
  }
  if (name.size() > 1024) {
    *error_message = "Field name too long";
  }
  if (value.size() > 1024) {
    *error_message = "Field value too long";
    return false;
  }

  for (int i = 0, n = name.size(); i < n; ++i) {
    char c = name[i];
    if (!IsNonControlAscii(c)) {
      *error_message =
          StrCat("Invalid character in field name: ", GoogleString(1, c));
      return false;
    }
    if (http_header_separators[static_cast<unsigned int>(c)]) {
      *error_message =
          StrCat("Separator found in field name: ", GoogleString(1, c));
      return false;
    }
  }

  for (int i = 0, n = value.size(); i < n; ++i) {
    // Formally the value may have any OCTET except CNTL, but including LWS.
    // But in practice I think this is fair enough for our use case:
    if (!IsNonControlAscii(value[i])) {
      *error_message = StrCat("Invalid character in field value: ",
                              GoogleString(1, value[i]));
      return false;
    }
  }

  return true;
}

bool RewriteOptions::ValidateAndAddResourceHeader(const StringPiece& name,
                                                  const StringPiece& value,
                                                  GoogleString* error_message) {
  GoogleString gs_name;
  GoogleString gs_value;

  // Trim our input of whitespace, as we don't want any interpretation of that
  // (especially header folding), and it probably was not intentionally
  // configured like that anyway.
  TrimWhitespace(name, &gs_name);
  TrimWhitespace(value, &gs_value);

  if (ValidateConfiguredHttpHeader(gs_name, gs_value, error_message)) {
    // We don't allow overwriting hop-by-hop headers like Connection.
    StringPieceVector hbh = HttpAttributes::SortedHopByHopHeaders();
    StringCompareInsensitive compare;
    if (std::binary_search(&hbh[0], &hbh[0] + hbh.size(), gs_name, compare)) {
      *error_message = StrCat("Rejecting hop by hop header '", name, " '");
      return false;
    }

    // We don't allow overwriting of headers we compute, like Cache-Control.
    // In addition, there's a few other headers that should not be allowed
    // so reject those fields as well:
    const StringPiece* start = &fixed_resource_headers->at(0);
    if (std::binary_search(start, start + fixed_resource_headers->size(),
                           gs_name, compare)) {
      *error_message = StrCat("Rejecting header '", name, " '");
      return false;
    }

    // Arbitrary limit of adding 20 headers
    if (resource_headers_.size() > 20) {
      *error_message = "Too many AddResourceHeader directives (max: 20)";
      return false;
    }
    AddResourceHeader(gs_name, gs_value);
    return true;
  }
  return false;
}

void RewriteOptions::AddResourceHeader(const StringPiece& name,
                                       const StringPiece& value) {
  resource_headers_.push_back(new NameValue(name, value));
}

// TODO(oschaaf): should AddCustomFetchHeader have validations as well?
void RewriteOptions::AddCustomFetchHeader(const StringPiece& name,
                                          const StringPiece& value) {
  custom_fetch_headers_.push_back(new NameValue(name, value));
}

// We expect experiment_specs_.size() to be small (not more than 2 or 3)
// so there is no need to optimize this.
RewriteOptions::ExperimentSpec* RewriteOptions::GetExperimentSpec(
    int id) const {
  for (int i = 0, n = experiment_specs_.size(); i < n; ++i) {
    if (experiment_specs_[i]->id() == id) {
      return experiment_specs_[i];
    }
  }
  return nullptr;
}

bool RewriteOptions::AvailableExperimentId(int id) {
  if (id < 0 || id == experiment::kExperimentNotSet ||
      id == experiment::kNoExperiment) {
    return false;
  }
  return (GetExperimentSpec(id) == nullptr);
}

RewriteOptions::ExperimentSpec* RewriteOptions::AddExperimentSpec(
    const StringPiece& spec, MessageHandler* handler) {
  ExperimentSpec* f_spec = new ExperimentSpec(spec, this, handler);
  if (!InsertExperimentSpecInVector(f_spec)) {
    return nullptr;  // InsertExperimentSpecInVector deletes f_spec on failure.
  }
  return f_spec;
}

bool RewriteOptions::InsertExperimentSpecInVector(ExperimentSpec* spec) {
  // See RewriteOptions::GetExperimentStateStr for why we can't have more than
  // 26.
  if (!AvailableExperimentId(spec->id()) || spec->percent() < 0 ||
      experiment_percent_ + spec->percent() > 100 ||
      experiment_specs_.size() + 1 > 26) {
    delete spec;
    return false;
  }
  experiment_specs_.push_back(spec);
  experiment_percent_ += spec->percent();
  return true;
}

// Always enable add_head, insert_ga, add_instrumentation, and HtmlWriter.  This
// is considered a "no-filter" base for experiments.
// Note: insert_ga no longer needs add_head, but add_instrumentation still does.
bool RewriteOptions::SetupExperimentRewriters() {
  // Don't change anything if we're not in an experiment or have some
  // unset id.
  if (experiment_id_ == experiment::kExperimentNotSet ||
      experiment_id_ == experiment::kNoExperiment) {
    return true;
  }
  // Control: just make sure that the necessary stuff is on.
  // Do NOT try to set up things to look like the ExperimentSpec
  // for this id: it doesn't match the rewrite options.
  ExperimentSpec* spec = GetExperimentSpec(experiment_id_);
  if (spec == nullptr) {
    return false;
  }

  if (!spec->ga_id().empty()) {
    set_ga_id(spec->ga_id());
  }

  set_experiment_ga_slot(spec->slot());

  // 'default' means keep the current filters, otherwise clear them -and- set
  // the level. Note that we cannot set the level if 'default' is on because
  // the default level is PassThrough which breaks the idea of 'default'.
  if (!spec->use_default()) {
    ClearFilters();
    SetRewriteLevel(spec->rewrite_level());
  }
  EnableFilters(spec->enabled_filters());
  DisableFilters(spec->disabled_filters());
  // spec doesn't specify forbidden filters so no need to call ForbidFilters().
  // We need these for the experiment to work properly.
  SetRequiredExperimentFilters();
  // Options were already checked during config parsing.
  NullMessageHandler null_message_handler;
  SetOptionsFromName(spec->filter_options(), &null_message_handler);
  spec->ApplyAlternateOriginsToDomainLawyer(WriteableDomainLawyer(),
                                            &null_message_handler);
  return true;
}

void RewriteOptions::SetRequiredExperimentFilters() {
  ForceEnableFilter(RewriteOptions::kAddHead);
  ForceEnableFilter(RewriteOptions::kAddInstrumentation);
  ForceEnableFilter(RewriteOptions::kComputeStatistics);
  ForceEnableFilter(RewriteOptions::kInsertGA);
  ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
}

RewriteOptions::ExperimentSpec::ExperimentSpec(const StringPiece& spec,
                                               const RewriteOptions* options,
                                               MessageHandler* handler)
    : id_(experiment::kExperimentNotSet),
      ga_id_(options->ga_id()),
      ga_variable_slot_(options->experiment_ga_slot()),
      percent_(-1),
      rewrite_level_(kPassThrough),
      use_default_(false) {
  Initialize(spec, handler);
}

RewriteOptions::ExperimentSpec::ExperimentSpec(int id)
    : id_(id),
      ga_id_(""),
      ga_variable_slot_(kDefaultExperimentSlot),
      percent_(-1),
      rewrite_level_(kPassThrough),
      use_default_(false) {}

RewriteOptions::ExperimentSpec::~ExperimentSpec() {}

void RewriteOptions::ExperimentSpec::Merge(const ExperimentSpec& spec) {
  enabled_filters_.Merge(spec.enabled_filters_);
  disabled_filters_.Merge(spec.disabled_filters_);
  for (OptionSet::const_iterator iter = spec.filter_options_.begin();
       iter != spec.filter_options_.end(); ++iter) {
    filter_options_.insert(*iter);
  }
  ga_id_ = spec.ga_id_;
  ga_variable_slot_ = spec.ga_variable_slot_;
  percent_ = spec.percent_;
  rewrite_level_ = spec.rewrite_level_;
  use_default_ = spec.use_default_;
  if (spec.matches_device_types_.get() != nullptr) {
    matches_device_types_ =
        std::make_unique<DeviceTypeBitSet>(*spec.matches_device_types_);
  }
  if (!spec.alternate_origin_domains_.empty()) {
    alternate_origin_domains_ = spec.alternate_origin_domains_;
  }
}

RewriteOptions::ExperimentSpec* RewriteOptions::ExperimentSpec::Clone() {
  ExperimentSpec* ret = new ExperimentSpec(id_);
  ret->Merge(*this);
  return ret;
}

// Options are written in the form:
// ExperimentSpec 'id= 2; percent= 20; RewriteLevel= CoreFilters;
// enable= resize_images; disable = is; inline_css = 25556; ga=UA-233842-1'
void RewriteOptions::ExperimentSpec::Initialize(const StringPiece& spec,
                                                MessageHandler* handler) {
  StringPieceVector spec_pieces;
  SplitStringPieceToVector(spec, ";", &spec_pieces, true);
  for (int i = 0, n = spec_pieces.size(); i < n; ++i) {
    StringPiece piece = spec_pieces[i];
    TrimWhitespace(&piece);
    if (StringCaseStartsWith(piece, "id")) {
      StringPiece id = PieceAfterEquals(piece);
      if (id.length() > 0 && !StringToInt(id, &id_)) {
        // If we failed to turn this string into an int, then
        // set the id_ to kExperimentNotSet so we don't end up adding
        // in this spec.
        id_ = experiment::kExperimentNotSet;
      }
    } else if (StringCaseEqual(piece, "default")) {
      // "Default" means use whatever RewriteOptions are.
      use_default_ = true;
    } else if (StringCaseStartsWith(piece, "percent")) {
      StringPiece percent = PieceAfterEquals(piece);
      StringToInt(percent, &percent_);
    } else if (StringCaseStartsWith(piece, "ga")) {
      StringPiece ga = PieceAfterEquals(piece);
      if (ga.length() > 0) {
        ga_id_ = GoogleString(ga.data(), ga.length());
      }
    } else if (StringCaseStartsWith(piece, "slot")) {
      StringPiece slot = PieceAfterEquals(piece);
      int stored_id = ga_variable_slot_;
      StringToInt(slot, &ga_variable_slot_);
      // Valid custom variable slots are 1-5 inclusive.
      if (ga_variable_slot_ < 1 || ga_variable_slot_ > 5) {
        LOG(INFO) << "Invalid custom variable slot.";
        ga_variable_slot_ = stored_id;
      }
    } else if (StringCaseStartsWith(piece, "level")) {
      StringPiece level = PieceAfterEquals(piece);
      if (level.length() > 0) {
        ParseRewriteLevel(level, &rewrite_level_);
      }
    } else if (StringCaseStartsWith(piece, "enable")) {
      StringPiece enabled = PieceAfterEquals(piece);
      if (enabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(enabled, &enabled_filters_, handler);
      }
    } else if (StringCaseStartsWith(piece, "disable")) {
      StringPiece disabled = PieceAfterEquals(piece);
      if (disabled.length() > 0) {
        AddCommaSeparatedListToFilterSet(disabled, &disabled_filters_, handler);
      }
    } else if (StringCaseStartsWith(piece, "options")) {
      StringPiece options = PieceAfterEquals(piece);
      if (options.length() > 0) {
        AddCommaSeparatedListToOptionSet(options, &filter_options_, handler);
      }
    } else if (StringCaseStartsWith(piece, "matches_device_type")) {
      matches_device_types_ = std::make_unique<DeviceTypeBitSet>();
      ParseDeviceTypeBitSet(PieceAfterEquals(piece),
                            matches_device_types_.get(), handler);
    } else if (StringCaseStartsWith(piece, "alternate_origin_domain=")) {
      alternate_origin_domains_.push_back(AlternateOriginDomainSpec());
      if (!ParseAlternateOriginDomain(PieceAfterEquals(piece),
                                      &alternate_origin_domains_.back(),
                                      handler)) {
        handler->Message(kWarning,
                         "Ignorning invalid alternate_origin_domain: '%s'",
                         piece.as_string().c_str());
        alternate_origin_domains_.pop_back();
      }
    } else {
      handler->Message(kWarning, "Skipping unknown experiment setting: %s",
                       piece.as_string().c_str());
    }
  }
}

void RewriteOptions::ExperimentSpec::CombineQuotedHostPort(
    StringPieceVector* vec, size_t first_pos,
    GoogleString* combined_container) {
  if (first_pos + 1 >= vec->size()) {
    return;
  }

  StringPiece& a = (*vec)[first_pos];
  StringPiece& b = (*vec)[first_pos + 1];

  if (a.starts_with("\"") && b.ends_with("\"")) {
    a.remove_prefix(1);
    b.remove_suffix(1);
    *combined_container = a.as_string() + ":" + b.as_string();
    (*vec)[first_pos] = StringPiece(*combined_container);
    vec->erase(vec->begin() + first_pos + 1);
  }
}

bool RewriteOptions::ExperimentSpec::LooksLikeValidHost(
    const StringPiece& host_str) {
  StringPieceVector host_components;
  SplitStringPieceToVector(host_str, ":", &host_components, false);

  if (host_components.empty() || host_components.size() > 2) {
    return false;
  }

  // host_components[0] is the host component. Just check it contains
  // a non-numeric, so we know it's not empty or a stray port number.
  if (host_components[0].find_first_not_of("1234567890") == StringPiece::npos) {
    return false;
  }

  // host_components[1] is the port, which may not be present. Check it is
  // non-empty and contains only numbers.
  if (host_components.size() > 1 &&
      (host_components[1].empty() || host_components[1].find_first_not_of(
                                         "1234567890") != GoogleString::npos)) {
    return false;
  }

  return true;
}

bool RewriteOptions::ExperimentSpec::ParseAlternateOriginDomain(
    const StringPiece& in, AlternateOriginDomainSpec* out,
    MessageHandler* handler) {
  // Input format: serving_domain[,...]:alt_origin_domain[:host_header]
  // alt_origin_domain and host_header can include a port, in which case
  // they must be quoted:
  // serving_domain:"alt_origin_domain:port":"host_header:port".
  // A *single* serving_domain may have a port added in the same fashion.

  StringPieceVector args_str;
  SplitStringPieceToVector(in, ":", &args_str, false);

  // serving_domain can be a comma separated list, however this code that deals
  // with unescaping only allows a single quoted host:port in that field. Fixing
  // that properly is too much work for a feature only required by tests.
  GoogleString serving_combined_container;
  if (args_str.size() >= 2) {
    CombineQuotedHostPort(&args_str, 0, &serving_combined_container);
  }

  GoogleString ref_combined_container;
  if (args_str.size() >= 3) {
    CombineQuotedHostPort(&args_str, 1, &ref_combined_container);
  }

  GoogleString host_combined_container;
  if (args_str.size() >= 4) {
    CombineQuotedHostPort(&args_str, 2, &host_combined_container);
  }

  if (args_str.size() < 2 || args_str.size() > 3) {
    handler->Message(
        kWarning, "Incorrect number of arguments for alternate_origin_domain");
    return false;
  }

  out->serving_domains.clear();
  out->origin_domain = args_str[1].as_string();
  if (args_str.size() > 2) {
    out->host_header = args_str[2].as_string();
  } else {
    out->host_header.clear();
  }

  // We now attempt to configure a DomainLaywer with the supplied arguments.
  // If there is a problem, we want to find out now (ie: parse time) and
  // not when we later try and configure a DomainLawyer for real.
  // We also check for non-numeric in the headers, since that's likely a stray
  // port number and a valid hostname must contain a non-numeric.

  DomainLawyer lawyer;
  // origin_domain cannot be empty or the lawyer will be very unhappy.
  if (!LooksLikeValidHost(out->origin_domain) ||
      !lawyer.AddTwoProtocolOriginDomainMapping(out->origin_domain, "good.com",
                                                "", handler)) {
    handler->Message(kWarning, "Invalid origin domain: '%s'",
                     out->origin_domain.c_str());
    // This breaks *everything* else below, so we have to early exit.
    return false;
  }

  lawyer.Clear();
  if (!out->host_header.empty() &&
      (!LooksLikeValidHost(out->host_header) ||
       !lawyer.AddTwoProtocolOriginDomainMapping(out->origin_domain, "good.com",
                                                 out->host_header, handler))) {
    handler->Message(kWarning, "Invalid host header: '%s'",
                     out->host_header.c_str());
    return false;
  }

  StringPieceVector serving_domains;
  SplitStringPieceToVector(args_str[0], ",", &serving_domains, true);

  lawyer.Clear();

  for (StringPieceVector::const_iterator i = serving_domains.begin();
       i != serving_domains.end(); ++i) {
    const StringPiece& serving_domain = *i;
    if (LooksLikeValidHost(serving_domain) &&
        lawyer.AddTwoProtocolOriginDomainMapping(
            out->origin_domain, serving_domain, out->host_header, handler)) {
      out->serving_domains.push_back(serving_domain.as_string());
    } else {
      handler->Message(kWarning, "Invalid serving domain: '%s'",
                       serving_domain.as_string().c_str());
    }
  }

  return !out->serving_domains.empty();
}

bool RewriteOptions::ExperimentSpec::ParseDeviceTypeBitSet(
    const StringPiece& in, ExperimentSpec::DeviceTypeBitSet* out,
    MessageHandler* handler) {
  bool success = false;

  StringPieceVector devices;
  SplitStringPieceToVector(in, ",", &devices, true);

  for (int i = 0, n = devices.size(); i < n; ++i) {
    StringPiece device = devices[i];

    UserAgentMatcher::DeviceType device_type =
        UserAgentMatcher::kEndOfDeviceType;

    if (device == "desktop") {
      device_type = UserAgentMatcher::kDesktop;
    } else if (device == "mobile") {
      device_type = UserAgentMatcher::kMobile;
    } else if (device == "tablet") {
      device_type = UserAgentMatcher::kTablet;
    }

    if (device_type != UserAgentMatcher::kEndOfDeviceType) {
      out->set(device_type, true);
      success = true;
    } else {
      handler->Message(kWarning, "Skipping unknown device type: %s",
                       device.as_string().c_str());
    }
  }

  return success;
}

bool RewriteOptions::ExperimentSpec::matches_device_type(
    UserAgentMatcher::DeviceType type) const {
  // It would be nice to use matches_device_types_->size() for the second
  // if clause. Unfortunately, matches_device_types_ might be NULL and
  // size is not static, despite it being a template paramater.
  if (type < 0 || type >= UserAgentMatcher::kEndOfDeviceType) {
    LOG(DFATAL) << "DeviceType out of range: " << type;
    return false;
  }

  // If no device_type filter has been specified, this will match all devices.
  if (matches_device_types_.get() == nullptr) {
    return true;
  }

  return (*matches_device_types_)[type];
}

void RewriteOptions::ExperimentSpec::ApplyAlternateOriginsToDomainLawyer(
    DomainLawyer* lawyer, MessageHandler* handler) const {
  for (AlternateOriginDomains::const_iterator i =
           alternate_origin_domains_.begin();
       i != alternate_origin_domains_.end(); ++i) {
    const AlternateOriginDomainSpec& alt_spec = *i;

    for (StringVector::const_iterator j = alt_spec.serving_domains.begin();
         j != alt_spec.serving_domains.end(); ++j) {
      const GoogleString& serving_domain = *j;

      lawyer->AddTwoProtocolOriginDomainMapping(alt_spec.origin_domain,
                                                serving_domain,
                                                alt_spec.host_header, handler);
    }
  }
}

void RewriteOptions::AddInlineUnauthorizedResourceType(
    semantic_type::Category category) {
  inline_unauthorized_resource_types_.mutable_value().insert(category);
}

bool RewriteOptions::HasInlineUnauthorizedResourceType(
    semantic_type::Category category) const {
  return inline_unauthorized_resource_types_.value().find(category) !=
         inline_unauthorized_resource_types_.value().end();
}

void RewriteOptions::ClearInlineUnauthorizedResourceTypes() {
  inline_unauthorized_resource_types_.mutable_value().clear();
}

void RewriteOptions::set_inline_unauthorized_resource_types(
    ResourceCategorySet x) {
  set_option(x, &inline_unauthorized_resource_types_);
}

void RewriteOptions::AddUrlValuedAttribute(const StringPiece& element,
                                           const StringPiece& attribute,
                                           semantic_type::Category category) {
  if (url_valued_attributes_ == nullptr) {
    url_valued_attributes_ =
        std::make_unique<std::vector<ElementAttributeCategory>>();
  }
  ElementAttributeCategory eac;
  element.CopyToString(&eac.element);
  attribute.CopyToString(&eac.attribute);
  eac.category = category;
  url_valued_attributes_->push_back(eac);
}

void RewriteOptions::UrlValuedAttribute(
    int index, StringPiece* element, StringPiece* attribute,
    semantic_type::Category* category) const {
  const ElementAttributeCategory& eac = (*url_valued_attributes_)[index];
  *element = StringPiece(eac.element);
  *attribute = StringPiece(eac.attribute);
  *category = eac.category;
}

bool RewriteOptions::IsUrlCacheValid(StringPiece url, int64 time_ms,
                                     bool search_wildcards) const {
  {
    ThreadSystem::ScopedReader read_lock(cache_purge_mutex_.get());
    if (!purge_set_->IsValid(url.as_string(), time_ms)) {
      return false;
    }
  }

  if (!search_wildcards) {
    return true;
  }

  // Check legacy wildcards.  Hopefully there aren't any or this may be
  // quite slow.
  int i = 0;
  int n = url_cache_invalidation_entries_.size();
  while (i < n && time_ms > url_cache_invalidation_entries_[i]->timestamp_ms) {
    ++i;
  }
  // Now all entries from 0 to i-1 have timestamp less than time_ms and hence
  // cannot invalidate a url cached at time_ms.
  // TODO(sriharis):  Should we use binary search instead of the above loop?
  // Probably does not make sense as long as the following while loop is there.

  // Once FastWildcardGroup is in, we should check if it makes sense to make a
  // FastWildcardGroup of Wildcards from position i to n-1, and Match against
  // it.
  while (i < n) {
    if (url_cache_invalidation_entries_[i]->url_pattern.Match(url)) {
      return false;
    }
    ++i;
  }
  return true;
}

void RewriteOptions::PurgeUrl(StringPiece url, int64 timestamp_ms) {
  ScopedMutex lock(cache_purge_mutex_.get());
  // Note that in this API, we do not handle failure due to moving
  // backwards in time.  This API is used for collecting purge-records
  // from a database, and not for handling PURGE http requests.  That
  // is handled in ../apache/instaweb_handler.cc, handle_purge_request().
  purge_set_.MakeWriteable()->Put(url.as_string(), timestamp_ms);
}

void RewriteOptions::AddUrlCacheInvalidationEntry(
    StringPiece url_pattern, int64 timestamp_ms,
    bool ignores_metadata_and_pcache) {
  if (enable_cache_purge() && !ignores_metadata_and_pcache &&
      (url_pattern.find('*') == StringPiece::npos)) {
    // We could use Wildcard::IsSimple but let's define ? to mean in this
    // context a literal '?' because query-params are way more common than
    // single-char matching.
    PurgeUrl(url_pattern, timestamp_ms);
  } else {
    if (!url_cache_invalidation_entries_.empty()) {
      // Check that this Add preserves the invariant that
      // url_cache_invalidation_entries_ is sorted on timestamp_ms.
      if (url_cache_invalidation_entries_.back()->timestamp_ms > timestamp_ms) {
        LOG(DFATAL) << "Timestamp " << timestamp_ms << " is less than the last "
                    << "timestamp already added: "
                    << url_cache_invalidation_entries_.back()->timestamp_ms;
        return;
      }
    }
    url_cache_invalidation_entries_.push_back(new UrlCacheInvalidationEntry(
        url_pattern, timestamp_ms, ignores_metadata_and_pcache));
  }
}

bool RewriteOptions::UpdateCacheInvalidationTimestampMs(int64 timestamp_ms) {
  ScopedMutex lock(cache_purge_mutex_.get());
  DCHECK_LT(0, timestamp_ms);
  bool ret = false;
  if (purge_set_->global_invalidation_timestamp_ms() < timestamp_ms) {
    bool recompute_signature = ClearSignatureWithCaution();
    ret = purge_set_.MakeWriteable()->UpdateGlobalInvalidationTimestampMs(
        timestamp_ms);
    Modify();
    if (recompute_signature) {
      signature_.clear();
      ComputeSignatureLockHeld();
    }
  }
  return ret;
}

int64 RewriteOptions::cache_invalidation_timestamp() const {
  ThreadSystem::ScopedReader lock(cache_purge_mutex_.get());
  DCHECK(purge_set_->has_global_invalidation_timestamp_ms());
  return purge_set_->global_invalidation_timestamp_ms();
}

bool RewriteOptions::has_cache_invalidation_timestamp_ms() const {
  ThreadSystem::ScopedReader lock(cache_purge_mutex_.get());
  return purge_set_->has_global_invalidation_timestamp_ms();
}

bool RewriteOptions::UpdateCachePurgeSet(
    const CopyOnWrite<PurgeSet>& purge_set) {
  bool ret = false;
  ScopedMutex lock(cache_purge_mutex_.get());
  if (purge_set_.get() != purge_set.get()) {
    bool recompute_signature = ClearSignatureWithCaution();
    purge_set_ = purge_set;
    Modify();
    if (recompute_signature) {
      signature_.clear();
      ComputeSignatureLockHeld();
    }
    ret = true;
  }
  return ret;
}

GoogleString RewriteOptions::PurgeSetString() const {
  ScopedMutex lock(cache_purge_mutex_.get());
  return purge_set_->ToString();
}

bool RewriteOptions::IsUrlCacheInvalidationEntriesSorted() const {
  for (int i = 0, n = url_cache_invalidation_entries_.size(); i < n - 1; ++i) {
    if (url_cache_invalidation_entries_[i]->timestamp_ms >
        url_cache_invalidation_entries_[i + 1]->timestamp_ms) {
      return false;
    }
  }
  return true;
}

HttpOptions RewriteOptions::ComputeHttpOptions() const {
  HttpOptions options;
  options.respect_vary = respect_vary();
  options.implicit_cache_ttl_ms = implicit_cache_ttl_ms();
  return options;
}

bool RewriteOptions::NeedsDependenciesCohort() const {
  return Enabled(kExperimentHttp2) || Enabled(kHintPreloadSubresources);
}

bool RewriteOptions::CacheFragmentOption::SetFromString(
    StringPiece value, GoogleString* error_detail) {
  // The main thing here is that the fragment not contain '/' (the separator
  // used by HTTPCache) or '.' (so that a fragment can't be confused for a Host:
  // header) but use a whitelist to be on the safe side.
  //
  // This has security implications.  If you could set the fragment to
  // "good.com" when running "evil.com" you could posion good.com's cache.  See
  // the comment at the top of http_cache.h.
  for (int i = 0, n = value.length(); i < n; ++i) {
    const char c = value.data()[i];
    if (!IsAsciiAlphaNumeric(c) && c != '-' && c != '_') {
      *error_detail =
          ("A CacheFragment must be only letters, numbers, "
           "underscores and hyphens.  Found '");
      *error_detail += c;
      *error_detail += "'.";
      return false;
    }
  }
  set(value.as_string());
  return true;
}

GoogleString RewriteOptions::ScopeEnumToString(OptionScope scope) {
  switch (scope) {
    case kQueryScope:
      return "Query";
    case kDirectoryScope:
      return "Directory";
    case kServerScope:
      return "Server";
    case kLegacyProcessScope:
      return "Process";
    case kProcessScopeStrict:
      return "Process Strict";
    default:
      return "Unknown";
  }
}

bool RewriteOptions::CheckLevelSpecificOption(
    RewriteLevel rewrite_level, const Option<bool>& option) const {
  if (option.was_set() || (level() != rewrite_level)) {
    return option.value();
  }
  return true;
}

int64 RewriteOptions::ImageJpegQuality() const {
  int64 quality = image_jpeg_recompress_quality_.value();
  if (quality < 0) {
    quality = image_recompress_quality_.value();
  }
  return quality;
}

int64 RewriteOptions::ImageJpegQualityForSmallScreen() const {
  int64 quality = image_jpeg_recompress_quality_for_small_screens_.value();
  if (quality < 0) {
    quality = ImageJpegQuality();
  }
  return quality;
}

int64 RewriteOptions::ImageJpegQualityForSaveData() const {
  int64 quality = image_jpeg_quality_for_save_data_.value();
  if (quality < 0) {
    quality = ImageJpegQuality();
  }
  return quality;
}

int64 RewriteOptions::ImageWebpQuality() const {
  int64 quality = image_webp_recompress_quality_.value();
  if (quality < 0) {
    quality = image_recompress_quality_.value();
  }
  return quality;
}

int64 RewriteOptions::ImageWebpQualityForSmallScreen() const {
  int64 quality = image_webp_recompress_quality_for_small_screens_.value();
  if (quality < 0) {
    quality = ImageWebpQuality();
  }
  return quality;
}

int64 RewriteOptions::ImageWebpQualityForSaveData() const {
  int64 quality = image_webp_quality_for_save_data_.value();
  if (quality < 0) {
    quality = ImageWebpQuality();
  }
  return quality;
}

int64 RewriteOptions::ImageWebpAnimatedQuality() const {
  int64 quality = image_webp_animated_recompress_quality_.value();
  if (quality < 0) {
    quality = ImageWebpQuality();
  }
  return quality;
}

int64 RewriteOptions::ImageJpegNumProgressiveScansForSmallScreen() const {
  int64 num = image_jpeg_num_progressive_scans_for_small_screens_.value();
  if (num < 0) {
    num = image_jpeg_num_progressive_scans_.value();
  }
  return num;
}

bool RewriteOptions::HasValidSmallScreenQualities() const {
  return (ImageWebpQualityForSmallScreen() != ImageWebpQuality() ||
          ImageJpegQualityForSmallScreen() != ImageJpegQuality());
}

bool RewriteOptions::HasValidSaveDataQualities() const {
  return (ImageWebpQualityForSaveData() != ImageWebpQuality() ||
          ImageJpegQualityForSaveData() != ImageJpegQuality());
}

}  // namespace net_instaweb
