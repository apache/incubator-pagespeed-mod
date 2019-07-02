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

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_options_test_base.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/message_handler_test_base.h"
#include "pagespeed/kernel/base/mock_hasher.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_thread_system.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

class RewriteOptionsTest : public RewriteOptionsTestBase<RewriteOptions> {
 protected:
  typedef RewriteOptions::FilterSet FilterSet;

  RewriteOptionsTest() : options_(&thread_system_) {
  }

  bool NoneEnabled() {
    FilterSet s;
    return OnlyEnabled(s);
  }

  bool OnlyEnabled(const FilterSet& filters) {
    bool ret = true;
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         ret && (f < RewriteOptions::kEndOfFilters);
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (filters.IsSet(f)) {
        if (!options_.Enabled(f)) {
          ret = false;
        }
      } else {
        if (options_.Enabled(f)) {
          ret = false;
        }
      }
    }
    return ret;
  }

  bool OnlyEnabled(RewriteOptions::Filter filter) {
    FilterSet s;
    s.Insert(filter);
    return OnlyEnabled(s);
  }

  void MergeOptions(const RewriteOptions& one, const RewriteOptions& two) {
    options_.Merge(one);
    options_.Merge(two);
  }

  // Tests either SetOptionFromName or SetOptionFromNameAndLog depending
  // on 'test_log_variant'
  void TestNameSet(RewriteOptions::OptionSettingResult expected_result,
                   bool test_log_variant,
                   const StringPiece& name,
                   const StringPiece& value,
                   MessageHandler* handler) {
    if (test_log_variant) {
      bool expected = (expected_result == RewriteOptions::kOptionOk);
      EXPECT_EQ(
          expected,
          options_.SetOptionFromNameAndLog(name, value, handler));
    } else {
      GoogleString msg;
      EXPECT_EQ(expected_result,
                options_.SetOptionFromName(name, value, &msg));
      // Should produce a message exactly when not OK.
      EXPECT_EQ(expected_result != RewriteOptions::kOptionOk, !msg.empty())
          << msg;
    }
  }

  // Helper method that is used to verify different kinds of merges between
  // InlineResourcesWithoutExplicitAuthorization values for global and local
  // options.
  void VerifyInlineUnauthorizedResourceTypeMerges(
      StringPiece global_option_val,
      StringPiece local_option_val,
      bool expect_script,
      bool expect_stylesheet) {
    scoped_ptr<RewriteOptions> new_options(new RewriteOptions(&thread_system_));
    // Initialize global options.
    scoped_ptr<RewriteOptions> global_options(
        new RewriteOptions(&thread_system_));
    if (!global_option_val.empty()) {
      RewriteOptions::ResourceCategorySet x;
      ASSERT_TRUE(RewriteOptions::ParseInlineUnauthorizedResourceType(
                      global_option_val, &x));
      global_options->set_inline_unauthorized_resource_types(x);
    }
    // Initialize local options.
    RewriteOptions local_options(&thread_system_);
    if (!local_option_val.empty()) {
      RewriteOptions::ResourceCategorySet x;
      ASSERT_TRUE(RewriteOptions::ParseInlineUnauthorizedResourceType(
                      local_option_val, &x));
      local_options.set_inline_unauthorized_resource_types(x);
    }

    // Merge the options.
    new_options->Merge(*global_options);
    new_options->Merge(local_options);

    // Check what resource types have been authorized.
    EXPECT_EQ(
        expect_script,
        new_options->HasInlineUnauthorizedResourceType(semantic_type::kScript))
        << "Global: " << global_option_val << ", local: " << local_option_val;
    EXPECT_EQ(
        expect_stylesheet,
        new_options->HasInlineUnauthorizedResourceType(
            semantic_type::kStylesheet))
        << "Global: " << global_option_val << ", local: " << local_option_val;
  }

  // Adds an experiment spec to the options.  We take the spec as a
  // const char* and make a scoped GoogleString specifically to reproduce
  // a bug with lifetime of the experiment option names.
  bool AddExperimentSpec(const char* spec) {
    NullMessageHandler handler;
    GoogleString spec_string(spec);
    return options_.AddExperimentSpec(spec_string, &handler);
  }

  void SetupTestExperimentSpecs() {
    options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
    options_.set_running_experiment(true);

    EXPECT_TRUE(AddExperimentSpec("id=1;percent=15;enable=defer_javascript;"
                                  "options=CssInlineMaxBytes=1024"));
    EXPECT_TRUE(AddExperimentSpec(
        "id=2;percent=15;enable=resize_images;options=BogusOption=35"));
    EXPECT_TRUE(AddExperimentSpec("id=3;percent=15;enable=defer_javascript"));
    EXPECT_TRUE(AddExperimentSpec("id=4;percent=15;enable=defer_javascript;"
                                  "options=CssInlineMaxBytes=Cabbage"));
    EXPECT_TRUE(AddExperimentSpec(
        "id=5;percent=15;enable=defer_javascript;"
        "options=Potato=Carrot,5=10,6==9,CssInlineMaxBytes=1024"));
    EXPECT_TRUE(AddExperimentSpec(
        "id=6;percent=15;enable=defer_javascript;"
        "options=JsOutlineMinBytes=4096,JpegRecompresssionQuality=50,"
        "CssInlineMaxBytes=100,JsInlineMaxBytes=123"));
  }

  void VerifyMapOrigin(const DomainLawyer& lawyer,
                       const GoogleString& serving_url,
                       const GoogleString& expected_origin_domain,
                       const GoogleString& expected_host_header,
                       bool expected_is_proxy) {
    GoogleString actual_origin_domain;
    GoogleString actual_host_header;
    bool actual_is_proxy;

    EXPECT_TRUE(lawyer.MapOrigin(serving_url, &actual_origin_domain,
                                 &actual_host_header, &actual_is_proxy));

    EXPECT_EQ(expected_origin_domain, actual_origin_domain);
    EXPECT_EQ(expected_host_header, actual_host_header);
    EXPECT_EQ(expected_is_proxy, actual_is_proxy);
  }

  void VerifyNoMapOrigin(const DomainLawyer& lawyer,
                         const GoogleString& serving_domain) {
    GoogleUrl url(serving_domain);

    ASSERT_TRUE(url.IsWebValid());
    EXPECT_FALSE(lawyer.IsOriginKnown(url));
  }

  void VerifyAllowVaryOn(const GoogleString& input_str,
                         bool expected_valid,
                         bool expected_allow_auto,
                         bool expected_allow_save_data,
                         bool expected_allow_user_agent,
                         bool expected_allow_accept,
                         const GoogleString& expected_str) {
    RewriteOptions::OptionSettingResult is_valid =
        options_.SetOptionFromName(RewriteOptions::kAllowVaryOn, input_str);

    if (expected_valid) {
      EXPECT_EQ(RewriteOptions::kOptionOk, is_valid);
    } else {
      EXPECT_EQ(RewriteOptions::kOptionValueInvalid, is_valid);
      return;  // No more checking
    }
    EXPECT_EQ(expected_allow_auto, options_.AllowVaryOnAuto());
    EXPECT_EQ(expected_allow_save_data, options_.AllowVaryOnSaveData());
    EXPECT_EQ(expected_allow_user_agent,
              options_.AllowVaryOnUserAgent());
    EXPECT_EQ(expected_allow_accept, options_.AllowVaryOnAccept());
    EXPECT_STREQ(expected_str, options_.AllowVaryOnToString());
  }

  void VerifyMergingAllowVaryOn(const GoogleString& old_option_str,
                                const GoogleString& new_option_str,
                                const GoogleString& expected_option_str) {
    RewriteOptions merged_options(&thread_system_);
    RewriteOptions new_options(&thread_system_);
    if (!old_option_str.empty()) {
      EXPECT_EQ(RewriteOptions::kOptionOk,
                merged_options.SetOptionFromName(RewriteOptions::kAllowVaryOn,
                                                 old_option_str));
    }
    if (!new_option_str.empty()) {
      EXPECT_EQ(RewriteOptions::kOptionOk,
                new_options.SetOptionFromName(RewriteOptions::kAllowVaryOn,
                                              new_option_str));
    }
    merged_options.Merge(new_options);
    EXPECT_STREQ(expected_option_str, merged_options.AllowVaryOnToString());
  }

  void TestSetOptionFromName(bool test_log_variant);

  NullThreadSystem thread_system_;
  RewriteOptions options_;
  MockHasher hasher_;
};

TEST_F(RewriteOptionsTest, EnabledStates) {
  options_.set_enabled(RewriteOptions::kEnabledUnplugged);
  ASSERT_FALSE(options_.enabled());
  ASSERT_TRUE(options_.unplugged());
  options_.set_enabled(RewriteOptions::kEnabledOff);
  ASSERT_FALSE(options_.enabled());
  ASSERT_FALSE(options_.unplugged());
  options_.set_enabled(RewriteOptions::kEnabledOn);
  ASSERT_TRUE(options_.enabled());
  ASSERT_FALSE(options_.unplugged());
  options_.set_enabled(RewriteOptions::kEnabledStandby);
  ASSERT_FALSE(options_.enabled());
  ASSERT_FALSE(options_.unplugged());
}

TEST_F(RewriteOptionsTest, DefaultEnabledFilters) {
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, InstrumentationDisabled) {
  // Make sure the kCoreFilters enables some filters.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Now disable all filters and make sure none are enabled.
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
  }
  ASSERT_TRUE(NoneEnabled());
}

TEST_F(RewriteOptionsTest, DisableTrumpsEnable) {
  // Disable the default filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    options_.DisableFilter(f);
    options_.EnableFilter(f);
  }
}

TEST_F(RewriteOptionsTest, ForceEnableFilter) {
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);
  options_.EnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));

  options_.ForceEnableFilter(RewriteOptions::kHtmlWriterFilter);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kHtmlWriterFilter));
}

TEST_F(RewriteOptionsTest, NumFilterInLevels) {
  const RewriteOptions::RewriteLevel levels[] = {
      RewriteOptions::kOptimizeForBandwidth,
      RewriteOptions::kCoreFilters,
      RewriteOptions::kMobilizeFilters,
      RewriteOptions::kTestingCoreFilters,
      RewriteOptions::kAllFilters
  };

  for (int i = 0; i < arraysize(levels); ++i) {
    options_.SetRewriteLevel(levels[i]);
    FilterSet s;
    for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
         f < RewriteOptions::kEndOfFilters;
         f = static_cast<RewriteOptions::Filter>(f + 1)) {
      if (options_.Enabled(f)) {
        s.Insert(f);
      }
    }

    // Make sure that more than one filter is enabled in the filter set.
    ASSERT_GT(s.size(), 1);
  }
}

TEST_F(RewriteOptionsTest, Enable) {
  FilterSet s;
  for (RewriteOptions::Filter f = RewriteOptions::kFirstFilter;
       f < RewriteOptions::kEndOfFilters;
       f = static_cast<RewriteOptions::Filter>(f + 1)) {
    s.Insert(f);
    s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
    options_.EnableFilter(f);
    ASSERT_TRUE(OnlyEnabled(s));
  }
}

TEST_F(RewriteOptionsTest, CommaSeparatedList) {
  FilterSet s;
  s.Insert(RewriteOptions::kAddInstrumentation);
  s.Insert(RewriteOptions::kLeftTrimUrls);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  static const char kList[] = "add_instrumentation,trim_urls";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, CompoundFlag) {
  FilterSet s;
  s.Insert(RewriteOptions::kConvertGifToPng);
  s.Insert(RewriteOptions::kConvertJpegToProgressive);
  s.Insert(RewriteOptions::kConvertJpegToWebp);
  s.Insert(RewriteOptions::kConvertPngToJpeg);
  s.Insert(RewriteOptions::kConvertToWebpLossless);
  s.Insert(RewriteOptions::kInlineImages);
  s.Insert(RewriteOptions::kJpegSubsampling);
  s.Insert(RewriteOptions::kRecompressJpeg);
  s.Insert(RewriteOptions::kRecompressPng);
  s.Insert(RewriteOptions::kRecompressWebp);
  s.Insert(RewriteOptions::kResizeImages);
  s.Insert(RewriteOptions::kStripImageMetaData);
  s.Insert(RewriteOptions::kStripImageColorProfile);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  static const char kList[] = "rewrite_images";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, CompoundFlagRecompressImages) {
  FilterSet s;
  s.Insert(RewriteOptions::kConvertGifToPng);
  s.Insert(RewriteOptions::kConvertJpegToProgressive);
  s.Insert(RewriteOptions::kConvertJpegToWebp);
  s.Insert(RewriteOptions::kConvertPngToJpeg);
  s.Insert(RewriteOptions::kJpegSubsampling);
  s.Insert(RewriteOptions::kRecompressJpeg);
  s.Insert(RewriteOptions::kRecompressPng);
  s.Insert(RewriteOptions::kRecompressWebp);
  s.Insert(RewriteOptions::kStripImageMetaData);
  s.Insert(RewriteOptions::kStripImageColorProfile);
  s.Insert(RewriteOptions::kHtmlWriterFilter);  // enabled by default
  static const char kList[] = "recompress_images";
  NullMessageHandler handler;
  ASSERT_TRUE(
      options_.EnableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(s));
  ASSERT_TRUE(
      options_.DisableFiltersByCommaSeparatedList(kList, &handler));
  ASSERT_TRUE(OnlyEnabled(RewriteOptions::kHtmlWriterFilter));  // default
}

TEST_F(RewriteOptionsTest, ParseRewriteLevel) {
  RewriteOptions::RewriteLevel level;
  EXPECT_TRUE(RewriteOptions::ParseRewriteLevel("PassThrough", &level));
  EXPECT_EQ(RewriteOptions::kPassThrough, level);

  EXPECT_TRUE(RewriteOptions::ParseRewriteLevel("CoreFilters", &level));
  EXPECT_EQ(RewriteOptions::kCoreFilters, level);

  EXPECT_TRUE(RewriteOptions::ParseRewriteLevel("MobilizeFilters", &level));
  EXPECT_EQ(RewriteOptions::kMobilizeFilters, level);

  EXPECT_FALSE(RewriteOptions::ParseRewriteLevel(StringPiece(), &level));
  EXPECT_FALSE(RewriteOptions::ParseRewriteLevel("", &level));
  EXPECT_FALSE(RewriteOptions::ParseRewriteLevel("Garbage", &level));
}

TEST_F(RewriteOptionsTest, IsRequestDeclined) {
  RewriteOptions one(&thread_system_);
  one.AddRejectedUrlWildcard("*blocked*");
  one.AddRejectedHeaderWildcard(HttpAttributes::kUserAgent,
                                "*blocked UA*");
  one.AddRejectedHeaderWildcard(HttpAttributes::kXForwardedFor,
                                "12.34.13.*");

  RequestHeaders headers;
  headers.Add(HttpAttributes::kUserAgent, "Chrome");
  ASSERT_FALSE(one.IsRequestDeclined("www.test.com/a", &headers));
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/blocked", &headers));

  headers.Add(HttpAttributes::kUserAgent, "this is blocked UA agent");
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/a", &headers));

  headers.Add(HttpAttributes::kUserAgent, "Chrome");
  headers.Add(HttpAttributes::kXForwardedFor, "12.34.13.1");
  ASSERT_TRUE(one.IsRequestDeclined("www.test.com/a", &headers));

  headers.Clear();
  ASSERT_FALSE(one.IsRequestDeclined("www.test.com/a", &headers));
}

