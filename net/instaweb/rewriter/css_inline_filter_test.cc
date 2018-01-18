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


#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/css_inline_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

namespace {

class CssInlineFilterTest : public RewriteTestBase {
 protected:
  CssInlineFilterTest() : filters_added_(false) {}

  void TestInlineCssWithOutputUrl(
                     const GoogleString& html_url,
                     const GoogleString& head_extras,
                     const GoogleString& css_url,
                     const GoogleString& css_out_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body,
                     const GoogleString& debug_string) {
    if (!filters_added_) {
      AddFilter(RewriteOptions::kInlineCss);
      filters_added_ = true;
    }

    GoogleString html_template = StrCat(
        "<head>\n",
        head_extras,
        "  <link rel=\"stylesheet\" href=\"%s\"",
        (other_attrs.empty() ? "" : " " + other_attrs) + ">",
        "%s\n</head>\n"
        "<body>Hello, world!</body>\n");

    const GoogleString html_input =
        StringPrintf(html_template.c_str(), css_url.c_str(), "");

    const GoogleString outline_html_output =
        StringPrintf(html_template.c_str(), css_out_url.c_str(), "");

    const GoogleString outline_debug_html_output = debug_string.empty()
        ? outline_html_output
        : StringPrintf(html_template.c_str(), css_out_url.c_str(),
                       StrCat("<!--", debug_string, "-->").c_str());

    // Put original CSS file into our fetcher.
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(css_url, default_css_header, css_original_body);

    // Rewrite the HTML page.
    ParseUrl(html_url, html_input);

    const GoogleString expected_output =
        (!expect_inline ? outline_html_output :
         StrCat("<head>\n",
                head_extras,
                StrCat("  <style",
                       (other_attrs.empty() ? "" : " " + other_attrs),
                       ">"),
                css_rewritten_body, "</style>\n"
                "</head>\n"
                "<body>Hello, world!</body>\n"));
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);

    if (!expect_inline) {
      output_buffer_.clear();
      TurnOnDebug();
      ParseUrl(html_url, html_input);
      EXPECT_EQ(AddHtmlBody(outline_debug_html_output), output_buffer_);
    }
  }

  void TestInlineCss(const GoogleString& html_url,
                     const GoogleString& css_url,
                     const GoogleString& other_attrs,
                     const GoogleString& css_original_body,
                     bool expect_inline,
                     const GoogleString& css_rewritten_body) {
    TestInlineCssWithOutputUrl(
        html_url, "", css_url, css_url, other_attrs, css_original_body,
        expect_inline, css_rewritten_body, "");
  }

  void TestNoInlineCss(const GoogleString& html_url,
                       const GoogleString& css_url,
                       const GoogleString& other_attrs,
                       const GoogleString& css_original_body,
                       const GoogleString& css_rewritten_body,
                       const GoogleString& debug_string) {
    TestInlineCssWithOutputUrl(
        html_url, "", css_url, css_url, other_attrs, css_original_body,
        false, css_rewritten_body, debug_string);
  }

  void VerifyNoInliningForClosingStyleTag(
      const GoogleString& closing_style_tag) {
    AddFilter(RewriteOptions::kInlineCss);
    SetResponseWithDefaultHeaders("foo.css", kContentTypeCss,
                                  StrCat("a{margin:0}", closing_style_tag),
                                  100);

    // We don't mess with links that contain a closing style tag.
    ValidateNoChanges("no_inlining_of_close_style_tag",
                      "<link rel='stylesheet' href='foo.css'>");

    TurnOnDebug();
    ValidateExpected("no_inlining_of_close_style_tag+debug",
                     "<link rel='stylesheet' href='foo.css'>",
                     "<link rel='stylesheet' href='foo.css'>"
                     "<!--CSS not inlined since it contains "
                          "style closing tag-->");
  }

  void TurnOnDebug() {
    options()->ClearSignatureForTesting();
    options()->EnableFilter(RewriteOptions::kDebug);
    server_context()->ComputeSignature(options());
  }

  bool AddHtmlTags() const override { return false; }

 private:
  bool filters_added_;
};

