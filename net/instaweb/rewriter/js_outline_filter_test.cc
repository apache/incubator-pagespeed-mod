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


#include "net/instaweb/rewriter/public/js_outline_filter.h"

#include "net/instaweb/rewriter/public/debug_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/support_noscript_filter.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

class JsOutlineFilterTest : public RewriteTestBase {
 protected:
  // We need an explicitly called method here rather than using SetUp so
  // that NoOutlineScript can call another AddFilter function first.
  void SetupOutliner() {
    DisableGzip();
    options()->set_js_outline_min_bytes(0);
    options()->SoftEnableFilterForTesting(RewriteOptions::kOutlineJavascript);
    rewrite_driver()->AddFilters();
  }

  void SetupDebug(StringPiece debug_message) {
    options()->EnableFilter(RewriteOptions::kDebug);
    SetupOutliner();

    // For some reason SupportNoScript filter is disabled here.
    StringVector expected_disabled_filters;
    SupportNoscriptFilter support_noscript_filter(rewrite_driver());
    expected_disabled_filters.push_back(support_noscript_filter.Name());

    debug_message.CopyToString(&debug_message_);
    debug_suffix_ = DebugFilter::FormatEndDocumentMessage(
        0, 0, 0, 0, 0, false, StringSet(),
        expected_disabled_filters);
  }

  // Test outlining scripts with options to write headers.
  void OutlineScript(const StringPiece& id, bool expect_outline) {
    GoogleString script_text = "FOOBAR";
    GoogleString outline_text;
    AppendDefaultHeaders(kContentTypeJavascript, &outline_text);
    outline_text += script_text;

    GoogleString hash = hasher()->Hash(script_text);
    GoogleString outline_url = Encode(
        kTestDomain, JsOutlineFilter::kFilterId, hash, "_", "js");

    GoogleString wrong_hash_outline_url = Encode(
        kTestDomain, JsOutlineFilter::kFilterId, StrCat("not", hash),
        "_", "js");

    GoogleString html_input = StrCat(
        "<head>\n"
        "  <title>Example style outline</title>\n"
        "  <!-- Script starts here -->\n"
        "  <script type='text/javascript'>", script_text, "</script>\n"
        "  <!-- Script ends here -->\n"
        "</head>");
    GoogleString expected_output;
    if (expect_outline) {
      expected_output = StrCat(
          "<head>\n"
          "  <title>Example style outline</title>\n"
          "  <!-- Script starts here -->\n"
          "  <script type='text/javascript'"
          " src=\"", outline_url, "\"></script>\n"
          "  <!-- Script ends here -->\n"
          "</head>");
    } else {
      expected_output = StrCat(
          "<head>\n"
          "  <title>Example style outline</title>\n"
          "  <!-- Script starts here -->\n"
          "  <script type='text/javascript'>", script_text, "</script>",
          debug_message_,
          "\n"
          "  <!-- Script ends here -->\n"
          "</head>");
    }
    Parse(id, html_input);
    EXPECT_HAS_SUBSTR(expected_output, output_buffer_);
    if (!debug_suffix_.empty()) {
      EXPECT_HAS_SUBSTR(debug_suffix_, output_buffer_);
    }

    if (expect_outline) {
      GoogleString actual_outline;
      ResponseHeaders headers;
      EXPECT_TRUE(FetchResourceUrl(outline_url, &actual_outline, &headers));
      EXPECT_EQ(outline_text, StrCat(headers.ToString(), actual_outline));

      // Make sure we don't try anything funny with fallbacks if the hash
      // given is wrong. It'd be an attack vector otherwise since outlined
      // resources may contain things from private pages.
      EXPECT_FALSE(
          FetchResourceUrl(wrong_hash_outline_url, &actual_outline, &headers));
    }
  }

  GoogleString debug_message_;
  GoogleString debug_suffix_;
};

// Tests for outlining scripts.
TEST_F(JsOutlineFilterTest, OutlineScript) {
  SetupOutliner();
  OutlineScript("outline_scripts_no_hash", true);
}

