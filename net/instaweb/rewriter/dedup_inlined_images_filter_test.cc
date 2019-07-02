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


#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/dedup_inlined_images_filter.h"
#include "net/instaweb/rewriter/public/delay_images_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/wildcard.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/user_agent_matcher_test_base.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kCuppaPngFilename[] = "Cuppa.png";
const char kPuzzleJpgFilename[] = "Puzzle.jpg";

const char kCuppaPngInlineData[] =
    "data:image/png;base64,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";
const char kCuppaPngWildcardData[] = "data:image/png;base64*";

const char kInlinedScriptFormat[] =
    "<script type=\"text/javascript\""
    " id=\"pagespeed_script_%d\" data-pagespeed-no-defer>"
    "pagespeed.dedupInlinedImages.inlineImg("
    "'pagespeed_img_0%d','pagespeed_img_0%d','pagespeed_script_%d'"
    ");</script>";

const char kHtmlWrapperFormat[] =
    "<head>\n"
    "  <title>Dedup Inlined Images Test</title>\n"
    "%s"
    "</head>\n"
    "<body>"
    "%s"
    "</body>\n";

class DedupInlinedImagesTest : public RewriteTestBase,
                               public ::testing::WithParamInterface<bool> {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetFiltersAndOptions();
    rewrite_driver()->AddFilters();
    SetCurrentUserAgent(
        UserAgentMatcherTestBase::kChrome18UserAgent);

    AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFilename),
                         kCuppaPngFilename, kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFilename),
                         kPuzzleJpgFilename, kContentTypeJpeg, 100);

    SetHtmlMimetype();  // Don't wrap scripts in <![CDATA[ ]]>
    StaticAssetManager* static_asset_manager =
        server_context()->static_asset_manager();
    dedup_inlined_images_js_ =
        StrCat("<script type=\"text/javascript\" data-pagespeed-no-defer>",
               static_asset_manager->GetAsset(
                   StaticAssetEnum::DEDUP_INLINED_IMAGES_JS, options()),
               DedupInlinedImagesFilter::kDiiInitializer,
               "</script>");
  }

  virtual void SetFiltersAndOptions() {
    options()->EnableFilter(RewriteOptions::kInlineImages);
    options()->EnableFilter(RewriteOptions::kDedupInlinedImages);
    options()->set_image_inline_max_bytes(2000);
  }

  void TestDedupImages(const StringPiece& case_id,
                       const GoogleString& head_html_in,
                       const GoogleString& head_html_out,
                       const GoogleString& body_html_in,
                       const GoogleString& body_html_out) {
    GoogleString url(StrCat(
        "http://test.com/", case_id, ".html?PageSpeed=noscript"));
    GoogleString html_in(StringPrintf(
        kHtmlWrapperFormat, head_html_in.c_str(), body_html_in.c_str()));
    GoogleString body_out(StrCat(StringPrintf(kNoScriptRedirectFormatter,
                                              url.c_str(), url.c_str()),
                                 body_html_out));
    GoogleString html_out(StringPrintf(
        kHtmlWrapperFormat, head_html_out.c_str(), body_out.c_str()));

    Parse(case_id, html_in);
    GoogleString expected_out = doctype_string_ + AddHtmlBody(html_out);

    EXPECT_EQ(expected_out, output_buffer_) << "Test id:" << case_id;
    output_buffer_.clear();
  }

  GoogleString InsertScriptBefore(const StringPiece& snippet) const {
    return StrCat(dedup_inlined_images_js_, snippet);
  }

  GoogleString dedup_inlined_images_js_;
};

TEST_F(DedupInlinedImagesTest, Simple) {
  TestDedupImages("simple", "", "", "<div/>", "<div/>");
}

TEST_F(DedupInlinedImagesTest, InlineSingleSmallImage) {
  // Add an id to the first occurence.
  TestDedupImages("inline_single_small_image", "", "",
                  StrCat("<img src='", kCuppaPngFilename, "'>"),
                  StrCat("<img src='", kCuppaPngInlineData,
                         "' id=\"pagespeed_img_01\">"));
}