TEST_F(CssInlineFilterTest, InlineCssSimple) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

class CssInlineFilterTestCustomOptions : public CssInlineFilterTest {
 protected:
  // Derived classes should add their options and then call
  // CssInlineFilterTest::SetUp().
  virtual void SetUp() {}
};

TEST_F(CssInlineFilterTest, InlineCssUnhealthy) {
  lru_cache()->set_is_healthy(false);
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, false, css);
}

TEST_F(CssInlineFilterTest, InlineCss404) {
  // Test to make sure that a missing input is handled well.
  SetFetchResponse404("404.css");
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<link rel=stylesheet href='404.css'>");
}

TEST_F(CssInlineFilterTest, InlineCssCached) {
  // Doing it twice should be safe, too.
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, InlineCssRewriteUrls1) {
  // CSS with a relative URL that needs to be changed:
  const GoogleString css1 =
      "BODY { background-image: url('bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: url('foo/bar/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, InlineCssRewriteUrls2) {
  // CSS with a relative URL, this time with ".." in it:
  const GoogleString css1 =
      "BODY { background-image: url('../quux/bg.png'); }\n";
  const GoogleString css2 =
      "BODY { background-image: url('foo/quux/bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/foo/bar/baz.css",
                "", css1, true, css2);
}

TEST_F(CssInlineFilterTest, NoRewriteUrlsSameDir) {
  const GoogleString css = "BODY { background-image: url('bg.png'); }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css, true, css);
}

TEST_F(CssInlineFilterTest, ShardSubresources) {
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  lawyer->AddShard("www.example.com", "shard1.com,shard2.com",
                   &message_handler_);

  const GoogleString css_in =
      ".p1 { background-image: url('b1.png'); }"
      ".p2 { background-image: url('b2.png'); }";
  const GoogleString css_out =
      ".p1 { background-image: url('http://shard2.com/b1.png'); }"
      ".p2 { background-image: url('http://shard1.com/b2.png'); }";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/baz.css",
                "", css_in, true, css_out);
}

TEST_F(CssInlineFilterTest, DoNotInlineCssWithMediaNotScreen) {
  const GoogleString css = "BODY { color: red; }\n";
  TestNoInlineCss("http://www.example.com/index.html",
                  "http://www.example.com/styles.css",
                  "media=\"print\"", css, "",
                  "CSS not inlined because media does not match screen");
}

TEST_F(CssInlineFilterTest, DoInlineCssWithMediaAll) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"all\"", css, true, css);
}

TEST_F(CssInlineFilterTest, DoInlineCssWithMediaScreen) {
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"print, audio ,, ,sCrEeN \"", css, true, css);
}

TEST_F(CssInlineFilterTest, DoInlineCssWithMediaQuery) {
  // Media queries are tested more exhaustively in css_tag_scanner_test.
  const GoogleString css = "BODY { color: red; }\n";
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                "media=\"only (color)\"", css, true, css);
}

TEST_F(CssInlineFilterTest, Empty) {
  // Don't inline empty resources. This is defensive programming against
  // issues like: https://github.com/apache/incubator-pagespeed-mod/issues/1050
  const GoogleString css = "";
  TestNoInlineCss("http://www.example.com/index.html",
                  "http://www.example.com/styles.css",
                  "", css, "",
                  "Resource is empty, preventing rewriting of "
                  "http://www.example.com/styles.css");
}

TEST_F(CssInlineFilterTest, InlineCssWithInvalidMedia) {
  // Use an invalid media tag, but one that's still decipherable.
  // Trying to deal with indecipherable media tags turned out to be
  // more trouble than it's worth.
  const char kNotValid[] = "not!?#?;valid";

  const GoogleString css = "BODY { color: red; }\n";
  GoogleString media;

  // Now do the actual test that we don't inline the CSS with an invalid
  // media type (and not screen or all as well).
  media = StrCat("media=\"", kNotValid, "\"");
  TestNoInlineCss("http://www.example.com/index.html",
                  "http://www.example.com/styles.css",
                  media, css, "",
                  "CSS not inlined because media does not match screen");

  // And now test that we DO inline the CSS with an invalid media type
  // if there's also an instance of "screen" in the media attribute.
  media = StrCat("media=\"", kNotValid, ",screen\"");
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/styles.css",
                media, css, true, css);
}