TEST_F(JsOutlineFilterTest, OutlineScriptMd5) {
  UseMd5Hasher();
  SetupOutliner();
  OutlineScript("outline_scripts_md5", true);
}

// Make sure we don't misplace things into domain of the base tag,
// as we may not be able to fetch from it.
// (The leaf in base href= also covers a previous check failure)
TEST_F(JsOutlineFilterTest, OutlineScriptWithBase) {
  SetupOutliner();

  const char kInput[] =
      "<base href='http://cdn.example.com/file.html'><script>42;</script>";
  GoogleString expected_output =
      StrCat("<base href='http://cdn.example.com/file.html'>",
             "<script src=\"",
             EncodeWithBase("http://cdn.example.com/", kTestDomain,
                            "jo", "0", "_", "js"),
             "\"></script>");
  ValidateExpected("test.html", kInput, expected_output);
}

// Negative test.
TEST_F(JsOutlineFilterTest, NoOutlineScript) {
  GoogleString file_prefix = GTestTempDir() + "/no_outline";
  GoogleString url_prefix = "http://mysite/no_outline";

  options()->SoftEnableFilterForTesting(RewriteOptions::kOutlineCss);
  SetupOutliner();

  static const char html_input[] =
      "<head>\n"
      "  <title>Example style outline</title>\n"
      "  <!-- Script starts here -->\n"
      "  <script type='text/javascript' src='http://othersite/script.js'>"
      "</script>\n"
      "  <!-- Script ends here -->\n"
      "</head>";
  ValidateNoChanges("no_outline_script", html_input);
}


// By default we succeed at outlining.
TEST_F(JsOutlineFilterTest, UrlNotTooLong) {
  SetupOutliner();
  OutlineScript("url_not_too_long", true);
}

TEST_F(JsOutlineFilterTest, JsPreserveURL) {
  DisableGzip();
  options()->set_js_outline_min_bytes(0);
  options()->SoftEnableFilterForTesting(RewriteOptions::kOutlineJavascript);
  options()->set_js_preserve_urls(true);
  rewrite_driver()->AddFilters();
  OutlineScript("js_preserve_url", false);
}

TEST_F(JsOutlineFilterTest, JsPreserveURLOff) {
  DisableGzip();
  options()->set_js_outline_min_bytes(0);
  options()->SoftEnableFilterForTesting(RewriteOptions::kOutlineJavascript);
  options()->set_js_preserve_urls(false);
  rewrite_driver()->AddFilters();
  OutlineScript("js_preserve_url_off", true);
}

// But if we set max_url_size too small, it will fail cleanly.
TEST_F(JsOutlineFilterTest, UrlTooLong) {
  options()->set_max_url_size(0);
  SetupDebug(StrCat("<!--Rewritten URL too long: ", kTestDomain,
                    "_.pagespeed.jo.#.-->"));
  OutlineScript("url_too_long", false);
}

// Make sure we deal well with no Charactors() node between StartElement()
// and EndElement().
TEST_F(JsOutlineFilterTest, EmptyScript) {
  SetupOutliner();
  ValidateNoChanges("empty_script", "<script></script>");
}

// http://github.com/apache/incubator-pagespeed-mod/issues/416
TEST_F(JsOutlineFilterTest, RewriteDomain) {
  SetupOutliner();
  AddRewriteDomainMapping("cdn.com", kTestDomain);

  // Check that CSS gets outlined to the rewritten domain.
  GoogleString expected_url = Encode("http://cdn.com/", "jo", "0", "_", "js");
  ValidateExpected("rewrite_domain",
                   "<script>alert('foo');</script>",
                   StrCat("<script src=\"", expected_url, "\"></script>"));

  // And check that it serves correctly from that domain.
  GoogleString content;
  ASSERT_TRUE(FetchResourceUrl(expected_url, &content));
  EXPECT_EQ("alert('foo');", content);
}

}  // namespace

}  // namespace net_instaweb