TEST_F(RewriteOptionsTest, IsRequestDeclinedMerge) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  RequestHeaders headers;
  one.AddRejectedUrlWildcard("http://www.a.com/b/*");
  EXPECT_TRUE(one.IsRequestDeclined("http://www.a.com/b/sdsd123", &headers));
  EXPECT_FALSE(one.IsRequestDeclined("http://www.a.com/", &headers));
  EXPECT_FALSE(one.IsRequestDeclined("http://www.b.com/b/", &headers));

  two.AddRejectedHeaderWildcard(HttpAttributes::kUserAgent, "*Chrome*");
  two.AddRejectedUrlWildcard("http://www.b.com/b/*");
  MergeOptions(one, two);

  EXPECT_TRUE(options_.IsRequestDeclined("http://www.a.com/b/sds13", &headers));
  EXPECT_FALSE(options_.IsRequestDeclined("http://www.a.com/", &headers));
  EXPECT_TRUE(options_.IsRequestDeclined("http://www.b.com/b/", &headers));

  headers.Add(HttpAttributes::kUserAgent, "firefox");
  EXPECT_FALSE(options_.IsRequestDeclined("http://www.a.com/", &headers));

  headers.Add(HttpAttributes::kUserAgent, "abc Chrome 456");
  EXPECT_TRUE(options_.IsRequestDeclined("http://www.a.com/", &headers));
}


TEST_F(RewriteOptionsTest, MergeLevelsDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOneCoreTwoPass) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kPassThrough, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsOnePassTwoCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kPassThrough);  // overrides default
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);  // overrides one
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeLevelsBothCore) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
}

TEST_F(RewriteOptionsTest, MergeFilterPassThrough) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterEnaOneDisTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  two.DisableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeFilterDisOneEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.DisableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kAddHead);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, MergeCoreFilter) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterEnaOneDisTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.EnableFilter(RewriteOptions::kExtendCacheImages);
  two.DisableFilter(RewriteOptions::kExtendCacheImages);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, MergeCoreFilterDisOneEnaTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.DisableFilter(RewriteOptions::kExtendCacheScripts);
  two.EnableFilter(RewriteOptions::kExtendCacheScripts);
  MergeOptions(one, two);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
}

TEST_F(RewriteOptionsTest, MergeThresholdDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_EQ(RewriteOptions::kDefaultCssInlineMaxBytes,
            options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_css_inline_max_bytes(5);
  MergeOptions(one, two);
  EXPECT_EQ(5, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeThresholdOverride) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.set_css_inline_max_bytes(5);
  two.set_css_inline_max_bytes(6);
  MergeOptions(one, two);
  EXPECT_EQ(6, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampDefault) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  MergeOptions(one, two);
  EXPECT_FALSE(options_.has_cache_invalidation_timestamp_ms());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOne) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.UpdateCacheInvalidationTimestampMs(11111111);
  MergeOptions(one, two);
  EXPECT_EQ(11111111, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwo) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  two.UpdateCacheInvalidationTimestampMs(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampOneLarger) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.UpdateCacheInvalidationTimestampMs(33333333);
  two.UpdateCacheInvalidationTimestampMs(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(33333333, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeCacheInvalidationTimeStampTwoLarger) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.UpdateCacheInvalidationTimestampMs(11111111);
  two.UpdateCacheInvalidationTimestampMs(22222222);
  MergeOptions(one, two);
  EXPECT_EQ(22222222, options_.cache_invalidation_timestamp());
}

TEST_F(RewriteOptionsTest, MergeOnlyProcessScopeOptions) {
  RewriteOptions dest(&thread_system_), src(&thread_system_);
  dest.set_image_max_rewrites_at_once(2);
  dest.set_max_url_segment_size(1);
  src.set_image_max_rewrites_at_once(5);
  src.set_max_url_segment_size(4);

  dest.MergeOnlyProcessScopeOptions(src);
  // Pulled in set_image_max_rewrites_at_once, which is process scope,
  // but not the other option.
  EXPECT_EQ(5, dest.image_max_rewrites_at_once());
  EXPECT_EQ(1, dest.max_url_segment_size());
}

TEST_F(RewriteOptionsTest, Allow) {
  options_.Allow("*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("a*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  options_.Allow("ab*.css");
  EXPECT_TRUE(options_.IsAllowed("abcd.css"));
  options_.Disallow("abc*.css");
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
}

TEST_F(RewriteOptionsTest, MergeAllow) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.Allow("*.css");
  EXPECT_TRUE(one.IsAllowed("abcd.css"));
  one.Disallow("a*.css");
  EXPECT_FALSE(one.IsAllowed("abcd.css"));

  two.Allow("ab*.css");
  EXPECT_TRUE(two.IsAllowed("abcd.css"));
  two.Disallow("abc*.css");
  EXPECT_FALSE(two.IsAllowed("abcd.css"));

  MergeOptions(one, two);
  EXPECT_FALSE(options_.IsAllowed("abcd.css"));
  EXPECT_FALSE(options_.IsAllowed("abc.css"));
  EXPECT_TRUE(options_.IsAllowed("ab.css"));
  EXPECT_FALSE(options_.IsAllowed("a.css"));
}

TEST_F(RewriteOptionsTest, DisableAllFilters) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  two.DisableAllFilters();  // Should disable both.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));

  MergeOptions(one, two);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersNotExplicitlyEnabled) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.EnableFilter(RewriteOptions::kAddHead);
  two.EnableFilter(RewriteOptions::kExtendCacheCss);
  two.DisableAllFiltersNotExplicitlyEnabled();  // Should disable AddHead.
  MergeOptions(one, two);

  // Make sure AddHead enabling didn't leak through.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddHead));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

TEST_F(RewriteOptionsTest, DisableAllFiltersOverrideFilterLevel) {
  // Disable the default enabled filter.
  options_.DisableFilter(RewriteOptions::kHtmlWriterFilter);

  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.EnableFilter(RewriteOptions::kAddHead);
  options_.DisableAllFiltersNotExplicitlyEnabled();

  // Check that *only* AddHead is enabled, even though we have CoreFilters
  // level set.
  EXPECT_TRUE(OnlyEnabled(RewriteOptions::kAddHead));
}

TEST_F(RewriteOptionsTest, ForbidFilter) {
  // Forbid a core filter: this will disable it.
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.ForbidFilter(RewriteOptions::kExtendCacheCss);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));

  // Forbid a filter, then try to merge in an enablement: it won't take.
  // At the same time, merge in a new "forbiddenment": it will take.
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.SetRewriteLevel(RewriteOptions::kCoreFilters);
  one.ForbidFilter(RewriteOptions::kExtendCacheCss);
  two.SetRewriteLevel(RewriteOptions::kCoreFilters);
  two.ForbidFilter(RewriteOptions::kFlattenCssImports);
  one.Merge(two);
  EXPECT_FALSE(one.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_FALSE(one.Enabled(RewriteOptions::kFlattenCssImports));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kExtendCacheCss)));
  EXPECT_TRUE(one.Forbidden(
      RewriteOptions::FilterId(RewriteOptions::kFlattenCssImports)));
}

TEST_F(RewriteOptionsTest, AllDoesNotImplyStripScrips) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kStripScripts));
}

TEST_F(RewriteOptionsTest, ExplicitlyEnabledDangerousFilters) {
  options_.SetRewriteLevel(RewriteOptions::kAllFilters);
  options_.EnableFilter(RewriteOptions::kStripScripts);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kStripScripts));
  options_.EnableFilter(RewriteOptions::kDivStructure);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
}

TEST_F(RewriteOptionsTest, CoreAndNotDangerous) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kAddInstrumentation));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
}

TEST_F(RewriteOptionsTest, CoreByNameNotLevel) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.EnableFiltersByCommaSeparatedList("core", &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // Test these for PlusAndMinus validation.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, PlusAndMinus) {
  static const char kList[] =
      "core,+div_structure, -inline_css,+extend_cache_css";
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  ASSERT_TRUE(options_.AdjustFiltersByCommaSeparatedList(kList, &handler));

  // Test the same ones as tested in InstrumentationDisabled.
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  ASSERT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));

  // These should be opposite from normal.
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDivStructure));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, SetDefaultRewriteLevel) {
  NullMessageHandler handler;
  RewriteOptions new_options(&thread_system_);
  new_options.SetDefaultRewriteLevel(RewriteOptions::kCoreFilters);

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  options_.Merge(new_options);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
}

void RewriteOptionsTest::TestSetOptionFromName(bool test_log_variant) {
  NullMessageHandler handler;

  // TODO(sriharis): Add tests for all Options here per LookupOptionByNameTest.

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "FetcherTimeOutMs",
              "1024",
              &handler);
  // Default for this is 5 * Timer::kSecondMs.
  EXPECT_EQ(1024, options_.blocking_fetch_timeout_ms());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CssInlineMaxBytes",
              "1024",
              &handler);
  // Default for this is 2048.
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "JpegRecompressionQuality",
              "1",
              &handler);
  // Default is -1.
  EXPECT_EQ(1, options_.ImageJpegQuality());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "CombineAcrossPaths",
              "false",
              &handler);
  // Default is true
  EXPECT_FALSE(options_.combine_across_paths());

  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "http://www.example.com/beacon2 https://www.example.com/beacon3",
              &handler);
  EXPECT_EQ("http://www.example.com/beacon2", options_.beacon_url().http);
  EXPECT_EQ("https://www.example.com/beacon3", options_.beacon_url().https);
  TestNameSet(RewriteOptions::kOptionOk,
              test_log_variant,
              "BeaconUrl",
              "/pagespeed_beacon?",
              &handler);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().http);
  EXPECT_EQ("/pagespeed_beacon?", options_.beacon_url().https);

  RewriteOptions::RewriteLevel old_level = options_.level();
  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "RewriteLevel",
              "does_not_work",
              &handler);
  EXPECT_EQ(old_level, options_.level());

  TestNameSet(RewriteOptions::kOptionNameUnknown,
              test_log_variant,
              "InvalidName",
              "example",
              &handler);

  TestNameSet(RewriteOptions::kOptionValueInvalid,
              test_log_variant,
              "JsInlineMaxBytes",
              "NOT_INT",
              &handler);
  EXPECT_EQ(RewriteOptions::kDefaultJsInlineMaxBytes,
            options_.js_inline_max_bytes());  // unchanged from default.
}

TEST_F(RewriteOptionsTest, SetOptionFromName) {
  TestSetOptionFromName(false);
}

TEST_F(RewriteOptionsTest, SetOptionFromNameAndLog) {
  TestSetOptionFromName(true);
}

