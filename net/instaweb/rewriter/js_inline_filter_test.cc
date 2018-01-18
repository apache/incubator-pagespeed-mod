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


#include "net/instaweb/rewriter/public/js_inline_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

namespace {

class JsInlineFilterTest : public RewriteTestBase {
 public:
  JsInlineFilterTest() : filters_added_(false) {}

 protected:
  // TODO(matterbury): Delete this method as it should be redundant.
  virtual void SetUp() {
    RewriteTestBase::SetUp();
  }

  void TestInlineJavascript(const GoogleString& html_url,
                            const GoogleString& js_url,
                            const GoogleString& js_original_inline_body,
                            const GoogleString& js_outline_body,
                            bool expect_inline) {
    TestInlineJavascriptGeneral(
        html_url,
        "",  // don't use a doctype for these tests
        js_url,
        js_url,
        js_original_inline_body,
        js_outline_body,
        js_outline_body,  // expect ouline body to be inlined verbatim
        expect_inline,
        "");
  }

  void TestNoInlineJavascript(const GoogleString& html_url,
                              const GoogleString& js_url,
                              const GoogleString& js_original_inline_body,
                              const GoogleString& js_outline_body,
                              const GoogleString& debug_message) {
    TestInlineJavascriptGeneral(
        html_url,
        "",  // don't use a doctype for these tests
        js_url,
        js_url,
        js_original_inline_body,
        js_outline_body,
        js_outline_body,  // expect ouline body to be inlined verbatim
        false,  // Not inlined
        debug_message);
  }

  void TestInlineJavascriptXhtml(const GoogleString& html_url,
                                 const GoogleString& js_url,
                                 const GoogleString& js_outline_body,
                                 bool expect_inline) {
    TestInlineJavascriptGeneral(
        html_url,
        kXhtmlDtd,
        js_url,
        js_url,
        "",  // use an empty original inline body for these tests
        js_outline_body,
        // Expect outline body to get surrounded by a CDATA block:
        "//<![CDATA[\n" + js_outline_body + "\n//]]>",
        expect_inline,
        "");
  }

  void TestInlineJavascriptGeneral(const GoogleString& html_url,
                                   const GoogleString& doctype,
                                   const GoogleString& js_url,
                                   const GoogleString& js_out_url,
                                   const GoogleString& js_original_inline_body,
                                   const GoogleString& js_outline_body,
                                   const GoogleString& js_expected_inline_body,
                                   bool expect_inline,
                                   const GoogleString& debug_string) {
    if (!filters_added_) {
      options()->SoftEnableFilterForTesting(RewriteOptions::kInlineJavascript);
      rewrite_driver()->AddFilters();
      filters_added_ = true;
    }

    // Specify the input and expected output.
    if (!doctype.empty()) {
      SetDoctype(doctype);
    }

    const char kHtmlTemplate[] =
        "<head>\n"
        "  <script src=\"%s\">%s</script>%s\n"
        "</head>\n"
        "<body>Hello, world!</body>\n";

    const GoogleString html_input =
        StringPrintf(kHtmlTemplate, js_url.c_str(),
                     js_original_inline_body.c_str(), "");

    const GoogleString outline_html_output =
        StringPrintf(kHtmlTemplate, js_out_url.c_str(),
                     js_original_inline_body.c_str(), "");

    const GoogleString expected_output =
        (!expect_inline ? outline_html_output :
         "<head>\n"
         "  <script>" + js_expected_inline_body + "</script>\n"
         "</head>\n"
         "<body>Hello, world!</body>\n");

    const GoogleString outline_debug_html_output = debug_string.empty()
        ? outline_html_output
        : StringPrintf(kHtmlTemplate, js_out_url.c_str(),
                       js_original_inline_body.c_str(),
                       StrCat("<!--", debug_string, "-->").c_str());

    // Put original Javascript file into our fetcher.
    ResponseHeaders default_js_header;
    SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header);
    SetFetchResponse(js_url, default_js_header, js_outline_body);

    // Rewrite the HTML page.
    ValidateExpectedUrl(html_url, html_input, expected_output);

    if (!expect_inline) {
      TurnOnDebug();
      ValidateExpectedUrl(html_url, html_input,
                          outline_debug_html_output);
    }
  }