TEST_F(CssInlineFilterTest, DoNotInlineCssTooBig) {
  // CSS too large to inline:
  const int64 length = 2 * RewriteOptions::kDefaultCssInlineMaxBytes;
  TestNoInlineCss("http://www.example.com/index.html",
                  "http://www.example.com/styles.css", "",
                  ("BODY { background-image: url('" +
                   GoogleString(length, 'z') + ".png'); }\n"),
                  "", "CSS not inlined since it&#39;s bigger than 2048 bytes");
}

TEST_F(CssInlineFilterTest, DoInlineCssDifferentDomain) {
  const GoogleString css = "BODY { color: red; }\n";
  options()->AddInlineUnauthorizedResourceType(semantic_type::kStylesheet);
  TestInlineCss("http://www.example.com/index.html",
                "http://unauth.com/styles.css",
                "", css, true, css);
  EXPECT_EQ(1,
            statistics()->GetVariable(CssInlineFilter::kNumCssInlined)->Get());
}

TEST_F(CssInlineFilterTest, DoNotInlineCssDifferentDomain) {
  // Note: This only fails because we haven't authorized unauth.com
  GoogleUrl gurl("http://unauth.com/styles.css");
  TestNoInlineCss("http://www.example.com/index.html", gurl.Spec().as_string(),
                  "", "BODY { color: red; }\n", "",
                  rewrite_driver()->GenerateUnauthorizedDomainDebugComment(
                      gurl, RewriteDriver::InputRole::kStyle));
  EXPECT_EQ(0,
            statistics()->GetVariable(CssInlineFilter::kNumCssInlined)->Get());
}

TEST_F(CssInlineFilterTest, CorrectlyInlineCssWithImports) {
  TestInlineCss("http://www.example.com/index.html",
                "http://www.example.com/dir/styles.css", "",
                "@import \"foo.css\"; BODY { color: red; }\n", true,
                "@import \"dir/foo.css\"; BODY { color: red; }\n");
}

// https://github.com/apache/incubator-pagespeed-mod/issues/252
TEST_F(CssInlineFilterTest, ClaimsXhtmlButHasUnclosedLink) {
  // XHTML text should not have unclosed links.  But if they do, like
  // in Issue 252, then we should leave them alone.
  static const char html_format[] =
      "<head>\n"
      "  %s\n"
      "  %s\n"
      "  <script type='text/javascript' src='c.js'></script>"     // 'in' <link>
      "</head>\n"
      "<body><div class=\"c1\"><div class=\"c2\"><p>\n"
      "  Yellow on Blue</p></div></div></body>";

  static const char unclosed_css[] =
      "  <link rel='stylesheet' href='a.css' type='text/css'>\n";  // unclosed
  static const char inlined_css[] = "  <style>.a {}</style>\n";

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, "a.css"), default_css_header, ".a {}");
  AddFilter(RewriteOptions::kInlineCss);
  ValidateExpected("claims_xhtml_but_has_unclosed_links",
                   StringPrintf(html_format, kXhtmlDtd, unclosed_css),
                   StringPrintf(html_format, kXhtmlDtd, inlined_css));
}

TEST_F(CssInlineFilterTest, DontInlineInNoscript) {
  options()->EnableFilter(RewriteOptions::kInlineCss);
  rewrite_driver()->AddFilters();

  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString html_input =
      StrCat("<noscript><link rel=stylesheet href=\"", kCssUrl,
             "\"></noscript>");

  ValidateNoChanges("noscript_noinline", html_input);
}

class CssInlineAndPriotizeFilterTest : public CssInlineFilterTest {
 public:
  void SetUp() override {
    CssInlineFilterTest::SetUp();

    rewrite_driver()->set_property_page(NewMockPage(kTestDomain));
    // Set up pcache for page.
    const PropertyCache::Cohort* cohort =
        SetupCohort(page_property_cache(), RewriteDriver::kBeaconCohort);
    server_context()->set_beacon_cohort(cohort);
    page_property_cache()->Read(rewrite_driver()->property_page());
    // Set up and register a beacon finder.
    CriticalSelectorFinder* finder = new BeaconCriticalSelectorFinder(
        server_context()->beacon_cohort(), factory()->nonce_generator(),
        statistics());
    server_context()->set_critical_selector_finder(finder);
  }
};