// All the base option names are explicitly enumerated here. Modifications are
// handled by the explicit tests. Additions/deletions are handled by checking
// the count explicitly (and assuming we add/delete an option value when we
// add/delete an option name).
TEST_F(RewriteOptionsTest, LookupOptionByNameTest) {
  const char* const option_names[] = {
    RewriteOptions::kAcceptInvalidSignatures,
    RewriteOptions::kAccessControlAllowOrigins,
    RewriteOptions::kAddOptionsToUrls,
    RewriteOptions::kAllowLoggingUrlsInLogRecord,
    RewriteOptions::kAllowOptionsToBeSetByCookies,
    RewriteOptions::kAllowVaryOn,
    RewriteOptions::kAlwaysRewriteCss,
    RewriteOptions::kAmpLinkPattern,
    RewriteOptions::kAnalyticsID,
    RewriteOptions::kAvoidRenamingIntrospectiveJavascript,
    RewriteOptions::kAwaitPcacheLookup,
    RewriteOptions::kBeaconReinstrumentTimeSec,
    RewriteOptions::kBeaconUrl,
    RewriteOptions::kCacheFragment,
    RewriteOptions::kCacheSmallImagesUnrewritten,
    RewriteOptions::kClientDomainRewrite,
    RewriteOptions::kCombineAcrossPaths,
    RewriteOptions::kContentExperimentID,
    RewriteOptions::kContentExperimentVariantID,
    RewriteOptions::kCriticalImagesBeaconEnabled,
    RewriteOptions::kCssFlattenMaxBytes,
    RewriteOptions::kCssImageInlineMaxBytes,
    RewriteOptions::kCssInlineMaxBytes,
    RewriteOptions::kCssOutlineMinBytes,
    RewriteOptions::kCssPreserveURLs,
    RewriteOptions::kDefaultCacheHtml,
    RewriteOptions::kDisableBackgroundFetchesForBots,
    RewriteOptions::kDisableRewriteOnNoTransform,
    RewriteOptions::kDomainRewriteCookies,
    RewriteOptions::kDomainRewriteHyperlinks,
    RewriteOptions::kDomainShardCount,
    RewriteOptions::kDownstreamCachePurgeMethod,
    RewriteOptions::kDownstreamCacheRebeaconingKey,
    RewriteOptions::kDownstreamCacheRewrittenPercentageThreshold,
    RewriteOptions::kEnableAggressiveRewritersForMobile,
    RewriteOptions::kEnableCachePurge,
    RewriteOptions::kEnableDeferJsExperimental,
    RewriteOptions::kEnableExtendedInstrumentation,
    RewriteOptions::kEnableLazyLoadHighResImages,
    RewriteOptions::kEnablePrioritizingScripts,
    RewriteOptions::kEnabled,
    RewriteOptions::kEnrollExperiment,
    RewriteOptions::kExperimentCookieDurationMs,
    RewriteOptions::kExperimentSlot,
    RewriteOptions::kFetcherTimeOutMs,
    RewriteOptions::kFinderPropertiesCacheExpirationTimeMs,
    RewriteOptions::kFinderPropertiesCacheRefreshTimeMs,
    RewriteOptions::kFlushBufferLimitBytes,
    RewriteOptions::kFlushHtml,
    RewriteOptions::kFollowFlushes,
    RewriteOptions::kForbidAllDisabledFilters,
    RewriteOptions::kGoogleFontCssInlineMaxBytes,
    RewriteOptions::kHideRefererUsingMeta,
    RewriteOptions::kHttpCacheCompressionLevel,
    RewriteOptions::kHonorCsp,
    RewriteOptions::kIdleFlushTimeMs,
    RewriteOptions::kImageInlineMaxBytes,
    RewriteOptions::kImageJpegNumProgressiveScans,
    RewriteOptions::kImageJpegNumProgressiveScansForSmallScreens,
    RewriteOptions::kImageJpegQualityForSaveData,
    RewriteOptions::kImageJpegRecompressionQuality,
    RewriteOptions::kImageJpegRecompressionQualityForSmallScreens,
    RewriteOptions::kImageLimitOptimizedPercent,
    RewriteOptions::kImageLimitRenderedAreaPercent,
    RewriteOptions::kImageLimitResizeAreaPercent,
    RewriteOptions::kImageMaxRewritesAtOnce,
    RewriteOptions::kImagePreserveURLs,
    RewriteOptions::kImageRecompressionQuality,
    RewriteOptions::kImageResolutionLimitBytes,
    RewriteOptions::kImageWebpQualityForSaveData,
    RewriteOptions::kImageWebpRecompressionQuality,
    RewriteOptions::kImageWebpRecompressionQualityForSmallScreens,
    RewriteOptions::kImageWebpAnimatedRecompressionQuality,
    RewriteOptions::kImageWebpTimeoutMs,
    RewriteOptions::kImplicitCacheTtlMs,
    RewriteOptions::kIncreaseSpeedTracking,
    RewriteOptions::kInlineOnlyCriticalImages,
    RewriteOptions::kInlineResourcesWithoutExplicitAuthorization,
    RewriteOptions::kInPlacePreemptiveRewriteCss,
    RewriteOptions::kInPlacePreemptiveRewriteCssImages,
    RewriteOptions::kInPlacePreemptiveRewriteImages,
    RewriteOptions::kInPlacePreemptiveRewriteJavascript,
    RewriteOptions::kInPlaceResourceOptimization,
    RewriteOptions::kInPlaceRewriteDeadlineMs,
    RewriteOptions::kInPlaceSMaxAgeSec,
    RewriteOptions::kInPlaceWaitForOptimized,
    RewriteOptions::kJsInlineMaxBytes,
    RewriteOptions::kJsOutlineMinBytes,
    RewriteOptions::kJsPreserveURLs,
    RewriteOptions::kLazyloadImagesAfterOnload,
    RewriteOptions::kLazyloadImagesBlankUrl,
    RewriteOptions::kLoadFromFileCacheTtlMs,
    RewriteOptions::kLogBackgroundRewrite,
    RewriteOptions::kLogMobilizationSamples,
    RewriteOptions::kLogRewriteTiming,
    RewriteOptions::kLogUrlIndices,
    RewriteOptions::kLowercaseHtmlNames,
    RewriteOptions::kMaxCacheableResponseContentLength,
    RewriteOptions::kMaxCombinedCssBytes,
    RewriteOptions::kMaxCombinedJsBytes,
    RewriteOptions::kMaxHtmlCacheTimeMs,
    RewriteOptions::kMaxHtmlParseBytes,
    RewriteOptions::kMaxImageSizeLowResolutionBytes,
    RewriteOptions::kMaxInlinedPreviewImagesIndex,
    RewriteOptions::kMaxLowResImageSizeBytes,
    RewriteOptions::kMaxLowResToHighResImageSizePercentage,
    RewriteOptions::kMaxRewriteInfoLogSize,
    RewriteOptions::kMaxUrlSegmentSize,
    RewriteOptions::kMaxUrlSize,
    RewriteOptions::kMetadataCacheStalenessThresholdMs,
    RewriteOptions::kMinImageSizeLowResolutionBytes,
    RewriteOptions::kMinResourceCacheTimeToRewriteMs,
    RewriteOptions::kModifyCachingHeaders,
    RewriteOptions::kNoop,
    RewriteOptions::kNoTransformOptimizedImages,
    RewriteOptions::kNonCacheablesForCachePartialHtml,
    RewriteOptions::kObliviousPagespeedUrls,
    RewriteOptions::kOptionCookiesDurationMs,
    RewriteOptions::kOverrideCachingTtlMs,
    RewriteOptions::kPreserveSubresourceHints,
    RewriteOptions::kPreserveUrlRelativity,
    RewriteOptions::kPrivateNotVaryForIE,
    RewriteOptions::kProactiveResourceFreshening,
    RewriteOptions::kProactivelyFreshenUserFacingRequest,
    RewriteOptions::kProgressiveJpegMinBytes,
    RewriteOptions::kPubliclyCacheMismatchedHashesExperimental,
    RewriteOptions::kRejectBlacklisted,
    RewriteOptions::kRejectBlacklistedStatusCode,
    RewriteOptions::kRemoteConfigurationTimeoutMs,
    RewriteOptions::kRemoteConfigurationUrl,
    RewriteOptions::kReportUnloadTime,
    RewriteOptions::kRequestOptionOverride,
    RewriteOptions::kRespectVary,
    RewriteOptions::kRespectXForwardedProto,
    RewriteOptions::kResponsiveImageDensities,
    RewriteOptions::kRewriteDeadlineMs,
    RewriteOptions::kRewriteLevel,
    RewriteOptions::kRewriteRandomDropPercentage,
    RewriteOptions::kRewriteUncacheableResources,
    RewriteOptions::kRunningExperiment,
    RewriteOptions::kServeStaleIfFetchError,
    RewriteOptions::kServeStaleWhileRevalidateThresholdSec,
    RewriteOptions::kServeWebpToAnyAgent,
    RewriteOptions::kServeXhrAccessControlHeaders,
    RewriteOptions::kStickyQueryParameters,
    RewriteOptions::kSupportNoScriptEnabled,
    RewriteOptions::kTestOnlyPrioritizeCriticalCssDontApplyOriginalCss,
    RewriteOptions::kUrlSigningKey,
    RewriteOptions::kUseAnalyticsJs,
    RewriteOptions::kUseBlankImageForInlinePreview,
    RewriteOptions::kUseExperimentalJsMinifier,
    RewriteOptions::kUseFallbackPropertyCacheValues,
    RewriteOptions::kXModPagespeedHeaderValue,
    RewriteOptions::kXPsaBlockingRewrite,
  };

  // Check that every option can be looked up by name.
  std::set<StringPiece> tested_names;
  for (int i = 0; i < arraysize(option_names); ++i) {
    EXPECT_TRUE(NULL != RewriteOptions::LookupOptionByName(option_names[i]))
        << option_names[i] << " cannot be looked up by name!";
    EXPECT_FALSE(RewriteOptions::IsDeprecatedOptionName(option_names[i]))
        << option_names[i];
    tested_names.insert(option_names[i]);
  }

  // Now go through the named options in all_properties_ and check that each
  // one has been tested.
  int named_properties = 0;
  for (int i = 0, n = RewriteOptions::all_properties_->size(); i < n; ++i) {
    StringPiece name =
        RewriteOptions::all_properties_->property(i)->option_name();
    if (!name.empty()) {
      ++named_properties;
      EXPECT_NE(tested_names.end(), tested_names.find(name))
          << name << " has not been tested!";
    }
  }
  EXPECT_EQ(named_properties, tested_names.size());

  // Check that case doesn't matter when looking up directives.
  EXPECT_TRUE(NULL != RewriteOptions::LookupOptionByName("EnableRewriting"));
  EXPECT_TRUE(NULL != RewriteOptions::LookupOptionByName("eNaBlErEWrItIng"));
}

// All the non-base option names are explicitly enumerated here. Modifications
// are handled by the explicit tests. Additions/deletions are NOT handled.
TEST_F(RewriteOptionsTest, LookupNonBaseOptionByNameTest) {
  // Use macro so that the failure message tells us the name of the option
  // failing the test; using a function would obscure that.
#define FailLookupOptionByName(name) \
  EXPECT_TRUE(NULL == RewriteOptions::LookupOptionByName(name))

  // The following are not accessible by name, they are handled explicitly
  // by name comparison. We could/should test them all using their setters,
  // though -some- of them are (cf. ParseAndSetOptionFromName1/2/3 following).

  // Non-scalar options
  FailLookupOptionByName(RewriteOptions::kAllow);
  FailLookupOptionByName(RewriteOptions::kBlockingRewriteRefererUrls);
  FailLookupOptionByName(RewriteOptions::kDisableFilters);
  FailLookupOptionByName(RewriteOptions::kDisallow);
  FailLookupOptionByName(RewriteOptions::kDomain);
  FailLookupOptionByName(RewriteOptions::kDownstreamCachePurgeLocationPrefix);
  FailLookupOptionByName(RewriteOptions::kEnableFilters);
  FailLookupOptionByName(RewriteOptions::kExperimentVariable);
  FailLookupOptionByName(RewriteOptions::kExperimentSpec);
  FailLookupOptionByName(RewriteOptions::kForbidFilters);
  FailLookupOptionByName(RewriteOptions::kRetainComment);
  FailLookupOptionByName(RewriteOptions::kPermitIdsForCssCombining);

  // 2-arg options
  FailLookupOptionByName(RewriteOptions::kCustomFetchHeader);
  FailLookupOptionByName(RewriteOptions::kLoadFromFile);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileMatch);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileRule);
  FailLookupOptionByName(RewriteOptions::kLoadFromFileRuleMatch);
  FailLookupOptionByName(RewriteOptions::kMapOriginDomain);
  FailLookupOptionByName(RewriteOptions::kMapProxyDomain);
  FailLookupOptionByName(RewriteOptions::kMapRewriteDomain);
  FailLookupOptionByName(RewriteOptions::kShardDomain);

  // 3-arg options
  FailLookupOptionByName(RewriteOptions::kUrlValuedAttribute);
  FailLookupOptionByName(RewriteOptions::kLibrary);

  // system/ and apache/ options.
  FailLookupOptionByName(RewriteOptions::kCacheFlushFilename);
  FailLookupOptionByName(RewriteOptions::kCacheFlushPollIntervalSec);
  FailLookupOptionByName(RewriteOptions::kCompressMetadataCache);
  FailLookupOptionByName(RewriteOptions::kFetchHttps);
  FailLookupOptionByName(RewriteOptions::kFetcherProxy);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanIntervalMs);
  FailLookupOptionByName(RewriteOptions::kFileCachePath);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanSizeKb);
  FailLookupOptionByName(RewriteOptions::kFileCacheCleanInodeLimit);
  FailLookupOptionByName(RewriteOptions::kLogDir);
  FailLookupOptionByName(RewriteOptions::kLruCacheByteLimit);
  FailLookupOptionByName(RewriteOptions::kLruCacheKbPerProcess);
  FailLookupOptionByName(RewriteOptions::kMemcachedServers);
  FailLookupOptionByName(RewriteOptions::kMemcachedThreads);
  FailLookupOptionByName(RewriteOptions::kMemcachedTimeoutUs);
  FailLookupOptionByName(RewriteOptions::kRateLimitBackgroundFetches);
  FailLookupOptionByName(RewriteOptions::kUseSharedMemLocking);
  FailLookupOptionByName(RewriteOptions::kSlurpDirectory);
  FailLookupOptionByName(RewriteOptions::kSlurpFlushLimit);
  FailLookupOptionByName(RewriteOptions::kSlurpReadOnly);
  FailLookupOptionByName(RewriteOptions::kStatisticsEnabled);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingEnabled);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingChartsCSS);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingChartsJS);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingIntervalMs);
  FailLookupOptionByName(RewriteOptions::kStatisticsLoggingMaxFileSizeKb);
  FailLookupOptionByName(RewriteOptions::kTestProxy);
  FailLookupOptionByName(RewriteOptions::kTestProxySlurp);
}