TEST_F(DedupInlinedImagesTest, DontInlineLargeImage) {
  TestDedupImages("dont_inline_large_image", "", "",
                  StrCat("<img src='", kPuzzleJpgFilename, "'>"),
                  StrCat("<img src='", kPuzzleJpgFilename, "'>"));
}

TEST_F(DedupInlinedImagesTest, DedupSecondSmallImage) {
  // Add an id to the first occurence and convert the second to JavaScript.
  TestDedupImages(
      "dedup_second_small_image", "", "",
      StrCat("<img src='", kCuppaPngFilename, "'>\n",
             "<img src='", kCuppaPngFilename, "'>"),
      StrCat("<img src='", kCuppaPngInlineData,
             "' id=\"pagespeed_img_01\">\n",
             InsertScriptBefore(
                 StrCat("<img id=\"pagespeed_img_02\">",
                        StringPrintf(kInlinedScriptFormat, 3, 1, 2, 3)))));
}

TEST_F(DedupInlinedImagesTest, DedupManySmallImages) {
  // Add an id to the first occurence and convert the following to JavaScript.
  GoogleString image = StrCat("<img src='", kCuppaPngFilename, "'>");
  TestDedupImages(
      "dedup_many_small_images", "", "",
      StrCat(image, "\n", image, "\n", image),
      StrCat("<img src='", kCuppaPngInlineData, "' id=\"pagespeed_img_01\">\n",
             InsertScriptBefore(
                 StrCat("<img id=\"pagespeed_img_02\">",
                        StringPrintf(kInlinedScriptFormat, 3, 1, 2, 3), "\n",
                        "<img id=\"pagespeed_img_04\">",
                        StringPrintf(kInlinedScriptFormat, 5, 1, 4, 5)))));
}

TEST_F(DedupInlinedImagesTest, DedupSecondSmallImageWithId) {
  // Keep the id on the first occurence and convert the second to JavaScript.
  TestDedupImages("dedup_second_small_image_with_id", "", "",
                  StrCat("<img src='", kCuppaPngFilename, "' id='xyzzy'>\n",
                         "<img src='", kCuppaPngFilename, "'>"),
                  StrCat("<img src='", kCuppaPngInlineData, "' id='xyzzy'>\n",
                         InsertScriptBefore(
                             "<img id=\"pagespeed_img_01\">"
                             "<script type=\"text/javascript\""
                             " id=\"pagespeed_script_2\""
                             " data-pagespeed-no-defer>"
                             "pagespeed.dedupInlinedImages.inlineImg("
                             "'xyzzy',"
                             "'pagespeed_img_01',"
                             "'pagespeed_script_2');"
                             "</script>")));
}

TEST_F(DedupInlinedImagesTest, DedupSecondSmallImageWithAttributes) {
  // Keep all the attributes.
  TestDedupImages("dedup_second_small_image_with_attributes", "", "",
                  StrCat("<img src='", kCuppaPngFilename, "'>\n",
                         "<img src='", kCuppaPngFilename,
                         "' alt='xyzzy' id='plugh'>"),
                  StrCat("<img src='", kCuppaPngInlineData,
                         "' id=\"pagespeed_img_01\">\n",
                         InsertScriptBefore(
                             "<img alt='xyzzy' id='plugh'>"
                             "<script type=\"text/javascript\""
                             " id=\"pagespeed_script_2\""
                             " data-pagespeed-no-defer>"
                             "pagespeed.dedupInlinedImages.inlineImg("
                             "'pagespeed_img_01',"
                             "'plugh',"
                             "'pagespeed_script_2');"
                             "</script>")));
}

TEST_F(DedupInlinedImagesTest, DisabledForOldBlackberry) {
  // This UA doesn't support LazyloadImages so nor does it support deduping.
  SetCurrentUserAgent(
      UserAgentMatcherTestBase::kBlackBerryOS5UserAgent);
  GoogleString case_id("disabled_for_old_blackberry");
  GoogleString repeated_inlined_image = StrCat(
      "<img src='", kCuppaPngFilename, "'>\n",
      "<img src='", kCuppaPngFilename, "'>");
  GoogleString html_in_out(StringPrintf(
      kHtmlWrapperFormat, "", repeated_inlined_image.c_str()));
  Parse(case_id, html_in_out);
  GoogleString expected_out = doctype_string_ + AddHtmlBody(html_in_out);
  EXPECT_EQ(expected_out, output_buffer_) << "Test id:" << case_id;
  output_buffer_.clear();
}