TEST_F(CssInlineAndPriotizeFilterTest, InlineAndPrioritizeCss) {
  // Make sure we interact with Critical CSS properly, including in cached
  // case.
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kPrioritizeCriticalCss);
  rewrite_driver()->AddFilters();

  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";
  const char kMinCss[] = "div{display:block}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString html_input =
      StrCat("<link rel=stylesheet href=\"", kCssUrl, "\">");
  GoogleString html_output = StrCat("<style>", kMinCss, "</style>");

  ValidateExpected("inline_prioritize", html_input, html_output);
}

TEST_F(CssInlineFilterTest, InlineCombined) {
  // Make sure we interact with CSS combiner properly, including in cached
  // case.
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString html_input =
      StrCat("<link rel=stylesheet href=\"", kCssUrl, "\">",
             "<link rel=stylesheet href=\"", kCssUrl, "\">");
  GoogleString html_output = StrCat("<style>", kCss, "\n", kCss, "</style>");

  ValidateExpected("inline_combined", html_input, html_output);
  ValidateExpected("inline_combined", html_input, html_output);
}

TEST_F(CssInlineFilterTest, InlineMinimizeInteraction) {
  // There was a bug in async mode where we would accidentally prevent
  // minification results from rendering when inlining was not to be done.
  options()->EnableFilter(RewriteOptions::kRewriteCss);
  options()->set_css_inline_max_bytes(4);

  TestInlineCssWithOutputUrl(
      StrCat(kTestDomain, "minimize_but_not_inline.html"), "",
      StrCat(kTestDomain, "a.css"),
      // Note: Original URL was absolute, so rewritten one is as well.
      Encode(kTestDomain, "cf", "0", "a.css", "css"),
      "", /* no other attributes*/
      "div{display: none;}",
      false,
      "div{display: none}",
      "CSS not inlined since it&#39;s bigger than 4 bytes");
}

TEST_F(CssInlineFilterTest, InlineCacheExtendInteraction) {
  options()->set_css_inline_max_bytes(400);
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  ValidateExpected("inline_plus_ce", CssLinkHref(kCssUrl),
                   StrCat("<style>", kCss, "</style>"));

  // Cache extender should not have successfully produced an output on this
  // CSS, as it got inlined --- in the past it would have.
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());

  // Now try again (as this should be hitting cache hit paths for the inliner).
  ValidateExpected("inline_plus_ce", CssLinkHref(kCssUrl),
                   StrCat("<style>", kCss, "</style>"));

  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());
}

TEST_F(CssInlineFilterTest, InlineCacheExtendInteractionRepeated) {
  // As above, but also with a repeated link
  options()->set_css_inline_max_bytes(400);
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  const char kCssUrl[] = "a.css";
  const char kCss[] = "div {display:block;}";

  SetResponseWithDefaultHeaders(kCssUrl, kContentTypeCss, kCss, 3000);

  GoogleString inlined_css = StrCat("<style>", kCss, "</style>");

  ValidateExpected("inline_plus_ce_repeated",
                   StrCat(CssLinkHref(kCssUrl), CssLinkHref(kCssUrl)),
                   StrCat(inlined_css, inlined_css));

  // Cache extender should not have successfully produced an output on this
  // CSS, as it got inlined --- in the past it would have.
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());

  // Now try again (as this should be hitting cache hit paths for the inliner).
  ValidateExpected("inline_plus_ce_repeated",
                   StrCat(CssLinkHref(kCssUrl), CssLinkHref(kCssUrl)),
                   StrCat(inlined_css, inlined_css));

  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());
}