TEST_F(RewriteOptionsTest, DeprecatedOptionsTest) {
  EXPECT_TRUE(RewriteOptions::IsDeprecatedOptionName("MaxPrefetchJsElements"));
  EXPECT_TRUE(RewriteOptions::IsDeprecatedOptionName("DistributeFetches"));
  EXPECT_TRUE(RewriteOptions::IsDeprecatedOptionName("DistributedRewriteKey"));
  EXPECT_TRUE(
      RewriteOptions::IsDeprecatedOptionName("DistributedRewriteServers"));
  EXPECT_TRUE(
      RewriteOptions::IsDeprecatedOptionName("DistributedRewriteTimeoutMs"));
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName1) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName1("arghh", "", &msg, &handler));

  // Simple scalar option.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1("JsInlineMaxBytes", "42",
                                                &msg, &handler));
  EXPECT_EQ(42, options_.js_inline_max_bytes());

  // Scalar with invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1("JsInlineMaxBytes", "one",
                                                &msg, &handler));
  EXPECT_EQ("Cannot set option JsInlineMaxBytes to one. ", msg);

  // Complex, valid value.
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                "EnableFilters", "debug,outline_css", &msg, &handler));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineCss));

  // Complex, invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                "EnableFilters", "no_such_filter", &msg, &handler));
  EXPECT_EQ("Failed to enable some filters.", msg);

  // Disallow/Allow.
  options_.Disallow("*");
  EXPECT_FALSE(options_.IsAllowed("example.com"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kAllow, "*.com", &msg, &handler));
  EXPECT_TRUE(options_.IsAllowed("example.com"));
  EXPECT_TRUE(options_.IsAllowed("evil.com"));
  EXPECT_FALSE(options_.IsAllowed("example.org"));

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisallow, "*evil*", &msg, &handler));
  EXPECT_TRUE(options_.IsAllowed("example.com"));
  EXPECT_FALSE(options_.IsAllowed("evil.com"));

  // Disable/forbid filters (enable covered above).
  options_.EnableFilter(RewriteOptions::kDebug);
  options_.EnableFilter(RewriteOptions::kOutlineCss);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisableFilters, "debug,outline_css",
                &msg, &handler));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDebug));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineCss));
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDisableFilters, "nosuch",
                &msg, &handler));
  EXPECT_EQ("Failed to disable some filters.", msg);

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kForbidFilters, "debug",
                &msg, &handler));
  EXPECT_FALSE(
      options_.Forbidden(options_.FilterId(RewriteOptions::kOutlineCss)));
  EXPECT_TRUE(
      options_.Forbidden(options_.FilterId(RewriteOptions::kDebug)));

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kForbidFilters, "nosuch",
                &msg, &handler));
  EXPECT_EQ("Failed to forbid some filters.", msg);

  // Domain.
  GoogleUrl main("http://example.com");
  GoogleUrl content("http://static.example.com");
  EXPECT_FALSE(options_.domain_lawyer()->IsDomainAuthorized(main, content));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDomain, "static.example.com",
                &msg, &handler));
  EXPECT_TRUE(options_.domain_lawyer()->IsDomainAuthorized(main, content)) <<
      options_.domain_lawyer()->ToString();

  // Downstream cache purge location prefix.
  // 1) Valid location.
  GoogleUrl valid_downstream_cache("http://caching-layer.example.com:8118");
  EXPECT_FALSE(options_.domain_lawyer()->IsOriginKnown(valid_downstream_cache));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDownstreamCachePurgeLocationPrefix,
                "http://caching-layer.example.com:8118/mypurgepath",
                &msg, &handler));
  EXPECT_TRUE(options_.domain_lawyer()->IsOriginKnown(valid_downstream_cache));
  EXPECT_EQ("http://caching-layer.example.com:8118/mypurgepath",
            options_.downstream_cache_purge_location_prefix());
  // 2) Invalid location.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kDownstreamCachePurgeLocationPrefix,
                "",
                &msg, &handler));
  EXPECT_EQ("Downstream cache purge location prefix is invalid.", msg);

  // Experiments.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentSpec,
                "id=2;enable=recompress_png;percent=50",
                &msg, &handler));
  RewriteOptions::ExperimentSpec* spec = options_.GetExperimentSpec(2);
  ASSERT_TRUE(spec != NULL);
  EXPECT_EQ(2, spec->id());
  EXPECT_EQ(50, spec->percent());
  EXPECT_EQ(1,  spec->enabled_filters().size());
  EXPECT_TRUE(
      spec->enabled_filters().IsSet(RewriteOptions::kRecompressPng));

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentSpec, "@)#@(#@(#@)((#)@",
                &msg, &handler));
  EXPECT_EQ("not a valid experiment spec", msg);

  EXPECT_NE(4, options_.experiment_ga_slot());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentVariable, "4", &msg, &handler));
  EXPECT_EQ(4, options_.experiment_ga_slot());

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kExperimentVariable, "10", &msg, &handler));
  EXPECT_EQ("must be an integer between 1 and 5", msg);

  // Retain comment.
  EXPECT_FALSE(options_.IsRetainedComment("important"));
  EXPECT_FALSE(options_.IsRetainedComment("silly"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kRetainComment, "*port*", &msg, &handler));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                RewriteOptions::kBlockingRewriteRefererUrls,
                "http://www.test.com/*", &msg, &handler));
  EXPECT_TRUE(options_.IsBlockingRewriteRefererUrlPatternPresent());
  EXPECT_TRUE(options_.IsBlockingRewriteEnabledForReferer(
      "http://www.test.com/"));
  EXPECT_FALSE(options_.IsBlockingRewriteEnabledForReferer(
      "http://www.testa.com/"));
  EXPECT_TRUE(options_.IsRetainedComment("important"));
  EXPECT_FALSE(options_.IsRetainedComment("silly"));
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName2) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName2("arghh", "", "",
                                                &msg, &handler));

  // Option mapped, but not a 2-argument.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName2("JsInlineMaxBytes", "", "",
                                                &msg, &handler));

  // Valid value.
  EXPECT_EQ(0, options_.num_custom_fetch_headers());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                "CustomFetchHeader", "header", "value", &msg, &handler));
  ASSERT_EQ(1, options_.num_custom_fetch_headers());
  EXPECT_EQ("header", options_.custom_fetch_header(0)->name);
  EXPECT_EQ("value", options_.custom_fetch_header(0)->value);

  // Invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                "LoadFromFileRule", "weird", "42", &msg, &handler));
  EXPECT_EQ("Argument 1 must be either 'Allow' or 'Disallow'", msg);

  // Various LoadFromFile options.
  GoogleString file_out;
  GoogleUrl url1("http://www.example.com/a.css");
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url1, &file_out));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFile, "http://www.example.com",
                "/example/", &msg, &handler));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url1, &file_out));
  EXPECT_EQ("/example/a.css", file_out);

  GoogleUrl url2("http://www.example.com/styles/b.css");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileMatch,
                "^http://www.example.com/styles/([^/]*)", "/style/\\1",
                &msg, &handler));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url2, &file_out));
  EXPECT_EQ("/style/b.css", file_out);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileMatch,
                "[a-", "/style/\\1",
                &msg, &handler));
  EXPECT_EQ("File mapping regular expression must match beginning of string. "
            "(Must start with '^'.)", msg);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRuleMatch,
                "Allow", "[a-",
                &msg, &handler));
  // Not testing the message since it's RE2-originated.

  GoogleUrl url3("http://www.example.com/images/a.png");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRule,
                "Disallow", "/example/images/",
                &msg, &handler));
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url3, &file_out));

  GoogleUrl url4("http://www.example.com/images/a.jpeg");
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName2(
                RewriteOptions::kLoadFromFileRuleMatch,
                "Allow", "\\.jpeg", &msg, &handler));
  EXPECT_FALSE(
      options_.file_load_policy()->ShouldLoadFromFile(url3, &file_out));
  EXPECT_TRUE(
      options_.file_load_policy()->ShouldLoadFromFile(url4, &file_out));
  EXPECT_EQ("/example/images/a.jpeg", file_out);

  // Domain lawyer options.
  scoped_ptr<RewriteOptions> options2(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options2->ParseAndSetOptionFromName2(
                RewriteOptions::kMapOriginDomain,
                "localhost/example", "www.example.com",
                &msg, &handler));
  EXPECT_EQ("http://localhost/example/\n"
            "http://www.example.com/ Auth "
                "OriginDomain:http://localhost/example/\n",
            options2->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options3(new RewriteOptions(&thread_system_));
  // This is an option 2 or 3, so test 2 here and 3 below.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options3->ParseAndSetOptionFromName3(
                RewriteOptions::kMapProxyDomain,
                "mainsite.com/static", "static.mainsite.com", "",
                &msg, &handler));
  EXPECT_EQ("http://mainsite.com/static/ Auth "
                "ProxyOriginDomain:http://static.mainsite.com/\n"
            "http://static.mainsite.com/ Auth "
                "ProxyDomain:http://mainsite.com/static/\n",
            options3->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options4(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options4->ParseAndSetOptionFromName2(
                RewriteOptions::kMapRewriteDomain,
                "cdn.example.com", "*example.com",
                &msg, &handler));
  EXPECT_EQ("http://*example.com/ Auth RewriteDomain:http://cdn.example.com/\n"
            "http://cdn.example.com/ Auth\n",
            options4->domain_lawyer()->ToString());

  scoped_ptr<RewriteOptions> options5(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options5->ParseAndSetOptionFromName2(
                RewriteOptions::kShardDomain,
                "https://www.example.com",
                "https://example1.cdn.com,https://example2.cdn.com",
                &msg, &handler));
  EXPECT_EQ("https://example1.cdn.com/ Auth "
                "RewriteDomain:https://www.example.com/\n"
            "https://example2.cdn.com/ Auth "
                "RewriteDomain:https://www.example.com/\n"
            "https://www.example.com/ Auth Shards:"
                "{https://example1.cdn.com/, "
                "https://example2.cdn.com/}\n",
            options5->domain_lawyer()->ToString());
}

TEST_F(RewriteOptionsTest, ParseAndSetOptionFromName3) {
  GoogleString msg;
  NullMessageHandler handler;

  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName3("arghh", "", "", "",
                                                &msg, &handler));

  // Option mapped, but not a 2-argument.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.ParseAndSetOptionFromName3("JsInlineMaxBytes", "", "", "",
                                                &msg, &handler));

  // Valid value.
  EXPECT_EQ(0, options_.num_url_valued_attributes());
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName3(
                "UrlValuedAttribute", "span", "src", "Hyperlink",
                &msg, &handler));
  ASSERT_EQ(1, options_.num_url_valued_attributes());
  StringPiece element, attribute;
  semantic_type::Category category;
  options_.UrlValuedAttribute(0, &element, &attribute, &category);
  EXPECT_EQ("span", element);
  EXPECT_EQ("src", attribute);
  EXPECT_EQ(semantic_type::kHyperlink, category);

  // Invalid value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName3(
                "UrlValuedAttribute", "span", "src", "nonsense",
                &msg, &handler));
  EXPECT_EQ("Invalid resource category: nonsense", msg);

  // Domain lawyer.
  scoped_ptr<RewriteOptions> options(new RewriteOptions(&thread_system_));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options->ParseAndSetOptionFromName3(
                RewriteOptions::kMapProxyDomain,
                "myproxy.com/static",
                "static.origin.com",
                "myproxy.cdn.com",
                &msg, &handler));
  EXPECT_EQ("http://myproxy.cdn.com/ Auth "
                "ProxyOriginDomain:http://static.origin.com/\n"
            "http://myproxy.com/static/ Auth "
                "RewriteDomain:http://myproxy.cdn.com/ "
                "ProxyOriginDomain:http://static.origin.com/\n"
            "http://static.origin.com/ Auth "
                "ProxyDomain:http://myproxy.cdn.com/\n",
            options->domain_lawyer()->ToString());

  options_.EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  GoogleString sig;
  options_.javascript_library_identification()->AppendSignature(&sig);
  EXPECT_EQ("", sig);
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName3(
                RewriteOptions::kLibrary, "43567", "5giEj_jl-Ag5G8",
                "http://www.example.com/url.js",
                &msg, &handler));
  sig.clear();
  options_.javascript_library_identification()->AppendSignature(&sig);
  EXPECT_EQ("S:43567_H:5giEj_jl-Ag5G8_J:http://www.example.com/url.js", sig);

  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.ParseAndSetOptionFromName3(
                RewriteOptions::kLibrary, "43567", "#@#)@(#@)",
                "http://www.example.com/url.js",
                &msg, &handler));
  EXPECT_EQ("Format is size md5 url; bad md5 #@#)@(#@) or "
            "URL http://www.example.com/url.js", msg);
}

TEST_F(RewriteOptionsTest, SetOptionFromQuery) {
  // Unknown option.
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.SetOptionFromQuery("arghh", ""));
  // Known option with a bad value.
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.SetOptionFromQuery(RewriteOptions::kCssFlattenMaxBytes,
                                        "nuh-uh"));
  // Known option with a good value.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.SetOptionFromQuery(RewriteOptions::kCssFlattenMaxBytes,
                                        "123"));
}

TEST_F(RewriteOptionsTest, ExperimentSpecTest) {
  // Test that we handle experiment specs properly, and that when we set the
  // options to one experiment or another, it works.
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  // Set the default slot to 4.
  options_.set_experiment_ga_slot(4);
  EXPECT_FALSE(options_.AddExperimentSpec("id=0", &handler));
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=7;percent=10;level=CoreFilters;enabled=sprite_images;"
      "disabled=inline_css;options=InlineJavascriptMaxBytes=600000", &handler));

  // Extra spaces to test whitespace handling.
  EXPECT_TRUE(options_.AddExperimentSpec("id=2;    percent=15;ga=UA-2222-1;"
                                         "disabled=insert_ga ;slot=3;",
                                         &handler));

  // Invalid slot - make sure the spec still gets added, and the slot defaults
  // to the global slot (4).
  EXPECT_TRUE(options_.AddExperimentSpec("id=17;percent=3;slot=8", &handler));

  options_.SetExperimentState(7);
  EXPECT_EQ(RewriteOptions::kCoreFilters, options_.level());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  // This experiment didn't have a ga_id, so make sure we still have the
  // global ga_id.
  EXPECT_EQ("UA-111111-1", options_.ga_id());
  EXPECT_EQ(4, options_.experiment_ga_slot());

  // insert_ga can not be disabled in any experiment because that filter injects
  // the instrumentation we use to collect the data.
  options_.SetExperimentState(2);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInsertGA));
  EXPECT_EQ(3, options_.experiment_ga_slot());
  // This experiment specified a ga_id, so make sure that we set it.
  EXPECT_EQ("UA-2222-1", options_.ga_id());

  options_.SetExperimentState(17);
  EXPECT_EQ(4, options_.experiment_ga_slot());

  options_.SetExperimentState(7);
  EXPECT_EQ("a", options_.GetExperimentStateStr());
  options_.SetExperimentState(2);
  EXPECT_EQ("b", options_.GetExperimentStateStr());
  options_.SetExperimentState(17);
  EXPECT_EQ("c", options_.GetExperimentStateStr());
  options_.SetExperimentState(experiment::kExperimentNotSet);
  EXPECT_EQ("", options_.GetExperimentStateStr());
  options_.SetExperimentState(experiment::kNoExperiment);
  EXPECT_EQ("", options_.GetExperimentStateStr());

  options_.SetExperimentStateStr("a");
  EXPECT_EQ("a", options_.GetExperimentStateStr());
  options_.SetExperimentStateStr("b");
  EXPECT_EQ("b", options_.GetExperimentStateStr());
  options_.SetExperimentStateStr("c");
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Invalid state index 'd'; we only added three specs above.
  options_.SetExperimentStateStr("d");
  // No effect on the experiment state; stay with 'c' from before.
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Check a state index that will be out of bounds in the other direction.
  options_.SetExperimentStateStr("`");
  // Still no effect on the experiment state.
  EXPECT_EQ("c", options_.GetExperimentStateStr());

  // Check that we have a maximum size of 26 concurrent experiment specs.
  // Get us up to 26.
  for (int i = options_.num_experiments(); i < 26 ; ++i) {
    int tmp_id = i+100;  // Don't want conflict with experiments added above.
    EXPECT_TRUE(options_.AddExperimentSpec(
        StrCat("id=", IntegerToString(tmp_id),
               ";percent=1;default"), &handler));
  }
  EXPECT_EQ(26, options_.num_experiments());
  // Object to adding a 27th.
  EXPECT_FALSE(options_.AddExperimentSpec("id=200;percent=1;default",
                                          &handler));
}

TEST_F(RewriteOptionsTest, DefaultExperimentSpecTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.EnableFilter(RewriteOptions::kStripScripts);
  options_.EnableFilter(RewriteOptions::kSpriteImages);
  options_.set_ga_id("UA-111111-1");
  // Check that we can combine 'default', 'enable' & 'disable', and 'options'.
  // strip_scripts was expressly enabled in addition to core and should stay on.
  // extend_cache_css is on because it's a core filter and should stay on.
  // defer_javascript is off by default but turned on by our spec.
  // local_storage_cache is off by default but turned on by our spec.
  // inline_css is on by default but turned off by our spec.
  // CssInlineMaxBytes is 1024 by default but set to 66 by our spec.
  options_.SetExperimentState(experiment::kNoExperiment);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kStripScripts));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLocalStorageCache));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_TRUE(options_.AddExperimentSpec(
      "id=18;percent=0;default"
      ";enable=defer_javascript,local_storage_cache"
      ";disable=inline_css,sprite_images"
      ";options=CssInlineMaxBytes=66", &handler));
  options_.SetExperimentState(18);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kStripScripts));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kLocalStorageCache));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
}

TEST_F(RewriteOptionsTest, PreserveURLDefaults) {
  // This test serves as a warning. If you enable preserve URLs by default then
  // many unit tests will fail due to filters being omitted from the HTML path.
  // Further, preserve_urls is not explicitly tested for the 'false' case, it is
  // assumed to be tested by the normal unit tests since the default value is
  // false.
  EXPECT_FALSE(options_.image_preserve_urls());
  EXPECT_FALSE(options_.css_preserve_urls());
  EXPECT_FALSE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, RewriteDeadlineTest) {
  EXPECT_EQ(RewriteOptions::kDefaultRewriteDeadlineMs,
            options_.rewrite_deadline_ms());
  options_.set_rewrite_deadline_ms(40);
  EXPECT_EQ(40, options_.rewrite_deadline_ms());
}