  void TurnOnDebug() {
    options()->ClearSignatureForTesting();
    options()->ForceEnableFilter(RewriteOptions::kDebug);
    server_context()->ComputeSignature(options());
  }

 private:
  bool filters_added_;
};

TEST_F(JsInlineFilterTest, DoInlineJavascriptNoMimetype) {
  // Simple case:
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function id(x) { return x; }\n",
                            true);
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptSimpleHtml) {
  SetHtmlMimetype();

  // Simple case:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       true);
}

class JsInlineFilterTestCustomOptions : public JsInlineFilterTest {
 protected:
  virtual void SetUp() {}
};

TEST_F(JsInlineFilterTestCustomOptions, InlineJsPreserveURLSOn) {
  // Make sure that we don't inline when preserve urls is on.
  options()->set_js_preserve_urls(true);
  JsInlineFilterTest::SetUp();
  SetHtmlMimetype();

  // Simple case:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptSimpleXhtml) {
  SetXhtmlMimetype();

  // Simple case:
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function id(x) { return x; }\n",
                            true);
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptWhitespace) {
  SetHtmlMimetype();

  // Whitespace between <script> and </script>:
  TestInlineJavascript("http://www.example.com/index2.html",
                       "http://www.example.com/script2.js",
                       "\n    \n  ",
                       "function id(x) { return x; }\n",
                       true);
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptDifferentDomain) {
  options()->AddInlineUnauthorizedResourceType(semantic_type::kScript);
  SetHtmlMimetype();
  TestInlineJavascript("http://www.example.net/index.html",
                       "http://scripts.example.org/script2.js",
                       "",
                       "function id(x) { return x; }\n",
                       true);
  EXPECT_EQ(1, statistics()->GetVariable(JsInlineFilter::kNumJsInlined)->Get());
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptDifferentDomain) {
  // Different domains:
  GoogleUrl gurl("http://scripts.example.org/script.js");
  TestNoInlineJavascript(
      "http://www.example.net/index.html",
      gurl.Spec().as_string(),
      "",
      "function id(x) { return x; }\n",
      rewrite_driver()->GenerateUnauthorizedDomainDebugComment(
          gurl, RewriteDriver::InputRole::kScript));
  EXPECT_EQ(0, statistics()->GetVariable(JsInlineFilter::kNumJsInlined)->Get());
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptInlineContents) {
  // Inline contents:
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "{\"json\": true}",
                       "function id(x) { return x; }\n",
                       false);
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptTooBig) {
  // Javascript too long:
  const int64 length = 2 * RewriteOptions::kDefaultJsInlineMaxBytes;
  TestNoInlineJavascript("http://www.example.com/index.html",
                         "http://www.example.com/script.js",
                         "",
                         ("function longstr() { return '" +
                          GoogleString(length, 'z') + "'; }\n"),
                         "JS not inlined since it&#39;s bigger than 2048 bytes");
}

TEST_F(JsInlineFilterTest, DoNotInlineIntrospectiveJavascriptByDefault) {
  // If it's unsafe to rename, because it contains fragile introspection like
  // $("script"), we have to leave it at the original url and not inline it.
  // Dependent on a config option that's on by default.
  TestNoInlineJavascript("http://www.example.com/index.html",
                         "http://www.example.com/script.js",
                         "",
                         "function close() { return $('script'); }\n",
                         "JS not inlined since it may be looking for "
                             "its source");
}

TEST_F(JsInlineFilterTest, DoInlineIntrospectiveJavascript) {
  options()->set_avoid_renaming_introspective_javascript(false);
  SetHtmlMimetype();

  // The same situation as DoNotInlineIntrospectiveJavascript, but in the
  // default configuration we want to be sure we're still inlining.
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function close() { return $('script'); }\n",
                       true);  // expect inlining
}

TEST_F(JsInlineFilterTest, DontInlineDisallowed) {
  SetHtmlMimetype();

  options()->Disallow("*script.js*");

  // The script is disallowed; can't be inlined.
  GoogleUrl gurl("http://www.example.com/script.js");
  TestNoInlineJavascript(
      "http://www.example.com/index.html",
      gurl.Spec().as_string(),
      "",
      "function close() { return 'inline!'; }\n",
      rewrite_driver()->GenerateUnauthorizedDomainDebugComment(
           gurl, RewriteDriver::InputRole::kScript));
}