TEST_F(CssInlineFilterTest, CharsetDetermination) {
  // Sigh. rewrite_filter.cc doesn't have its own unit test so we test this
  // method here since we're the only ones that use it.
  GoogleString x_css_url = "x.css";
  GoogleString y_css_url = "y.css";
  GoogleString z_css_url = "z.css";
  const char x_css_body[] = "BODY { color: red; }";
  const char y_css_body[] = "BODY { color: green; }";
  const char z_css_body[] = "BODY { color: blue; }";
  GoogleString y_bom_body = StrCat(kUtf8Bom, y_css_body);
  GoogleString z_bom_body = StrCat(kUtf8Bom, z_css_body);

  // x.css has no charset header nor a BOM.
  // y.css has no charset header but has a BOM.
  // z.css has a charset header and a BOM.
  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_header);
  SetFetchResponse(StrCat(kTestDomain, x_css_url), default_header, x_css_body);
  SetFetchResponse(StrCat(kTestDomain, y_css_url), default_header, y_bom_body);
  default_header.MergeContentType("text/css; charset=iso-8859-1");
  SetFetchResponse(StrCat(kTestDomain, z_css_url), default_header, z_bom_body);

  ResourcePtr x_css_resource(CreateResource(kTestDomain, x_css_url));
  ResourcePtr y_css_resource(CreateResource(kTestDomain, y_css_url));
  ResourcePtr z_css_resource(CreateResource(kTestDomain, z_css_url));
  EXPECT_TRUE(ReadIfCached(x_css_resource));
  EXPECT_TRUE(ReadIfCached(y_css_resource));
  EXPECT_TRUE(ReadIfCached(z_css_resource));

  GoogleString result;
  const StringPiece kUsAsciiCharset("us-ascii");

  // Nothing set: charset should be empty.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(), "", "");
  EXPECT_TRUE(result.empty());

  // Only the containing charset is set.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(),
                                                  "", kUsAsciiCharset);
  EXPECT_STREQ(result, kUsAsciiCharset);

  // The containing charset is trumped by the element's charset attribute.
  result = RewriteFilter::GetCharsetForStylesheet(x_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("gb", result);

  // The element's charset attribute is trumped by the resource's BOM.
  result = RewriteFilter::GetCharsetForStylesheet(y_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("utf-8", result);

  // The resource's BOM is trumped by the resource's header.
  result = RewriteFilter::GetCharsetForStylesheet(z_css_resource.get(),
                                                  "gb", kUsAsciiCharset);
  EXPECT_STREQ("iso-8859-1", result);
}

TEST_F(CssInlineFilterTest, InlineWithCompatibleBom) {
  const GoogleString css = "BODY { color: red; }\n";
  const GoogleString css_with_bom = StrCat(kUtf8Bom, css);
  TestInlineCssWithOutputUrl("http://www.example.com/index.html",
                             "  <meta charset=\"UTF-8\">\n",
                             "http://www.example.com/styles.css",
                             "http://www.example.com/styles.css",
                             "", css_with_bom, true, css, "");
}

TEST_F(CssInlineFilterTest, DoNotInlineWithIncompatibleBomAndNonAscii) {
  const GoogleString css = "BODY { color: red; /* \xD2\x90 */ }\n";
  const GoogleString css_with_bom = StrCat(kUtf8Bom, css);
  TestInlineCssWithOutputUrl("http://www.example.com/index.html",
                             "  <meta charset=\"ISO-8859-1\">\n",
                             "http://www.example.com/styles.css",
                             "http://www.example.com/styles.css",
                             "", css_with_bom, false, "",
                             "CSS not inlined due to apparent charset "
                             "incompatibility; we think the HTML is ISO-8859-1 "
                             "while the CSS is utf-8");
}

TEST_F(CssInlineFilterTest, DoInlineWithIncompatibleBomAndAscii) {
  // Even though the content is labeled utf-8, it keeps to ASCII subset, so it's
  // safe to inline.
  const GoogleString css = "BODY { color: red; }\n";
  const GoogleString css_with_bom = StrCat(kUtf8Bom, css);
  TestInlineCssWithOutputUrl("http://www.example.com/index.html",
                             "  <meta charset=\"ISO-8859-1\">\n",
                             "http://www.example.com/styles.css",
                             "http://www.example.com/styles.css",
                             "", css_with_bom, true, css,
                             "");
}

// See: http://www.alistapart.com/articles/alternate/
//  and http://www.w3.org/TR/html4/present/styles.html#h-14.3.1
TEST_F(CssInlineFilterTest, AlternateStylesheet) {
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss, "a{margin:0}", 100);

  // Normal (persistent) CSS links are inlined.
  ValidateExpected(
      "persistent",
      "<link rel='stylesheet' href='foo.css'>",
      "<style>a{margin:0}</style>");

  // Make sure we accept mixed case for the keyword.
  ValidateExpected(
      "mixed_case",
      "<link rel=' StyleSheet ' href='foo.css'>",
      "<style>a{margin:0}</style>");

  // Preferred CSS links are not because inline styles cannot be given
  // a title (AFAICT).  The title attribute indicates that the given
  // CSS can be overridden by an alternate style sheet.
  ValidateNoChanges(
      "preferred",
      "<link rel='stylesheet' href='foo.css' title='foo'>");

  // Alternate CSS links, likewise.
  ValidateNoChanges(
      "alternate",
      "<link rel='alternate stylesheet' href='foo.css' title='foo'>");
}