TEST_F(RewriteOptionsTest, ExperimentPrintTest) {
  NullMessageHandler handler;
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_ga_id("UA-111111-1");
  options_.set_running_experiment(true);
  EXPECT_FALSE(options_.AddExperimentSpec("id=2;enabled=rewrite_css;",
                                          &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=1;percent=15;default", &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=7;percent=15;level=AllFilters;",
                                         &handler));
  EXPECT_TRUE(options_.AddExperimentSpec("id=2;percent=15;enabled=rewrite_css;"
                                         "options=InlineCssMaxBytes=4096,"
                                         "InlineJsMaxBytes=4;"
                                         "ga_id=122333-4", &handler));
  options_.SetExperimentState(-7);
  // No experiment changes.
  EXPECT_EQ("", options_.ToExperimentDebugString());
  EXPECT_EQ("", options_.ToExperimentString());
  options_.SetExperimentState(1);
  EXPECT_EQ("Experiment: 1; id=1;ga=UA-111111-1;percent=15;default",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 1", options_.ToExperimentString());
  options_.SetExperimentState(7);
  EXPECT_EQ("Experiment: 7", options_.ToExperimentString());
  options_.SetExperimentState(2);
  // Note the options= section.
  EXPECT_EQ("Experiment: 2; id=2;ga=122333-4;percent=15;enabled=cf;"
            "options=InlineCssMaxBytes=4096,InlineJsMaxBytes=4",
            options_.ToExperimentDebugString());
  EXPECT_EQ("Experiment: 2", options_.ToExperimentString());

  // Make sure we set the ga_id to the one specified by spec 2.
  EXPECT_EQ("122333-4", options_.ga_id());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestDefaultUnchanged) {
  SetupTestExperimentSpecs();
  // Default for this is 2048.
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestCssInlineChange) {
  SetupTestExperimentSpecs();
  options_.SetExperimentState(1);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestCssInlineChangeToDefault) {
  SetupTestExperimentSpecs();
  options_.SetExperimentState(3);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestCssInlineChangeToInvalid) {
  SetupTestExperimentSpecs();
  options_.SetExperimentState(4);
  EXPECT_EQ(2048L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestCssInlineWithIllegalOptions) {
  SetupTestExperimentSpecs();
  options_.SetExperimentState(5);
  EXPECT_EQ(1024L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestMultipleOptions) {
  SetupTestExperimentSpecs();
  options_.SetExperimentState(6);
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
  EXPECT_EQ(123L, options_.js_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionsTestToString) {
  SetupTestExperimentSpecs();

  // Just compare the experiments, not the rest of the OptionsToString output.
  GoogleString options_string = options_.OptionsToString();
  StringPieceVector lines;
  StringPieceVector experiments;;
  SplitStringPieceToVector(options_string, "\n", &lines, true);
  for (int i = 0, n = lines.size(); i < n; ++i) {
    if (lines[i].starts_with("Experiment ")) {
      experiments.push_back(lines[i]);
    }
  }
  EXPECT_STREQ("Experiment id=1;percent=15;enabled=dj;"
               "options=CssInlineMaxBytes=1024",
               experiments[0]);
  EXPECT_STREQ("Experiment id=2;percent=15;enabled=ri;"
               "options=BogusOption=35",
               experiments[1]);
  EXPECT_STREQ("Experiment id=3;percent=15;enabled=dj",
               experiments[2]);
  EXPECT_STREQ("Experiment id=4;percent=15;enabled=dj;"
               "options=CssInlineMaxBytes=Cabbage",
               experiments[3]);
  EXPECT_STREQ("Experiment id=5;percent=15;enabled=dj;"
               "options=5=10,"
               "6=9,"
               "CssInlineMaxBytes=1024,"
               "Potato=Carrot",
               experiments[4]);
  EXPECT_STREQ("Experiment id=6;percent=15;enabled=dj;"
               "options=CssInlineMaxBytes=100,"
               "JpegRecompresssionQuality=50,"
               "JsInlineMaxBytes=123,"
               "JsOutlineMinBytes=4096",
               experiments[5]);
}

TEST_F(RewriteOptionsTest, ExperimentMergeTest) {
  NullMessageHandler handler;
  RewriteOptions::ExperimentSpec *spec = new
      RewriteOptions::ExperimentSpec("id=1;percentage=15;"
                                     "enable=defer_javascript;"
                                     "options=CssInlineMaxBytes=100",
                                     &options_, &handler);

  RewriteOptions::ExperimentSpec *spec2 = new
      RewriteOptions::ExperimentSpec("id=2;percentage=25;enable=resize_images;"
                                     "options=CssInlineMaxBytes=125", &options_,
                                     &handler);
  options_.InsertExperimentSpecInVector(spec);
  options_.InsertExperimentSpecInVector(spec2);
  options_.SetExperimentState(1);
  EXPECT_EQ(15, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
  spec->Merge(*spec2);
  options_.SetExperimentState(1);
  EXPECT_EQ(25, spec->percent());
  EXPECT_EQ(1, spec->id());
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_EQ(125L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentOptionLifetimeTest) {
  NullMessageHandler handler;
  // This allocates a character array on the stack and initializes it with the
  // specified string including a null terminator.  The size of the array is
  // taken from the length of the string.  The array is ours to modify.
  char str_spec[] = ("id=1;percentage=15;"
                     "enable=defer_javascript;"
                     "options=CssInlineMaxBytes=100");
  EXPECT_TRUE(options_.AddExperimentSpec(str_spec, &handler));
  // ExperimentSpec must not keep any references into str_spec because it's
  // not guaranteed to stick around or stay constant.  We modify str_spec to
  // make sure ExperimentSpec hasn't kept a reference.
  str_spec[sizeof(str_spec) - 2] = '9';
  options_.SetExperimentState(1);
  // If ExperimentSpec just kept pointers into str_spec then we'll get 109 here.
  EXPECT_EQ(100L, options_.css_inline_max_bytes());
}

TEST_F(RewriteOptionsTest, ExperimentDeviceTypeParseTest) {
  NullMessageHandler handler;

  {
    GoogleString spec_str("id=1;percent=15;"
                          "matches_device_type=desktop");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kDesktop));
    EXPECT_FALSE(spec.matches_device_type(UserAgentMatcher::kTablet));
    EXPECT_FALSE(spec.matches_device_type(UserAgentMatcher::kMobile));
  }

  {
    GoogleString spec_str("id=1;percent=15;"
                          "matches_device_type=tablet,mobile");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    EXPECT_FALSE(spec.matches_device_type(UserAgentMatcher::kDesktop));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kTablet));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kMobile));
  }

  {
    GoogleString spec_str("id=1;percent=15;"
                          "matches_device_type=desktop,tablet,mobile");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kDesktop));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kTablet));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kMobile));
  }

  {
    GoogleString spec_str("id=1;percent=15");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kDesktop));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kTablet));
    EXPECT_TRUE(spec.matches_device_type(UserAgentMatcher::kMobile));
  }
}

TEST_F(RewriteOptionsTest, ExperimentDeviceTypeRangeUnderflowDeathTest) {
  RewriteOptions::ExperimentSpec spec(1);

  UserAgentMatcher::DeviceType device_type(
      static_cast<UserAgentMatcher::DeviceType>(-1));

#ifdef NDEBUG
  EXPECT_FALSE(spec.matches_device_type(device_type));
#else
  EXPECT_DEATH(spec.matches_device_type(device_type),
               "DeviceType out of range:");
#endif
}

TEST_F(RewriteOptionsTest, ExperimentDeviceTypeRangeOverflowDeathTest) {
  RewriteOptions::ExperimentSpec spec(1);

  UserAgentMatcher::DeviceType device_type(UserAgentMatcher::kEndOfDeviceType);

#ifdef NDEBUG
  EXPECT_FALSE(spec.matches_device_type(device_type));
#else
  EXPECT_DEATH(spec.matches_device_type(device_type),
               "DeviceType out of range:");
#endif
}

TEST_F(RewriteOptionsTest, DeviceTypeMergeTest) {
  NullMessageHandler handler;
  {
    // From a spec with a device_type to one without.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15;matches_device_type=mobile",
        &options_, &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30",
        &options_, &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;matches_device_type=mobile",
              spec2.ToString());
  }
  {
    // From a spec without a device_type to one with.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15",
        &options_, &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30;matches_device_type=mobile",
        &options_, &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;matches_device_type=mobile",
              spec2.ToString());
  }
  {
    // Two specs, both with a device_type.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15;matches_device_type=tablet",
        &options_, &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30;matches_device_type=desktop",
        &options_, &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;matches_device_type=tablet",
              spec2.ToString());
  }
  {
    // Neither spec has a device type.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15",
        &options_, &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30",
        &options_, &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15", spec2.ToString());
  }
}

TEST_F(RewriteOptionsTest, AlternateOriginDomainMergeTest) {
  GoogleMessageHandler handler;
  {
    // From a spec with an alternate_origin_domain to one without.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15;alternate_origin_domain=foo.com:bar.com", &options_,
        &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30",
        &options_, &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;alternate_origin_domain=foo.com:bar.com",
              spec2.ToString());
  }
  {
    // From a spec without an alternate_origin_domain to one with.
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15",
        &options_, &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30;alternate_origin_domain=foo.com:bar.com", &options_,
        &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;alternate_origin_domain=foo.com:bar.com",
              spec2.ToString());
  }
  {
    // Two specs, both with alternate_origin_domains
    RewriteOptions::ExperimentSpec spec1(
        "id=1;percent=15;alternate_origin_domain=foo.com:bar.com", &options_,
        &handler);

    RewriteOptions::ExperimentSpec spec2(
        "id=2;percent=30;alternate_origin_domain=baz.com:qux.com", &options_,
        &handler);

    spec2.Merge(spec1);

    EXPECT_EQ("id=2;percent=15;alternate_origin_domain=foo.com:bar.com",
              spec2.ToString());
  }
}

TEST_F(RewriteOptionsTest, AlternateOriginDomainParseTest) {
  GoogleMessageHandler handler;
  {
    // Single domain, no host header.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=example.com:ref.example.com");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://example.com", "http://ref.example.com/",
                    "example.com", false);
    VerifyMapOrigin(lawyer, "https://example.com", "https://ref.example.com/",
                    "example.com", false);
  }
  {
    // Single domain, port, no host header.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=example.com:\"ref.example.com:99\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://example.com",
                    "http://ref.example.com:99/", "example.com", false);
    VerifyMapOrigin(lawyer, "https://example.com",
                    "https://ref.example.com:99/", "example.com", false);
  }
  {
    // Single domain with host header.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=example.com:ref.example.com:exh.com");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://example.com", "http://ref.example.com/",
                    "exh.com", false);
    VerifyMapOrigin(lawyer, "https://example.com", "https://ref.example.com/",
                    "exh.com", false);
  }
  {
    // Single domain with host header and port on both.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=ex.com:\"ref.ex.com:88\":\"exh.com:42\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://ex.com", "http://ref.ex.com:88/",
                    "exh.com:42", false);
    VerifyMapOrigin(lawyer, "https://ex.com", "https://ref.ex.com:88/",
                    "exh.com:42", false);
  }
  {
    // Single domain with port and host header and port on both.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain="
        "\"ex.com:63\":\"ref.ex.com:88\":\"exh.com:42\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://ex.com", "http://ex.com/",
                    "ex.com", false);
    VerifyMapOrigin(lawyer, "http://ex.com:63", "http://ref.ex.com:88/",
                    "exh.com:42", false);
    VerifyMapOrigin(lawyer, "https://ex.com:63", "https://ref.ex.com:88/",
                    "exh.com:42", false);
  }
  {
    // Multiple domains with a host header
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=foo.com,bar.com:ref.com:host.com");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://foo.com", "http://ref.com/", "host.com",
                    false);
    VerifyMapOrigin(lawyer, "https://foo.com", "https://ref.com/", "host.com",
                    false);
    VerifyMapOrigin(lawyer, "http://bar.com", "http://ref.com/", "host.com",
                    false);
    VerifyMapOrigin(lawyer, "https://bar.com", "https://ref.com/", "host.com",
                    false);
  }
}

TEST_F(RewriteOptionsTest, AlternateOriginDomainNegativeParseTest) {
  GoogleMessageHandler handler;
  {
    // Empty alternate_origin_domain spec.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());
  }
  {
    // Missing origin domain.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=bad.com");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://bad.com");
    VerifyNoMapOrigin(lawyer, "https://bad.com");
  }
  {
    // Trailing colon with missing origin domain.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=baz.com:");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://baz.com");
    VerifyNoMapOrigin(lawyer, "https://baz.com");
  }
  {
    // Unqoted port
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=baz.com:456");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://baz.com");
    VerifyNoMapOrigin(lawyer, "https://baz.com");
  }
  {
    // Trailing comma in serving domain.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=joe.com,:ref.com");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15;alternate_origin_domain=joe.com:ref.com",
              spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://joe.com", "http://ref.com/", "joe.com",
                    false);
    VerifyMapOrigin(lawyer, "https://joe.com", "https://ref.com/", "joe.com",
                    false);
  }
  {
    // Trailing colon for empty host header.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=jim.com:ref.com");
    GoogleString spec_str_plus_colon = spec_str + ":";

    RewriteOptions::ExperimentSpec spec(spec_str_plus_colon, &options_,
                                        &handler);

    EXPECT_EQ(spec_str, spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyMapOrigin(lawyer, "http://jim.com", "http://ref.com/", "jim.com",
                    false);
    VerifyMapOrigin(lawyer, "https://jim.com", "https://ref.com/", "jim.com",
                    false);
  }
  {
    // Non numeric serving domain port.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=\"jim.com:a\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://jim.com");
    VerifyNoMapOrigin(lawyer, "https://jim.com");
  }
  {
    // Non numeric reference domain port.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=jim.com:\"jam.com:a\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://jim.com");
    VerifyNoMapOrigin(lawyer, "https://jim.com");
  }
  {
    // Non numeric host header port.
    GoogleString spec_str(
        "id=1;percent=15;"
        "alternate_origin_domain=jim.com:jam.com:\"jom.com:g\"");

    RewriteOptions::ExperimentSpec spec(spec_str, &options_, &handler);

    EXPECT_EQ("id=1;percent=15", spec.ToString());

    DomainLawyer lawyer;
    spec.ApplyAlternateOriginsToDomainLawyer(&lawyer, &handler);

    VerifyNoMapOrigin(lawyer, "http://jim.com");
    VerifyNoMapOrigin(lawyer, "https://jim.com");
  }
}

TEST_F(RewriteOptionsTest, SetOptionsFromName) {
  TestMessageHandler handler;
  RewriteOptions::OptionSet option_set;
  option_set.insert(RewriteOptions::OptionStringPair(
      "CssInlineMaxBytes", "1024"));
  EXPECT_TRUE(options_.SetOptionsFromName(option_set, &handler));
  EXPECT_TRUE(handler.messages().empty());
  option_set.insert(RewriteOptions::OptionStringPair(
      "Not an Option", "nothing"));
  EXPECT_FALSE(options_.SetOptionsFromName(option_set, &handler));
  EXPECT_FALSE(handler.messages().empty());
}