TEST_F(JsInlineFilterTest, DoInlineDisallowedIfAllowedWhenInlining) {
  SetHtmlMimetype();
  options()->AllowOnlyWhenInlining("*script.js*");

  // The script is allowed when inlining.
  TestInlineJavascript("http://www.example.com/index.html",
                       "http://www.example.com/script.js",
                       "",
                       "function close() { return 'inline!'; }\n",
                       true);  // expect inlining
}

TEST_F(JsInlineFilterTest, DoInlineJavascriptXhtml) {
  // Simple case:
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function id(x) { return x; }\n",
                            true);
}

TEST_F(JsInlineFilterTest, DoNotInlineJavascriptXhtmlWithCdataEnd) {
  // External script contains "]]>":
  TestInlineJavascriptXhtml("http://www.example.com/index.html",
                            "http://www.example.com/script.js",
                            "function end(x) { return ']]>'; }\n",
                            false);
}

TEST_F(JsInlineFilterTest, CachedRewrite) {
  // Make sure we work fine when result is cached.
  const char kPageUrl[] = "http://www.example.com/index.html";
  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  const char kNothingInsideScript[] = "";
  SetHtmlMimetype();
  TestInlineJavascript(kPageUrl, kJsUrl, kNothingInsideScript, kJs, true);
  TestInlineJavascript(kPageUrl, kJsUrl, kNothingInsideScript, kJs, true);
}

TEST_F(JsInlineFilterTest, CachedWithSuccesors) {
  SetHtmlMimetype();

  // Regression test: in async case, at one point we had a problem with
  // slot rendering of a following cache extender trying to manipulate
  // the source attribute which the inliner deleted while using
  // cached filter results.
  SetHtmlMimetype();
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineJavascript);
  options()->SoftEnableFilterForTesting(RewriteOptions::kExtendCacheScripts);
  rewrite_driver()->AddFilters();

  const char kJsUrl[] = "script.js";
  const char kJs[] = "function id(x) { return x; }\n";

  SetResponseWithDefaultHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  GoogleString html_input = StrCat("<script src=\"", kJsUrl, "\"></script>");
  GoogleString html_output= StrCat("<script>", kJs, "</script>");

  ValidateExpected("inline_with_succ", html_input, html_output);
  ValidateExpected("inline_with_succ", html_input, html_output);
}

TEST_F(JsInlineFilterTest, CachedWithPredecessors) {
  // Regression test for crash: trying to inline after combining would crash.
  // (Current state is not to inline after combining due to the
  //  <script> element with src= being new).
  SetHtmlMimetype();
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineJavascript);
  options()->SoftEnableFilterForTesting(RewriteOptions::kCombineJavascript);
  rewrite_driver()->AddFilters();

  const char kJsUrl[] = "script.js";
  const char kJs[] = "function id(x) { return x; }\n";

  SetResponseWithDefaultHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  GoogleString html_input = StrCat("<script src=\"", kJsUrl, "\"></script>",
                                   "<script src=\"", kJsUrl, "\"></script>");

  Parse("inline_with_pred", html_input);
  Parse("inline_with_pred", html_input);
}

TEST_F(JsInlineFilterTest, InlineJs404) {
  // Test to make sure that a missing input is handled well.
  SetHtmlMimetype();
  SetFetchResponse404("404.js");
  AddFilter(RewriteOptions::kInlineJavascript);
  ValidateNoChanges("404", "<script src='404.js'></script>");

  // Second time, to make sure caching doesn't break it.
  ValidateNoChanges("404", "<script src='404.js'></script>");
}

TEST_F(JsInlineFilterTest, InlineMinimizeInteraction) {
  // There was a bug in async mode where we would accidentally prevent
  // minification results from rendering when inlining was not to be done.
  SetHtmlMimetype();
  options()->SoftEnableFilterForTesting(
      RewriteOptions::kRewriteJavascriptExternal);
  options()->SoftEnableFilterForTesting(
      RewriteOptions::kRewriteJavascriptInline);
  options()->set_js_inline_max_bytes(4);

  TestInlineJavascriptGeneral(
      StrCat(kTestDomain, "minimize_but_not_inline.html"),
      "",  // No doctype
      StrCat(kTestDomain, "a.js"),
      // Note: Original URL was absolute, so rewritten one is as well.
      Encode(kTestDomain, "jm", "0", "a.js", "js"),
      "",  // No inline body in,
      "var answer = 42; // const is non-standard",  // out-of-line body
      "",  // No inline body out,
      false,  // Not inlining
      "JS not inlined since it&#39;s bigger than 4 bytes");
}