TEST_F(CssInlineFilterTest, CarryAcrossOtherAttributes) {
  // Carry across attributes such as id and class to the inlined style tag.
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss, "a{margin:0}", 100);

  ValidateExpected(
      "CarryAcross",
      "<link rel='stylesheet' href='foo.css' id='my-stylesheet' class='a b c'"
      " lulz='!@$@#$%@4lulz'>",
      "<style id='my-stylesheet' class='a b c' lulz='!@$@#$%@4lulz'>"
      "a{margin:0}</style>");

  // But respect pagespeed_no_transform
  ValidateNoChanges(
      "NoTransform",
      "<link rel='stylesheet' href='foo.css' id='my-stylesheet' class='a b c' "
      "pagespeed_no_transform>");
  ValidateNoChanges(
      "NoTransform",
      "<link rel='stylesheet' href='foo.css' id='my-stylesheet' class='a b c' "
      "data-pagespeed-no-transform>");
}

TEST_F(CssInlineFilterTest, NoRel) {
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss, "a{margin:0}", 100);

  // We don't mess with links that lack rel attributes.
  ValidateNoChanges("no_rel", "<link href='foo.css'>");
}

TEST_F(CssInlineFilterTest, NonCss) {
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("foo.xsl", kContentTypeXml,
                                "<xsl:variable name='foo' select='bar'>", 100);

  ValidateNoChanges("non_css",
                    "<link rel='stylesheet' href='foo.xsl' type='text/xsl'/>");
}

TEST_F(CssInlineFilterTest, NoInliningOfCloseStyleTag) {
  VerifyNoInliningForClosingStyleTag("</style>");
}

TEST_F(CssInlineFilterTest, NoInliningOfCloseStyleTagWithCapitalization) {
  VerifyNoInliningForClosingStyleTag("</Style>");
}

TEST_F(CssInlineFilterTest, NoInliningOfCloseStyleTagWithSpaces) {
  VerifyNoInliningForClosingStyleTag("</style abc>");
}

TEST_F(CssInlineFilterTest, DisabledForAmp) {
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss,
                                "/* pretend there is a @font-face here */",
                                100);
  TurnOnDebug();
  ValidateExpected(
      "no_inlining_in_amp",
      "<html amp><link rel='stylesheet' href='foo.css'>",
      "<html amp><link rel='stylesheet' href='foo.css'>"
      "<!--CSS inlining not supported by PageSpeed for AMP documents-->");

  // Make sure same stylesheet gets inlined elsewhere.
  ValidateExpected(
      "same_url_in_non_amp",
      "<link rel='stylesheet' href='foo.css'>",
      "<style>/* pretend there is a @font-face here */</style>");
}