// TODO(sriharis):  Add thorough ComputeSignature tests

TEST_F(RewriteOptionsTest, ComputeSignatureWildcardGroup) {
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  // Tweak allow_resources_ and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.Disallow("http://www.example.com/*");
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);
  // Tweak retain_comments and check that signature changes.
  options_.ClearSignatureForTesting();
  options_.RetainComment("TEST");
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature1, signature3);
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, ComputeSignatureOptionEffect) {
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(2048);
  options_.set_in_place_rewriting_enabled(false);
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();

  // Changing an Option used in signature computation will change the signature.
  options_.ClearSignatureForTesting();
  options_.set_css_image_inline_max_bytes(1024);
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_NE(signature1, signature2);

  // Changing an Option not used in signature computation will not change the
  // signature.
  options_.ClearSignatureForTesting();
  options_.set_in_place_rewriting_enabled(true);
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();

  // See the comment in RewriteOptions::RewriteOptions -- we need to leave
  // signatures sensitive to ajax_rewriting.
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, SignatureIgnoresDebug) {
  options_.ClearSignatureForTesting();
  options_.EnableFilter(RewriteOptions::kCombineCss);
  options_.ComputeSignature();
  scoped_ptr<RewriteOptions> options2(options_.Clone());
  options2->ClearSignatureForTesting();
  options2->EnableFilter(RewriteOptions::kDebug);
  options2->ComputeSignature();
  EXPECT_STREQ(options_.signature(), options2->signature());
  EXPECT_FALSE(options_.IsEqual(*options2));
}

TEST_F(RewriteOptionsTest, IsEqual) {
  RewriteOptions a(&thread_system_), b(&thread_system_);
  a.ComputeSignature();
  b.ComputeSignature();
  EXPECT_TRUE(a.IsEqual(b));
  a.ClearSignatureForTesting();
  a.EnableFilter(RewriteOptions::kSpriteImages);
  a.ComputeSignature();
  EXPECT_FALSE(a.IsEqual(b));
  b.ClearSignatureForTesting();
  b.EnableFilter(RewriteOptions::kSpriteImages);
  b.ComputeSignature();
  EXPECT_TRUE(a.IsEqual(b));
}

TEST_F(RewriteOptionsTest, ComputeSignatureEmptyIdempotent) {
  options_.ClearSignatureForTesting();
  options_.DisallowTroublesomeResources();
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  options_.ClearSignatureForTesting();

  // Merging in empty RewriteOptions should not change the signature.
  RewriteOptions options2(&thread_system_);
  options_.Merge(options2);
  options_.ComputeSignature();
  EXPECT_EQ(signature1, options_.signature());
}

TEST_F(RewriteOptionsTest, ImageOptimizableCheck) {
  options_.ClearFilters();
  options_.EnableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kRecompressWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertGifToPng);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertJpegToWebp);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertPngToJpeg);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertToWebpLossless);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertToWebpLossless);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());

  options_.EnableFilter(RewriteOptions::kConvertToWebpAnimated);
  EXPECT_TRUE(options_.ImageOptimizationEnabled());
  options_.DisableFilter(RewriteOptions::kConvertToWebpAnimated);
  EXPECT_FALSE(options_.ImageOptimizationEnabled());
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationTest) {
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.AddUrlCacheInvalidationEntry("two*", 25L, false);
  options_.AddUrlCacheInvalidationEntry("four", 40L, false);
  options_.AddUrlCacheInvalidationEntry("five", 50L, false);
  options_.AddUrlCacheInvalidationEntry("six", 60L, false);
  RewriteOptions options1(&thread_system_);
  options1.AddUrlCacheInvalidationEntry("one*", 20L, true);
  options1.AddUrlCacheInvalidationEntry("three*", 23L, false);
  options1.AddUrlCacheInvalidationEntry("three*", 30L, true);
  options1.AddUrlCacheInvalidationEntry("four", 39L, false);
  options1.AddUrlCacheInvalidationEntry("five", 51L, false);
  options1.AddUrlCacheInvalidationEntry("seven", 70L, false);
  options_.Merge(options1);
  EXPECT_TRUE(options_.IsUrlCacheInvalidationEntriesSorted());
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 9L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("one1", 19L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("one1", 21L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("two2", 21L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("two2", 26L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("three3", 31L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("four", 40L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("four", 41L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("five", 51L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("five", 52L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("six", 60L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("six", 61L, true));
  EXPECT_FALSE(options_.IsUrlCacheValid("seven", 70L, true));
  EXPECT_TRUE(options_.IsUrlCacheValid("seven", 71L, true));
}

TEST_F(RewriteOptionsTest, UrlCacheInvalidationSignatureTest) {
  options_.ComputeSignature();
  GoogleString signature1 = options_.signature();
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("one*", 10L, true);
  options_.ComputeSignature();
  GoogleString signature2 = options_.signature();
  EXPECT_EQ(signature1, signature2);
  options_.ClearSignatureForTesting();
  options_.AddUrlCacheInvalidationEntry("two*", 10L, false);
  options_.ComputeSignature();
  GoogleString signature3 = options_.signature();
  EXPECT_NE(signature2, signature3);
}

TEST_F(RewriteOptionsTest, EnabledFiltersRequiringJavaScriptTest) {
  RewriteOptions foo(&thread_system_);
  foo.ClearFilters();
  foo.EnableFilter(RewriteOptions::kDeferJavascript);
  foo.EnableFilter(RewriteOptions::kResizeImages);
  RewriteOptions::FilterVector foo_fs;
  foo.GetEnabledFiltersRequiringScriptExecution(&foo_fs);
  EXPECT_FALSE(foo_fs.empty());
  EXPECT_EQ(1, foo_fs.size());

  RewriteOptions bar(&thread_system_);
  bar.ClearFilters();
  bar.EnableFilter(RewriteOptions::kResizeImages);
  bar.EnableFilter(RewriteOptions::kConvertPngToJpeg);
  RewriteOptions::FilterVector bar_fs;
  bar.GetEnabledFiltersRequiringScriptExecution(&bar_fs);
  EXPECT_TRUE(bar_fs.empty());
}

TEST_F(RewriteOptionsTest, FilterLookupMethods) {
  EXPECT_STREQ("Add Head",
               RewriteOptions::FilterName(RewriteOptions::kAddHead));
  EXPECT_STREQ("Remove Comments",
               RewriteOptions::FilterName(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("End of Filters",
  //              RewriteOptions::FilterName(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("Unknown Filter",
  //              RewriteOptions::FilterName(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_STREQ("ah",
               RewriteOptions::FilterId(RewriteOptions::kAddHead));
  EXPECT_STREQ("rc",
               RewriteOptions::FilterId(RewriteOptions::kRemoveComments));
  // Can't do these unless we remove the LOG(DFATAL) from FilterName().
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(RewriteOptions::kEndOfFilters));
  // EXPECT_STREQ("UF",
  //              RewriteOptions::FilterId(
  //                  static_cast<RewriteOptions::Filter>(-1)));

  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("  "));
  EXPECT_EQ(RewriteOptions::kAddHead,
            RewriteOptions::LookupFilterById("ah"));
  EXPECT_EQ(RewriteOptions::kRemoveComments,
            RewriteOptions::LookupFilterById("rc"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("zz"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("UF"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById("junk"));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(""));
  EXPECT_EQ(RewriteOptions::kEndOfFilters,
            RewriteOptions::LookupFilterById(StringPiece()));

  EXPECT_EQ(RewriteOptions::kAnalyticsID,
            RewriteOptions::LookupOptionNameById("ig"));
  EXPECT_EQ(RewriteOptions::kImageJpegRecompressionQuality,
            RewriteOptions::LookupOptionNameById("iq"));
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("  ").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("junk").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById("").empty());
  EXPECT_TRUE(RewriteOptions::LookupOptionNameById(StringPiece()).empty());
}

TEST_F(RewriteOptionsTest, ParseBeaconUrl) {
  RewriteOptions::BeaconUrl beacon_url;
  GoogleString url = "www.example.com";
  GoogleString url2 = "www.example.net";

  EXPECT_FALSE(RewriteOptions::ParseBeaconUrl("", &beacon_url));
  EXPECT_FALSE(RewriteOptions::ParseBeaconUrl("a b c", &beacon_url));

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("http://" + url, &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url, beacon_url.https);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("https://" + url, &beacon_url));
  EXPECT_STREQ("https://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url, beacon_url.https);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl(
      "http://" + url + " " + "https://" + url2, &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url2, beacon_url.https);

  // Verify that ets parameters get stripped from the beacon_url
  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("http://" + url + "?ets=" + " " +
                                             "https://"+ url2 + "?foo=bar&ets=",
                                             &beacon_url));
  EXPECT_STREQ("http://" + url, beacon_url.http);
  EXPECT_STREQ("https://" + url2 + "?foo=bar", beacon_url.https);
  EXPECT_STREQ("http://" + url, beacon_url.http_in);
  EXPECT_STREQ("https://" + url2, beacon_url.https_in);

  EXPECT_TRUE(RewriteOptions::ParseBeaconUrl("/mod_pagespeed_beacon?a=b",
                                             &beacon_url));
  EXPECT_STREQ("/mod_pagespeed_beacon?a=b", beacon_url.http);
  EXPECT_STREQ("/mod_pagespeed_beacon?a=b", beacon_url.https);
  EXPECT_STREQ("/mod_pagespeed_beacon", beacon_url.http_in);
  EXPECT_STREQ("/mod_pagespeed_beacon", beacon_url.https_in);
}

TEST_F(RewriteOptionsTest, AccessOptionByIdAndName) {
  const char* id = NULL;
  GoogleString value;
  bool was_set = false;
  EXPECT_TRUE(options_.OptionValue(
      RewriteOptions::kImageJpegRecompressionQuality, &id, &was_set, &value));
  EXPECT_FALSE(was_set);
  EXPECT_STREQ("iq", id);
  const StringPiece kBogusOptionName("bogosity!");
  EXPECT_EQ(RewriteOptions::kOptionNameUnknown,
            options_.SetOptionFromName(kBogusOptionName, ""));
  EXPECT_EQ(RewriteOptions::kOptionValueInvalid,
            options_.SetOptionFromName(
                RewriteOptions::kImageJpegRecompressionQuality, "garbage"));
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.SetOptionFromName(
                RewriteOptions::kImageJpegRecompressionQuality, "63"));
  id = NULL;
  EXPECT_TRUE(options_.OptionValue(
      RewriteOptions::kImageJpegRecompressionQuality, &id, &was_set, &value));
  EXPECT_TRUE(was_set);
  EXPECT_STREQ("iq", id);
  EXPECT_STREQ("63", value);

  EXPECT_FALSE(options_.OptionValue(kBogusOptionName, &id, &was_set, &value));
}

TEST_F(RewriteOptionsTest, AccessAcrossThreads) {
#ifndef NDEBUG  // Depends on bits set in rewrite_options.cc under debug
  NullThreadSystem null_thread_system;

  null_thread_system.set_current_thread(5);

  RewriteOptions options(&null_thread_system);
  // We can continue to modify in the same thread.
  EXPECT_TRUE(options.ModificationOK());

  // Unmodified, we could switch to a different thread.
  null_thread_system.set_current_thread(6);
  EXPECT_TRUE(options.ModificationOK());
  null_thread_system.set_current_thread(5);

  // Now make a modification.  We can continue to modify in the same thread.
  options.set_enabled(RewriteOptions::kEnabledStandby);
  EXPECT_TRUE(options.ModificationOK());

  // But from a different thread we must not modify.
  null_thread_system.set_current_thread(4);
  EXPECT_FALSE(options.ModificationOK());

  // Back in thread 5 we can modify.
  null_thread_system.set_current_thread(5);
  EXPECT_TRUE(options.ModificationOK());

  // We can merge from the same thread, but not from a different one.
  EXPECT_TRUE(options.MergeOK());
  null_thread_system.set_current_thread(4);
  EXPECT_FALSE(options.MergeOK());

  // Clearing the signature gets us on a clean slate and we can take over
  // from thread 4.
  options.ClearSignatureWithCaution();
  EXPECT_TRUE(options.MergeOK());

  // Once we freeze it we can merge from it.
  options.Freeze();
  EXPECT_TRUE(options.MergeOK());
  null_thread_system.set_current_thread(5);
  EXPECT_TRUE(options.MergeOK());
#endif
}

TEST_F(RewriteOptionsTest, ParseAndSetDeprecatedOptionFromName1) {
  GoogleString msg;
  NullMessageHandler handler;

  // 'ImageWebpRecompressionQuality' is replaced by 'WebpRecompressionQuality'.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1("ImageWebpRecompressionQuality",
                                                "12", &msg, &handler));
  EXPECT_EQ(12, options_.ImageWebpQuality());

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1("WebpRecompressionQuality",
                                                "23", &msg, &handler));
  EXPECT_EQ(23, options_.ImageWebpQuality());

  // 'ImageWebpRecompressionQualityForSmallScreens' is replaced by
  // 'WebpRecompressionQualityForSmallScreens'.
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                "ImageWebpRecompressionQualityForSmallScreens",
                "34", &msg, &handler));
  EXPECT_EQ(34, options_.ImageWebpQualityForSmallScreen());

  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.ParseAndSetOptionFromName1(
                "WebpRecompressionQualityForSmallScreens",
                "45", &msg, &handler));
  EXPECT_EQ(45, options_.ImageWebpQualityForSmallScreen());
}

TEST_F(RewriteOptionsTest, BandwidthMode) {
  scoped_ptr<RewriteOptions> vhost_options(new RewriteOptions(&thread_system_));
  vhost_options->SetRewriteLevel(RewriteOptions::kOptimizeForBandwidth);
  EXPECT_FALSE(vhost_options->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kConvertGifToPng));
  EXPECT_TRUE(vhost_options->Enabled(
      RewriteOptions::kConvertJpegToProgressive));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kConvertJpegToWebp));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kConvertPngToJpeg));
  EXPECT_TRUE(vhost_options->Enabled(
      RewriteOptions::kInPlaceOptimizeForBrowser));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kJpegSubsampling));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kRecompressPng));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kRecompressWebp));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(vhost_options->Enabled(
      RewriteOptions::kRewriteJavascriptExternal));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kRewriteJavascriptInline));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kStripImageColorProfile));
  EXPECT_TRUE(vhost_options->Enabled(RewriteOptions::kStripImageMetaData));
  EXPECT_TRUE(vhost_options->Enabled(
      RewriteOptions::kInPlaceOptimizeForBrowser));
  EXPECT_TRUE(vhost_options->in_place_rewriting_enabled());
  EXPECT_TRUE(vhost_options->css_preserve_urls());
  EXPECT_TRUE(vhost_options->image_preserve_urls());
  EXPECT_TRUE(vhost_options->js_preserve_urls());

  // We use preemptive rewrites so that there's a chance that a first or
  // second view will yield optimized resources.
  EXPECT_TRUE(vhost_options->in_place_preemptive_rewrite_css());
  EXPECT_TRUE(vhost_options->in_place_preemptive_rewrite_css_images());
  EXPECT_TRUE(vhost_options->in_place_preemptive_rewrite_images());
  EXPECT_TRUE(vhost_options->in_place_preemptive_rewrite_javascript());

  // Now override a bandwidth-option.  Let's say it's OK to mutate
  // CSS urls.
  vhost_options->set_css_preserve_urls(false);
  EXPECT_FALSE(vhost_options->css_preserve_urls());

  // JS and Image URLs must still be preserved.
  EXPECT_TRUE(vhost_options->image_preserve_urls());
  EXPECT_TRUE(vhost_options->js_preserve_urls());

  // Now merge with an options-set with Core enabled many of these answers
  // change.
  scoped_ptr<RewriteOptions> core(new RewriteOptions(&thread_system_));
  scoped_ptr<RewriteOptions> vhost_core(new RewriteOptions(&thread_system_));
  core->SetRewriteLevel(RewriteOptions::kCoreFilters);

  vhost_core->Merge(*vhost_options);
  vhost_core->Merge(*core);

  EXPECT_TRUE(vhost_core->Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(vhost_core->Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_TRUE(vhost_core->Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(vhost_core->Enabled(RewriteOptions::kRewriteJavascriptExternal));
  EXPECT_TRUE(vhost_core->Enabled(RewriteOptions::kRewriteJavascriptInline));
  EXPECT_FALSE(vhost_core->Enabled(RewriteOptions::kInPlaceOptimizeForBrowser));
  EXPECT_TRUE(vhost_core->in_place_rewriting_enabled());
  EXPECT_FALSE(vhost_core->css_preserve_urls());
  EXPECT_FALSE(vhost_core->image_preserve_urls());
  EXPECT_FALSE(vhost_core->js_preserve_urls());

  // Finally, merge in another option-set that is bandwidth-only.  We'll
  // revert back to the bandwidth-behavior, but we will inherit the override
  // for CSS preservation we made.
  scoped_ptr<RewriteOptions> bandwidth(new RewriteOptions(&thread_system_));
  bandwidth->SetRewriteLevel(RewriteOptions::kOptimizeForBandwidth);
  MergeOptions(*vhost_core, *bandwidth);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kRecompressJpeg));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kRewriteCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kRewriteJavascriptExternal));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kRewriteJavascriptInline));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInPlaceOptimizeForBrowser));
  EXPECT_TRUE(options_.in_place_rewriting_enabled());
  EXPECT_FALSE(options_.css_preserve_urls());
  EXPECT_TRUE(options_.image_preserve_urls());
  EXPECT_TRUE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, BandwidthOverride) {
  options_.SetRewriteLevel(RewriteOptions::kOptimizeForBandwidth);
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kCombineCss));
  options_.EnableFilter(RewriteOptions::kCombineCss);
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));

  // Now test it the other way around.
  RewriteOptions other_way(&thread_system_);
  other_way.SetRewriteLevel(RewriteOptions::kOptimizeForBandwidth);
  other_way.ComputeSignature();
  EXPECT_FALSE(other_way.Enabled(RewriteOptions::kCombineCss));
  other_way.ClearSignatureForTesting();
  other_way.EnableFilter(RewriteOptions::kCombineCss);
  other_way.ComputeSignature();
  EXPECT_TRUE(other_way.Enabled(RewriteOptions::kCombineCss));
}