TEST_F(JsInlineFilterTest, ScriptWithScriptTags) {
  SetHtmlMimetype();
  AddFilter(RewriteOptions::kInlineJavascript);

  ResponseHeaders default_js_header;
  SetDefaultLongCacheHeaders(&kContentTypeJavascript, &default_js_header);
  GoogleString js_url = StrCat(kTestDomain, "a.js");
  SetFetchResponse(js_url,
                   default_js_header,
                   "alert('<script></script>');"
                   "alert('<sCrIpT></ScRiPt>');"
                   "alert('</SCRIPT foo>');"
                   "alert('<Script</sCRIPT');"
                   "alert('</scr>');");

  // a.js now contains a script that needs escaping to inline.

  ValidateExpectedUrl(
      StrCat(kTestDomain, "inline_with_close_script.html"),

      // Input, with js referenced externally.
      StringPrintf(
          "<head>\n"
          "  <script src='%s'></script>\n"
          "</head>\n"
          "<body>Hello, world!</body>\n",
          js_url.c_str()),

      // Expected output, with js inlined and escaped.
      "<head>\n"
      "  <script>alert('<\\u0073cript></\\u0073cript>');"
      "alert('<\\u0073CrIpT></\\u0053cRiPt>');"
      "alert('</\\u0053CRIPT foo>');"
      "alert('<\\u0053cript</\\u0073CRIPT');"
      "alert('</scr>');</script>\n"
      "</head>\n"
      "<body>Hello, world!</body>\n");
}

TEST_F(JsInlineFilterTest, FlushSplittingScriptTag) {
  SetHtmlMimetype();
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineJavascript);
  rewrite_driver()->AddFilters();
  SetupWriter();

  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  SetResponseWithDefaultHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  html_parse()->StartParse("http://www.example.com");
  html_parse()->ParseText("<div><script src=\"script.js\"> ");
  html_parse()->Flush();
  html_parse()->ParseText("</script> </div>");
  html_parse()->FinishParse();
  EXPECT_STREQ("<div><script>function id(x) { return x; }\n</script> </div>",
               output_buffer_);
}

TEST_F(JsInlineFilterTest, NoFlushSplittingScriptTag) {
  SetHtmlMimetype();
  options()->SoftEnableFilterForTesting(RewriteOptions::kInlineJavascript);
  rewrite_driver()->AddFilters();
  SetupWriter();

  const char kJsUrl[] = "http://www.example.com/script.js";
  const char kJs[] = "function id(x) { return x; }\n";
  SetResponseWithDefaultHeaders(kJsUrl, kContentTypeJavascript, kJs, 3000);

  html_parse()->StartParse("http://www.example.com");
  html_parse()->ParseText("<div><script src=\"script.js\">     ");
  html_parse()->ParseText("     </script> </div>");
  html_parse()->FinishParse();
  EXPECT_STREQ("<div><script>function id(x) { return x; }\n</script> </div>",
               output_buffer_);
}

TEST_F(JsInlineFilterTest, BasicCsp) {
  SetHtmlMimetype();
  AddFilter(RewriteOptions::kInlineJavascript);
  EnableDebug();

  static const char kJs[] = "function id(x) { return x; }\n";
  SetResponseWithDefaultHeaders("script.js", kContentTypeJavascript, kJs, 3000);

  static const char kCspNoInline[] =
      "<meta http-equiv=\"Content-Security-Policy\" content=\"script-src *;\">";
  static const char kCspYesInline[] =
      "<meta http-equiv=\"Content-Security-Policy\" "
      "content=\"script-src * 'unsafe-inline';\">";

  ValidateExpected("no_inline_csp",
                   StrCat(kCspNoInline, "<script src=script.js></script>"),
                   StrCat(kCspNoInline, "<script src=script.js></script>"
                          "<!--PageSpeed output (by ji) not permitted by "
                          "Content Security Policy-->"));
  ValidateExpected(
      "inline_csp",
      StrCat(kCspYesInline, "<script src=script.js></script>"),
      StrCat(kCspYesInline, "<script>function id(x) { return x; }\n</script>"));
}

}  // namespace

}  // namespace net_instaweb