TEST_F(CssInlineFilterTest, CheckInliningOfLinkStyleTagInBodyPedantic) {
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kPedantic);
  rewrite_driver()->AddFilters();
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss,
                                "/* pretend there is a @font-face here */",
                                100);
  TurnOnDebug();

  // dont inline css for style link element in body
  // when pedantic is enabled AND move_css_to_head is not enabled.
  ValidateExpected(
      "check_inlining_for_link_tag_in_body_pedantic1",
      "<html><head></head><body><link property='stylesheet'"
      "rel='stylesheet' href='foo.css'></body></html>",
      "<html><head></head><body><link property='stylesheet' rel='stylesheet' "
      "href='foo.css'><!--CSS not inlined because style link element "
      "in html body--></body></html>");

  // inline css for style link element in head, pedantic enabled
  ValidateExpected(
      "check_inlining_for_link_tag_in_body_pedantic2",
      "<html><head><link property='stylesheet'rel='stylesheet' "
      "href='foo.css'></head><body></body></html>",
      "<html><head><style property='stylesheet' type=\"text/css\">"
      "/* pretend there is a @font-face here */</style></head>"
      "<body></body></html>");
}

TEST_F(CssInlineFilterTest, InliningOfLinkStyleTagInBody) {
  options()->EnableFilter(RewriteOptions::kInlineCss);
  options()->EnableFilter(RewriteOptions::kPedantic);
  options()->EnableFilter(RewriteOptions::kMoveCssToHead);
  rewrite_driver()->AddFilters();
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss,
                                "/* pretend there is a @font-face here */",
                                100);
  TurnOnDebug();

  // inline css for style link element in body,
  // with pedantic AND move_css_to_head enabled.
  // inlined css will be moved to head
  ValidateExpected(
      "inlining_for_link_tag_in_body",
      "<html><head></head><body><link property='stylesheet'"
      "rel='stylesheet' href='foo.css'></body></html>",
      "<html><head><style property='stylesheet' type=\"text/css\">"
      "/* pretend there is a @font-face here */</style></head>"
      "<body></body></html>");
}

TEST_F(CssInlineFilterTest, CheckInliningOfLinkStyleTagInBodyNonPedantic) {
  options()->EnableFilter(RewriteOptions::kInlineCss);
  rewrite_driver()->AddFilters();
  SetResponseWithDefaultHeaders("foo.css", kContentTypeCss,
                                "/* pretend there is a @font-face here */",
                                100);
  TurnOnDebug();

  // inline css for style link element in body
  // inlining is done in body since move_css_to_head,pedantic is not enabled.
  ValidateExpected(
      "check_inlining_for_link_tag_in_body_non_pedantic1",
      "<html><head></head><body><link property='stylesheet'"
      "rel='stylesheet' href='foo.css'></body></html>",
      "<html><head></head><body><style property='stylesheet'>"
      "/* pretend there is a @font-face here */</style></body></html>");

  // inline css for style link element in head
  // inlining is done in html head.
  ValidateExpected(
      "check_inlining_for_link_tag_in_body_non_pedantic2",
      "<html><head><link property='stylesheet'rel='stylesheet' "
      "href='foo.css'></head><body></body></html>",
      "<html><head><style property='stylesheet'>"
      "/* pretend there is a @font-face here */</style>"
      "</head><body></body></html>");
}

TEST_F(CssInlineFilterTest, BasicCsp) {
  AddFilter(RewriteOptions::kInlineCss);
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss, "a{margin:0}", 100);
  TurnOnDebug();

  const char kCspNoInline[] =
      "<meta http-equiv=\"Content-Security-Policy\" content=\"style-src *;\">";
  const char kCspYesInline[] =
      "<meta http-equiv=\"Content-Security-Policy\" "
      "content=\"style-src * 'unsafe-inline';\">";

  ValidateExpected(
      "no_inline_csp",
      StrCat(kCspNoInline, CssLinkHref("a.css")),
      StrCat(kCspNoInline, CssLinkHref("a.css"),
             "<!--PageSpeed output (by ci) not permitted by "
             "Content Security Policy-->"));
  ValidateExpected("yes_inline_csp",
                   StrCat(kCspYesInline, CssLinkHref("a.css")),
                   StrCat(kCspYesInline, "<style>a{margin:0}</style>"));
}

}  // namespace

}  // namespace net_instaweb