TEST_F(RewriteOptionsTest, PreserveOverridesCoreCss) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_css_preserve_urls(true);
  options_.ComputeSignature();
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineGoogleFontCss));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineImportToLink));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineCss));
}

TEST_F(RewriteOptionsTest, ExplicitCssFiltersOverridePreserve) {
  options_.set_css_preserve_urls(true);
  options_.ClearSignatureForTesting();
  options_.EnableFilter(RewriteOptions::kCombineCss);
  options_.EnableFilter(RewriteOptions::kExtendCacheCss);
  options_.EnableFilter(RewriteOptions::kInlineCss);
  options_.EnableFilter(RewriteOptions::kInlineGoogleFontCss);
  options_.EnableFilter(RewriteOptions::kInlineImportToLink);
  options_.EnableFilter(RewriteOptions::kLeftTrimUrls);
  options_.EnableFilter(RewriteOptions::kOutlineCss);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineGoogleFontCss));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineImportToLink));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kLeftTrimUrls));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineCss));
}

TEST_F(RewriteOptionsTest, PreserveOverridesCoreImages) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_image_preserve_urls(true);
  options_.ComputeSignature();
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDelayImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kLazyloadImages));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_FALSE(options_.Enabled(
      RewriteOptions::kResizeToRenderedImageDimensions));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kSpriteImages));
}

TEST_F(RewriteOptionsTest, ExplicitImageFiltersOverridePreserve) {
  options_.set_image_preserve_urls(true);
  options_.EnableFilter(RewriteOptions::kDelayImages);
  options_.EnableFilter(RewriteOptions::kExtendCacheImages);
  options_.EnableFilter(RewriteOptions::kInlineImages);
  options_.EnableFilter(RewriteOptions::kLazyloadImages);
  options_.EnableFilter(RewriteOptions::kResizeImages);
  options_.EnableFilter(RewriteOptions::kResizeToRenderedImageDimensions);
  options_.EnableFilter(RewriteOptions::kSpriteImages);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDelayImages));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheImages));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineImages));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kLazyloadImages));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kResizeImages));
  EXPECT_TRUE(options_.Enabled(
      RewriteOptions::kResizeToRenderedImageDimensions));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kSpriteImages));
}

TEST_F(RewriteOptionsTest, PreserveOverridesCoreJavaScript) {
  options_.SetRewriteLevel(RewriteOptions::kCoreFilters);
  options_.set_js_preserve_urls(true);
  options_.ComputeSignature();
  EXPECT_FALSE(options_.Enabled(
      RewriteOptions::kCanonicalizeJavascriptLibraries));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kCombineJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kInlineJavascript));
  EXPECT_FALSE(options_.Enabled(RewriteOptions::kOutlineJavascript));
}

TEST_F(RewriteOptionsTest, ExplicitJavaScriptFiltersOverridesPreserve) {
  options_.EnableFilter(RewriteOptions::kCanonicalizeJavascriptLibraries);
  options_.EnableFilter(RewriteOptions::kCombineJavascript);
  options_.EnableFilter(RewriteOptions::kDeferJavascript);
  options_.EnableFilter(RewriteOptions::kExtendCacheScripts);
  options_.EnableFilter(RewriteOptions::kInlineJavascript);
  options_.EnableFilter(RewriteOptions::kOutlineJavascript);
  options_.set_js_preserve_urls(true);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(
      RewriteOptions::kCanonicalizeJavascriptLibraries));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kCombineJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kDeferJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineJavascript));
  EXPECT_TRUE(options_.Enabled(RewriteOptions::kOutlineJavascript));
}

TEST_F(RewriteOptionsTest, ExtendCacheScriptsOverridesPreserve) {
  RewriteOptions global_options(&thread_system_);
  global_options.set_js_preserve_urls(true);
  global_options.SetRewriteLevel(RewriteOptions::kCoreFilters);
  global_options.ComputeSignature();
  EXPECT_FALSE(global_options.Enabled(RewriteOptions::kInlineJavascript));

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.EnableFilter(RewriteOptions::kExtendCacheScripts);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineJavascript));
  EXPECT_FALSE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, ExtendCacheImagesOverridesPreserve) {
  RewriteOptions global_options(&thread_system_);
  global_options.set_image_preserve_urls(true);
  global_options.SetRewriteLevel(RewriteOptions::kCoreFilters);
  global_options.ComputeSignature();
  EXPECT_FALSE(global_options.Enabled(RewriteOptions::kInlineImages));

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.EnableFilter(RewriteOptions::kExtendCacheImages);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineImages));
  EXPECT_FALSE(options_.image_preserve_urls());
}

TEST_F(RewriteOptionsTest, ExtendCacheStylesOverridesPreserve) {
  RewriteOptions global_options(&thread_system_);
  global_options.set_css_preserve_urls(true);
  global_options.SetRewriteLevel(RewriteOptions::kCoreFilters);
  global_options.ComputeSignature();
  EXPECT_FALSE(global_options.Enabled(RewriteOptions::kInlineCss));

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.EnableFilter(RewriteOptions::kExtendCacheCss);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_TRUE(options_.Enabled(RewriteOptions::kInlineCss));
  EXPECT_FALSE(options_.css_preserve_urls());
}

TEST_F(RewriteOptionsTest, PreserveOverridesExplicitFiltersScripts) {
  RewriteOptions global_options(&thread_system_);
  global_options.EnableFilter(RewriteOptions::kExtendCacheScripts);
  global_options.ComputeSignature();

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.set_js_preserve_urls(true);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheScripts));
  EXPECT_TRUE(options_.js_preserve_urls());
}

TEST_F(RewriteOptionsTest, PreserveOverridesExplicitFiltersImages) {
  RewriteOptions global_options(&thread_system_);
  global_options.EnableFilter(RewriteOptions::kExtendCacheImages);
  global_options.ComputeSignature();

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.set_image_preserve_urls(true);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheImages));
  EXPECT_TRUE(options_.image_preserve_urls());
}

TEST_F(RewriteOptionsTest, PreserveOverridesExplicitFiltersStyles) {
  RewriteOptions global_options(&thread_system_);
  global_options.EnableFilter(RewriteOptions::kExtendCacheCss);
  global_options.ComputeSignature();

  RewriteOptions vhost_options(&thread_system_);
  vhost_options.set_css_preserve_urls(true);
  MergeOptions(global_options, vhost_options);
  options_.ComputeSignature();

  EXPECT_FALSE(options_.Enabled(RewriteOptions::kExtendCacheCss));
  EXPECT_TRUE(options_.css_preserve_urls());
}

TEST_F(RewriteOptionsTest, MergeInlineResourcesWithoutExplicitAuthorization) {
  // Different variations of "off" and no-value in global and local options.
  VerifyInlineUnauthorizedResourceTypeMerges("off", "", false, false);
  VerifyInlineUnauthorizedResourceTypeMerges("off", "off", false, false);
  VerifyInlineUnauthorizedResourceTypeMerges("", "off", false, false);
  VerifyInlineUnauthorizedResourceTypeMerges("", "", false, false);

  // Local has "script", and global has effective "off".
  VerifyInlineUnauthorizedResourceTypeMerges("off", "script", true, false);
  VerifyInlineUnauthorizedResourceTypeMerges("", "script", true, false);

  // Local has no-value and global has "script".
  VerifyInlineUnauthorizedResourceTypeMerges("script", "", true, false);

  // Local has "off" and global has "script".
  VerifyInlineUnauthorizedResourceTypeMerges("script", "off", false, false);

  // Merging of script, stylesheet.
  VerifyInlineUnauthorizedResourceTypeMerges(
      "script", "stylesheet", false, true);
  VerifyInlineUnauthorizedResourceTypeMerges(
      "script", "script,stylesheet", true, true);
  VerifyInlineUnauthorizedResourceTypeMerges(
      "script,stylesheet", "stylesheet", false, true);
  VerifyInlineUnauthorizedResourceTypeMerges(
      "script,stylesheet", "", true, true);
}

TEST_F(RewriteOptionsTest, OptionsToString) {
  options_.SetRewriteLevel(RewriteOptions::kPassThrough);
  options_.UpdateCacheInvalidationTimestampMs(MockTimer::kApr_5_2010_ms);
  options_.EnableFilter(RewriteOptions::kSpriteImages);
  options_.set_inline_only_critical_images(true);
  RewriteOptions::ResourceCategorySet resources;
  resources.insert(semantic_type::kImage);
  resources.insert(semantic_type::kScript);
  options_.set_inline_unauthorized_resource_types(resources);
  options_.set_lazyload_images_blank_url("1.gif");
  NullMessageHandler handler;
  options_.WriteableDomainLawyer()->AddOriginDomainMapping(
      "origin.com", "from.com", "host.com", &handler);

  // These two options must be set to override settings established in
  // RewriteOptions' constructor when running on valgrind, otherwise
  // we'll see different results from OptionsForString.
  options_.set_rewrite_deadline_ms(100);
  options_.set_in_place_rewrite_deadline_ms(200);

  EXPECT_STREQ(StrCat(
      "Version: ", IntegerToString(RewriteOptions::kOptionsVersion), ": on\n"
      "\n"
      "Filters\n"
      "hw\tFlushes html\n"  // TODO(jmarantz): remove from base config?
      "is\tSprite Images\n"
      "\n"
      "Options\n"
      "  InlineOnlyCriticalImages (ioci)                      True\n"
      "  InlineResourcesWithoutExplicitAuthorization (irwea)  Image,Script\n"
      "  InPlaceRewriteDeadlineMs (iprdm)                     200\n"
      "  LazyloadImagesBlankUrl (llbu)                        1.gif\n"
      "  RewriteDeadlinePerFlushMs (rdm)                      100\n"
      "  RewriteLevel (l)                                     Pass Through\n"
      "\n"
      "Domain Lawyer\n"
      "  http://from.com/ Auth OriginDomain:http://origin.com/\n"
      "  http://origin.com/ HostHeader:host.com\n"
     "\n"
      "Invalidation Timestamp: Mon, 05 Apr 2010 18:51:26 GMT "
      "(1270493486000)\n"),
               options_.OptionsToString());
}

TEST_F(RewriteOptionsTest, ColorUtilTest) {
  RewriteOptions::Color out;
  EXPECT_FALSE(RewriteOptions::ParseFromString("", &out));
  EXPECT_FALSE(RewriteOptions::ParseFromString("!123456", &out));
  EXPECT_FALSE(RewriteOptions::ParseFromString("#12345", &out));
  EXPECT_TRUE(RewriteOptions::ParseFromString("#123456", &out));
  EXPECT_EQ(0x12u, out.r);
  EXPECT_EQ(0x34u, out.g);
  EXPECT_EQ(0x56u, out.b);
  EXPECT_TRUE(RewriteOptions::ParseFromString("#ABCDEF", &out));
  EXPECT_EQ(0xabu, out.r);
  EXPECT_EQ(0xcdu, out.g);
  EXPECT_EQ(0xefu, out.b);

  EXPECT_EQ("#abcdef", RewriteOptions::ToString(out));
}