class DedupInlinePreviewImagesTest : public DedupInlinedImagesTest {
 public:
  DedupInlinePreviewImagesTest() : DedupInlinedImagesTest() {}

  virtual void SetFiltersAndOptions() {
    options()->EnableFilter(RewriteOptions::kDedupInlinedImages);
    options()->EnableFilter(RewriteOptions::kDelayImages);
    options()->set_min_image_size_low_resolution_bytes(1 * 1024);
    options()->set_max_inlined_preview_images_index(-1);
  }

  GoogleString GetNoscript() const {
    return StringPrintf(
        kNoScriptRedirectFormatter,
        "http://test.com/dedup_inline_preview_images.html?PageSpeed=noscript",
        "http://test.com/dedup_inline_preview_images.html?PageSpeed=noscript");
  }

  GoogleString GetImageOnloadScriptBlock() const {
    return StrCat("<script data-pagespeed-no-defer type=\"text/javascript\">",
                  DelayImagesFilter::kImageOnloadJsSnippet,
                  "</script>");
  }

  GoogleString GenerateRewrittenImageTag(const GoogleString& url,
                                         const GoogleString& low_res_src) {
    return StrCat("<img data-pagespeed-high-res-src='", url, "'",
                  " src='", low_res_src, "'"
                  " onload='", DelayImagesFilter::kImageOnloadCode, "'/>");
  }
};

TEST_F(DedupInlinePreviewImagesTest, DedupInlinePreviewImages) {
  GoogleString image_filename = StrCat(kTestDomain, kCuppaPngFilename);
  GoogleString input_img = StrCat("<img src='", image_filename, "'/>");
  GoogleString inlined_img =
      StrCat("<img data-pagespeed-high-res-src='", image_filename,
             "' src=\"", kCuppaPngWildcardData,
             "\" onload=\"", DelayImagesFilter::kImageOnloadCode,
             "\" onerror=\"this.onerror=null;",
             DelayImagesFilter::kImageOnloadCode,
             "\" id=\"pagespeed_img_01\"/>");
  GoogleString scripted_img_fmt =
      StrCat("<img data-pagespeed-high-res-src='", image_filename,
             "' onload=\"", DelayImagesFilter::kImageOnloadCode,
             "\" onerror=\"this.onerror=null;",
             DelayImagesFilter::kImageOnloadCode,
             "\" id=\"pagespeed_img_0%d\"/>");
  GoogleString scripted_img_1 = StringPrintf(scripted_img_fmt.c_str(), 2);
  GoogleString scripted_img_2 = StringPrintf(scripted_img_fmt.c_str(), 4);
  GoogleString script_1 = StringPrintf(kInlinedScriptFormat, 3, 1, 2, 3);
  GoogleString script_2 = StringPrintf(kInlinedScriptFormat, 5, 1, 4, 5);
  GoogleString input_html = StrCat("<head></head>"
                                   "<body>",
                                   input_img, input_img, input_img,
                                   "</body>");
  GoogleString output_html = StrCat("<head></head>"
                                    "<body>",
                                    GetNoscript(),
                                    GetImageOnloadScriptBlock(),
                                    inlined_img,
                                    InsertScriptBefore(
                                        StrCat(scripted_img_1, script_1,
                                               scripted_img_2, script_2)),
                                    "</body>");

  // Since the preview image has been resized use a wildcard to match it.
  Parse("dedup_inline_preview_images", input_html);
  GoogleString full_html = StrCat(doctype_string_, AddHtmlBody(output_html));
  EXPECT_TRUE(Wildcard(full_html).Match(output_buffer_))
      << "Output_Html:\n" << full_html << "\n"
      << "Got:\n" << output_buffer_;
}

}  // namespace

}  // namespace net_instaweb