TEST_F(RewriteOptionsTest, OptionsScopeApplications) {
  NullMessageHandler handler;
  GoogleString msg;
  scoped_ptr<RewriteOptions> new_options(new RewriteOptions(&thread_system_));

  // MaxHtmlParseBytes has RewriteOptions::kLegacyProcessScope.
  // Setting this value should work.
  RewriteOptions::OptionSettingResult result =
      new_options->ParseAndSetOptionFromNameWithScope(
          RewriteOptions::kMaxHtmlParseBytes, "44",
          RewriteOptions::kLegacyProcessScope, &msg, &handler);
  EXPECT_EQ("", msg);
  EXPECT_EQ(result, RewriteOptions::kOptionOk);

  // Setting the value with a max_scope of RewriteOptions::kQueryScope should
  // not work.
  result = new_options->ParseAndSetOptionFromNameWithScope(
      RewriteOptions::kMaxHtmlParseBytes, "44", RewriteOptions::kQueryScope,
      &msg, &handler);
  EXPECT_EQ("", msg);
  EXPECT_EQ(result, RewriteOptions::kOptionNameUnknown);
}

TEST_F(RewriteOptionsTest, ParseFloats) {
  RewriteOptions::ResponsiveDensities densities, expected_densities;

  expected_densities.push_back(2);
  expected_densities.push_back(2.8);
  expected_densities.push_back(3.1);

  EXPECT_TRUE(RewriteOptions::ParseFromString("2, 2.8, 3.1", &densities));
  EXPECT_EQ(expected_densities, densities);
  EXPECT_STREQ("2,2.8,3.1", RewriteOptions::ToString(densities));

  EXPECT_TRUE(RewriteOptions::ParseFromString("2.8, 2, 3.1", &densities));
  EXPECT_EQ(expected_densities, densities);
  EXPECT_STREQ("2,2.8,3.1", RewriteOptions::ToString(densities));

  EXPECT_TRUE(RewriteOptions::ParseFromString("3.1, 2.8, 2", &densities));
  EXPECT_EQ(expected_densities, densities);
  EXPECT_STREQ("2,2.8,3.1", RewriteOptions::ToString(densities));

  EXPECT_TRUE(RewriteOptions::ParseFromString("13", &densities));
  ASSERT_EQ(1, densities.size());
  EXPECT_EQ(13, densities[0]);
  EXPECT_STREQ("13", RewriteOptions::ToString(densities));

  EXPECT_FALSE(RewriteOptions::ParseFromString("", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("Hello", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("1, 2; 3", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("1, 2, 3f", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("1, 2, -5", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("1.2.3", &densities));
  EXPECT_FALSE(RewriteOptions::ParseFromString("1 2 3", &densities));
}

TEST_F(RewriteOptionsTest, ParseAllowVaryOn) {
  // Explicitly listed headers should be supported, independently of "Via"
  // header.
  VerifyAllowVaryOn("User-Agent",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    false /* expected_allow_save_data */,
                    true /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "User-Agent");
  VerifyAllowVaryOn("Save-Data",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "Save-Data");
  VerifyAllowVaryOn("Accept",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    false /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    true /* expected_allow_accept */,
                    "Accept");
  VerifyAllowVaryOn("Save-Data,Accept,User-Agent",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    true /* expected_allow_user_agent */,
                    true /* expected_allow_accept */,
                    "Accept,Save-Data,User-Agent");
  VerifyAllowVaryOn("Save-Data,Accept,User-Agent",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    true /* expected_allow_user_agent */,
                    true /* expected_allow_accept */,
                    "Accept,Save-Data,User-Agent");

  // Case and empty space don't matter.
  VerifyAllowVaryOn(" accept,SAVE-DATA,   uSER-aGENT  ",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    true /* expected_allow_user_agent */,
                    true /* expected_allow_accept */,
                    "Accept,Save-Data,User-Agent");

  // "None" disables all headers.
  VerifyAllowVaryOn("None",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    false /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "None");
  VerifyAllowVaryOn("nONE  ",
                    true /* expected_valid */,
                    false /* expected_allow_auto */,
                    false /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "None");

  // In "Auto" mode, the "Auto" bit is set and the "Save-Data" header is
  // enabled. Caller can decide which other headers to allow.
  VerifyAllowVaryOn("AUTO",
                    true /* expected_valid */,
                    true /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "Auto");
  VerifyAllowVaryOn("   auto ",
                    true /* expected_valid */,
                    true /* expected_allow_auto */,
                    true /* expected_allow_save_data */,
                    false /* expected_allow_user_agent */,
                    false /* expected_allow_accept */,
                    "Auto");

  const bool not_used = false;
  // Unsupported or invalid headers will not be accepted.
  VerifyAllowVaryOn("Content-Length,User-Agent",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn(", ,User-Agent,Invalid",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn("Content-Length,Invalid",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");

  // Mixing "Auto" with "None", or mixing either of them with other headers
  // is not allowed.
  VerifyAllowVaryOn("Auto,None",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn("Auto,Accept",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn("Content-Length,None",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");

  // Empty string and extra comma are disallowed.
  VerifyAllowVaryOn("",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn("    ",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn(",",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn(", ,, ",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
  VerifyAllowVaryOn("accept,",
                    false /* expected_valid */,
                    not_used, not_used, not_used, not_used, "not-used");
}

TEST_F(RewriteOptionsTest, MergeAllowVaryOnOptions) {
  // New option, if specified, will always overwrite the old one.
  VerifyMergingAllowVaryOn("Accept,User-Agent", "Save-Data", "Save-Data");
  VerifyMergingAllowVaryOn("Accept", "Save-Data", "Save-Data");
  VerifyMergingAllowVaryOn("Accept", "None", "None");
  VerifyMergingAllowVaryOn("", "Save-Data", "Save-Data");
  VerifyMergingAllowVaryOn("", "None", "None");
  VerifyMergingAllowVaryOn("", "Auto", "Auto");

  // New option, is un-specified, will be ignored.
  VerifyMergingAllowVaryOn("Accept,User-Agent", "", "Accept,User-Agent");
  VerifyMergingAllowVaryOn("None", "", "None");
  VerifyMergingAllowVaryOn("Auto", "", "Auto");

  // If neither option has been specified, the default will be used.
  VerifyMergingAllowVaryOn("", "", "Auto");
}

TEST_F(RewriteOptionsTest, MergeAllowDisallow) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.Disallow("*");
  EXPECT_FALSE(one.IsAllowed("foobar"));
  EXPECT_FALSE(one.IsAllowed("bar"));
  two.Allow("foo*");
  EXPECT_TRUE(two.IsAllowed("foobar"));
  EXPECT_TRUE(two.IsAllowed("bar"));
  MergeOptions(one, two);
  EXPECT_TRUE(options_.IsAllowed("foobar"));
  EXPECT_FALSE(options_.IsAllowed("bar"));
}

TEST_F(RewriteOptionsTest, MergeAllowDisallowStar) {
  RewriteOptions one(&thread_system_), two(&thread_system_);
  one.Disallow("*");
  EXPECT_FALSE(one.IsAllowed("foo"));
  two.Allow("*");
  EXPECT_TRUE(two.IsAllowed("foo"));
  MergeOptions(one, two);
  EXPECT_TRUE(options_.IsAllowed("foo"));
}

TEST_F(RewriteOptionsTest, ImageQualitiesOverride) {
  options_.set_image_recompress_quality(1);

  options_.set_image_webp_recompress_quality(20);
  options_.set_image_webp_recompress_quality_for_small_screens(30);
  options_.set_image_webp_quality_for_save_data(40);
  options_.set_image_webp_animated_recompress_quality(50);

  options_.set_image_jpeg_recompress_quality(21);
  options_.set_image_jpeg_recompress_quality_for_small_screens(31);
  options_.set_image_jpeg_quality_for_save_data(41);
  options_.set_image_jpeg_num_progressive_scans(5);
  options_.set_image_jpeg_num_progressive_scans_for_small_screens(3);

  CHECK_EQ(20, options_.ImageWebpQuality());
  CHECK_EQ(30, options_.ImageWebpQualityForSmallScreen());
  CHECK_EQ(40, options_.ImageWebpQualityForSaveData());
  CHECK_EQ(50, options_.ImageWebpAnimatedQuality());
  CHECK_EQ(21, options_.ImageJpegQuality());
  CHECK_EQ(31, options_.ImageJpegQualityForSmallScreen());
  CHECK_EQ(41, options_.ImageJpegQualityForSaveData());
  CHECK_EQ(5, options_.image_jpeg_num_progressive_scans());
  CHECK_EQ(3, options_.ImageJpegNumProgressiveScansForSmallScreen());

  EXPECT_TRUE(options_.HasValidSmallScreenQualities());
  EXPECT_TRUE(options_.HasValidSaveDataQualities());
}

TEST_F(RewriteOptionsTest, ImageQualitiesSubEqualToBase) {
  options_.set_image_recompress_quality(1);

  options_.set_image_webp_recompress_quality(20);
  options_.set_image_webp_recompress_quality_for_small_screens(20);
  options_.set_image_webp_quality_for_save_data(20);
  options_.set_image_webp_animated_recompress_quality(20);

  options_.set_image_jpeg_recompress_quality(21);
  options_.set_image_jpeg_recompress_quality_for_small_screens(21);
  options_.set_image_jpeg_quality_for_save_data(21);
  options_.set_image_jpeg_num_progressive_scans(5);
  options_.set_image_jpeg_num_progressive_scans_for_small_screens(5);

  CHECK_EQ(20, options_.ImageWebpQuality());
  CHECK_EQ(20, options_.ImageWebpQualityForSmallScreen());
  CHECK_EQ(20, options_.ImageWebpQualityForSaveData());
  CHECK_EQ(20, options_.ImageWebpAnimatedQuality());
  CHECK_EQ(21, options_.ImageJpegQuality());
  CHECK_EQ(21, options_.ImageJpegQualityForSmallScreen());
  CHECK_EQ(21, options_.ImageJpegQualityForSaveData());
  CHECK_EQ(5, options_.image_jpeg_num_progressive_scans());
  CHECK_EQ(5, options_.ImageJpegNumProgressiveScansForSmallScreen());

  EXPECT_FALSE(options_.HasValidSmallScreenQualities());
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
}

TEST_F(RewriteOptionsTest, ImageQualitiesSubInheritFromBase) {
  options_.set_image_recompress_quality(1);

  options_.set_image_webp_recompress_quality(-1);
  options_.set_image_webp_recompress_quality_for_small_screens(-1);
  options_.set_image_webp_quality_for_save_data(-1);
  options_.set_image_webp_animated_recompress_quality(-1);

  options_.set_image_jpeg_recompress_quality(-1);
  options_.set_image_jpeg_recompress_quality_for_small_screens(-1);
  options_.set_image_jpeg_quality_for_save_data(-1);
  options_.set_image_jpeg_num_progressive_scans(5);
  options_.set_image_jpeg_num_progressive_scans_for_small_screens(-1);

  CHECK_EQ(1, options_.ImageWebpQuality());
  CHECK_EQ(1, options_.ImageWebpQualityForSmallScreen());
  CHECK_EQ(1, options_.ImageWebpQualityForSaveData());
  CHECK_EQ(1, options_.ImageWebpAnimatedQuality());
  CHECK_EQ(1, options_.ImageJpegQuality());
  CHECK_EQ(1, options_.ImageJpegQualityForSmallScreen());
  CHECK_EQ(1, options_.ImageJpegQualityForSaveData());
  CHECK_EQ(5, options_.image_jpeg_num_progressive_scans());
  CHECK_EQ(5, options_.ImageJpegNumProgressiveScansForSmallScreen());

  EXPECT_FALSE(options_.HasValidSmallScreenQualities());
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
}

TEST_F(RewriteOptionsTest, ImageQualitiesAllDisabled) {
  options_.set_image_recompress_quality(-1);

  options_.set_image_webp_recompress_quality(-1);
  options_.set_image_webp_recompress_quality_for_small_screens(-1);
  options_.set_image_webp_quality_for_save_data(-1);
  options_.set_image_webp_animated_recompress_quality(-1);

  options_.set_image_jpeg_recompress_quality(-1);
  options_.set_image_jpeg_recompress_quality_for_small_screens(-1);
  options_.set_image_jpeg_quality_for_save_data(-1);

  CHECK_EQ(-1, options_.ImageWebpQuality());
  CHECK_EQ(-1, options_.ImageWebpQualityForSmallScreen());
  CHECK_EQ(-1, options_.ImageWebpQualityForSaveData());
  CHECK_EQ(-1, options_.ImageWebpAnimatedQuality());
  CHECK_EQ(-1, options_.ImageJpegQuality());
  CHECK_EQ(-1, options_.ImageJpegQualityForSmallScreen());
  CHECK_EQ(-1, options_.ImageJpegQualityForSaveData());

  EXPECT_FALSE(options_.HasValidSmallScreenQualities());
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
}

TEST_F(RewriteOptionsTest, SupportSaveData) {
  // By default, AllowVaryOn is set to "Auto" which implies "Save-Data".
  options_.set_image_jpeg_quality_for_save_data(-1);
  options_.set_image_webp_quality_for_save_data(-1);
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
  EXPECT_TRUE(options_.AllowVaryOnSaveData());
  EXPECT_FALSE(options_.SupportSaveData());

  options_.set_image_jpeg_quality_for_save_data(20);
  options_.set_image_webp_quality_for_save_data(30);
  EXPECT_TRUE(options_.HasValidSaveDataQualities());
  EXPECT_TRUE(options_.AllowVaryOnSaveData());
  EXPECT_TRUE(options_.SupportSaveData());

  // Disallow vary on "Save-Data".
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.SetOptionFromName(RewriteOptions::kAllowVaryOn,
                                       "None"));
  options_.set_image_jpeg_quality_for_save_data(-1);
  options_.set_image_webp_quality_for_save_data(-1);
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
  EXPECT_FALSE(options_.AllowVaryOnSaveData());
  EXPECT_FALSE(options_.SupportSaveData());

  options_.set_image_jpeg_quality_for_save_data(20);
  options_.set_image_webp_quality_for_save_data(30);
  EXPECT_TRUE(options_.HasValidSaveDataQualities());
  EXPECT_FALSE(options_.AllowVaryOnSaveData());
  EXPECT_FALSE(options_.SupportSaveData());

  // Explicitly allow vary on "Save-Data".
  EXPECT_EQ(RewriteOptions::kOptionOk,
            options_.SetOptionFromName(RewriteOptions::kAllowVaryOn,
                                       "Save-Data"));
  EXPECT_TRUE(options_.HasValidSaveDataQualities());
  EXPECT_TRUE(options_.AllowVaryOnSaveData());
  EXPECT_TRUE(options_.SupportSaveData());

  options_.set_image_jpeg_quality_for_save_data(-1);
  options_.set_image_webp_quality_for_save_data(-1);
  EXPECT_FALSE(options_.HasValidSaveDataQualities());
  EXPECT_TRUE(options_.AllowVaryOnSaveData());
  EXPECT_FALSE(options_.SupportSaveData());
}

}  // namespace net_instaweb
