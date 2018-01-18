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


#include <cstddef>
#include "base/logging.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kTopCssFile[] = "assets/styles.css";
const char kOneLevelDownFile1[] = "assets/nested1.css";
const char kOneLevelDownFile2[] = "assets/nested2.css";
const char kTwoLevelsDownFile1[] = "assets/nested/nested1.css";
const char kTwoLevelsDownFile2[] = "assets/nested/nested2.css";
const char k404CssFile[] = "404.css";
const char kSimpleCssFile[] = "simple.css";
const char kComplexCssFile[] = "complex.css";

// Contents of resource files. Already minimized. NOTE relative paths!
static const char kTwoLevelsDownContents1[] =
    ".background_cyan{background-color:#0ff}"
    ".foreground_pink{color:#ffc0cb}";
static const char kTwoLevelsDownContents2[] =
    ".background_green{background-color:#0f0}"
    ".foreground_rose{color:rose}";
static const char kOneLevelDownCss1[] =
    ".background_blue{background-color:#00f}"
    ".foreground_gray{color:gray}";
static const char kOneLevelDownCss2[] =
    ".background_white{background-color:#fff}"
    ".foreground_black{color:#000}";
static const char kTopCss[] =
    ".background_red{background-color:red}"
    ".foreground_yellow{color:#ff0}";
static const char kSimpleCss[] =
    ".background_red{background-color:red}"
    ".foreground_yellow{color:#ff0}";
static const char kComplexCss[] =
    "  @media screen and (min-width: 240px) {"
    "  .background_red{background-color:red}"
    "}";

class CssFlattenImportsTest : public CssRewriteTestBase {
 protected:
  CssFlattenImportsTest() {
    kOneLevelDownContents1 = StrCat(
        "@import url(nested/nested1.css);",
        kOneLevelDownCss1);
    kOneLevelDownContents2 = StrCat(
        "@import url(nested/nested2.css);",
        kOneLevelDownCss2);
    kTopCssContents = StrCat(
        "@import url(nested1.css);",
        "@import url(nested2.css);",
        kTopCss);
    kFlattenedTopCssContents = StrCat(
        kTwoLevelsDownContents1, kOneLevelDownCss1,
        kTwoLevelsDownContents2, kOneLevelDownCss2,
        kTopCss);
    kFlattenedOneLevelDownContents1 = StrCat(
        kTwoLevelsDownContents1,
        kOneLevelDownCss1);
  }

  virtual void SetUpFilters() {
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->EnableFilter(RewriteOptions::kExtendCacheImages);
    options()->set_always_rewrite_css(true);
    rewrite_driver()->AddFilters();
  }

  virtual void SetUpResponses() {
    SetResponseWithDefaultHeaders(kTopCssFile, kContentTypeCss,
                                  kTopCssContents, 100);
    SetResponseWithDefaultHeaders(kOneLevelDownFile1, kContentTypeCss,
                                  kOneLevelDownContents1, 100);
    SetResponseWithDefaultHeaders(kOneLevelDownFile2, kContentTypeCss,
                                  kOneLevelDownContents2, 100);
    SetResponseWithDefaultHeaders(kTwoLevelsDownFile1, kContentTypeCss,
                                  kTwoLevelsDownContents1, 100);
    SetResponseWithDefaultHeaders(kTwoLevelsDownFile2, kContentTypeCss,
                                  kTwoLevelsDownContents2, 100);
    SetResponseWithDefaultHeaders(kComplexCssFile, kContentTypeCss,
                                  kComplexCss, 100);
    SetFetchResponse404(k404CssFile);
  }

  virtual void SetUp() {
    // We don't use the parent class setup, because we want to make sure that
    // RewriteCss is enabled implicitly by enabling FlattenCssImports.  We
    // skip to the setup for the parent of our parent class.
    RewriteTestBase::SetUp();
    SetUpFilters();
    SetUpResponses();
  }

  // General routine to test flattening of nested resources referenced with
  // relative (trim_urls == true) or absolute (trim_urls == false) paths and
  // optional post-flattening cache extension (cache_extend == true).
  void TestFlattenNested(bool trim_urls, bool cache_extend) {
    // /foo.png
    const char kFooPngFilename[] = "foo.png";
    const char kImageData[] = "Invalid PNG but does not matter for this test";
    SetResponseWithDefaultHeaders(kFooPngFilename, kContentTypePng,
                                  kImageData, 100);
    GoogleString foo_domain(trim_urls ? "" : kTestDomain);
    GoogleString foo_path =
        (cache_extend
         ? Encode(foo_domain, "ce", "0", kFooPngFilename, "png")
         : StrCat(foo_domain, kFooPngFilename));

    // /image1.css loads /foo.png as a background image.
    const char kCss1Filename[] = "image1.css";
    const GoogleString css1_before =
        StrCat("body {\n"
               "  background-image: url(", kFooPngFilename, ");\n"
               "}\n");
    const GoogleString css1_after =
        StrCat("body{background-image:url(", foo_path, ")}");
    SetResponseWithDefaultHeaders(kCss1Filename, kContentTypeCss,
                                  css1_before, 100);

    // /nested/bar.png
    const char kBarPngFilename[] = "bar.png";
    SetResponseWithDefaultHeaders(StrCat("nested/", kBarPngFilename),
                                  kContentTypePng, kImageData, 100);
    GoogleString bar_domain(trim_urls ? "nested/" :
                            StrCat(kTestDomain, "nested/"));
    GoogleString bar_path =
        (cache_extend
         ? Encode(bar_domain, "ce", "0", kBarPngFilename, "png")
         : StrCat(bar_domain, kBarPngFilename));

    // /nested/image2.css loads /nested/bar.png & /foo.png as background images.
    const char kCss2Filename[] = "nested/image2.css";  // because its CSS is!
    const GoogleString css2_before =
        StrCat("body {\n"
               "  background-image: url(", kBarPngFilename, ");\n"
               "}\n"
               "div {\n"
               "  background-image: url(../", kFooPngFilename, ");\n"
               "}\n");
    const GoogleString css2_after =
        StrCat("body{background-image:url(", bar_path, ")}"
               "div{background-image:url(", foo_path, ")}");
    SetResponseWithDefaultHeaders(kCss2Filename, kContentTypeCss,
                                  css2_before, 100);

    // /foo-then-bar.css @imports /nested/image1.css then /nested/image2.css
    const char kTop1CssFilename[] = "foo-then-bar.css";
    const GoogleString top1_before =
        StrCat("@import url(", kCss1Filename, ");",
               "@import url(", kCss2Filename, ");");
    const GoogleString top1_after = StrCat(css1_after, css2_after);
    SetResponseWithDefaultHeaders(kTop1CssFilename, kContentTypeCss,
                                  top1_before, 100);

    // /bar-then-foo.css @imports /nested/image2.css then /nested/image1.css
    const char kTop2CssFilename[] = "bar-then-foo.css";
    const GoogleString top2_before =
        StrCat("@import url(", kCss2Filename, ");",
               "@import url(", kCss1Filename, ");");
    const GoogleString top2_after = StrCat(css2_after, css1_after);
    SetResponseWithDefaultHeaders(kTop2CssFilename, kContentTypeCss,
                                  top2_before, 100);

    // Phew! Load them both. bar-then-foo.css should use cached data.
    ValidateRewriteExternalCss("flatten_then_cache_extend_nested1",
                               top1_before, top1_after,
                               kExpectSuccess | kNoClearFetcher);
    ValidateRewriteExternalCss("flatten_then_cache_extend_nested2",
                               top2_before, top2_after,
                               kExpectSuccess | kNoClearFetcher);
  }

  // General routine to test that we flatten -then- cache extend the PNG in
  // the resulting CSS while absolutifying the PNGs' URLs while flattening
  // then [not] relativizing them while rewriting them.
  void TestCacheExtendsAfterFlatteningNested(bool trim_urls) {
    TestFlattenNested(trim_urls, true);
  }

  // General routine to test charset handling. The header_charset argument
  // specifies the charset we stick into the HTML page's headers, if any, while
  // the meta_tag_charset and http_equiv_charset arguments specify the charset
  // we stick into a meta tag in the <head> element; these control the charset
  // of the HTML page that starts the flattening import. The imported css files
  // all specify @charset utf-8, and the default HTML charset, if none is
  // specified by one of these arguments, is iso-8859-1, so, unless the result
  // is for a HTML charset of utf-8, the test will fail. The bool says whether
  // we expect to succeed.
  void TestFlattenWithHtmlCharset(const StringPiece& header_charset,
                                  const StringPiece& meta_tag_charset,
                                  const StringPiece& http_equiv_charset,
                                  bool should_succeed) {
    const char kStylesFilename[] = "styles.css";
    const GoogleString kStylesContents = StrCat(
        "@charset \"UTF-8\";",
        "@import url(print.css);",
        "@import url(screen.css);",
        kSimpleCss);

    // Next block is a reimplementation of SetResponseWithDefaultHeaders()
    // but setting the charset in the Content-Type header.
    GoogleString url = AbsolutifyUrl(kStylesFilename);
    int64 ttl_sec = 100;
    ResponseHeaders response_headers;
    DefaultResponseHeaders(kContentTypeCss, ttl_sec, &response_headers);
    response_headers.Replace(HttpAttributes::kContentType,
                             "text/css; charset=utf-8");
    response_headers.ComputeCaching();
    SetFetchResponse(url, response_headers, kStylesContents);

    // Now we set the charset in the driver headers which is how we as a test
    // program set the HTML's charset.
    ResponseHeaders driver_headers;
    if (!header_charset.empty()) {
      driver_headers.Add(HttpAttributes::kContentType,
                         StrCat("text/css; charset=", header_charset));
    }
    ValidationFlags meta_tag_flag = kNoFlags;
    if (!meta_tag_charset.empty()) {
      if (meta_tag_charset == "utf-8") {
        meta_tag_flag = kMetaCharsetUTF8;
      } else if (meta_tag_charset == "iso-8859-1") {
        meta_tag_flag = kMetaCharsetISO88591;
      } else {
        DCHECK(meta_tag_charset == "utf-8" || meta_tag_charset == "iso-8859-1");
      }
      DCHECK(http_equiv_charset.empty());
    }
    if (!http_equiv_charset.empty()) {
      if (http_equiv_charset == "utf-8") {
        meta_tag_flag = kMetaHttpEquiv;
      } else if (http_equiv_charset == "iso-8859-1") {
        meta_tag_flag = kMetaHttpEquivUnquoted;
      } else {
        DCHECK(http_equiv_charset == "utf-8" ||
               http_equiv_charset == "iso-8859-1");
      }
      DCHECK(meta_tag_charset.empty());
    }
    driver_headers.ComputeCaching();
    rewrite_driver()->set_response_headers_ptr(&driver_headers);

    const char kPrintFilename[] = "print.css";
    const char kPrintCss[] =
        ".background_cyan{background-color:#0ff}"
        ".foreground_pink{color:#ffc0cb}";
    const GoogleString kPrintContents = kPrintCss;
    SetResponseWithDefaultHeaders(kPrintFilename, kContentTypeCss,
                                  kPrintContents, 100);

    const char kScreenFilename[] = "screen.css";
    const char kScreenCss[] =
        ".background_blue{background-color:#00f}"
        ".foreground_gray{color:gray}";
    const GoogleString kScreenContents = StrCat(
        "@charset \"UtF-8\";",
        kScreenCss);
    SetResponseWithDefaultHeaders(kScreenFilename, kContentTypeCss,
                                  kScreenContents, 100);

    const char css_in[] = "@import url(http://test.com/styles.css);";
    if (should_succeed) {
      const GoogleString css_out = StrCat(kPrintCss, kScreenCss, kSimpleCss);

      // TODO(sligocki): Why do we need kNoOtherContexts here?
      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_out,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
      // Check things work when data is already cached.
      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_out,
                                 kExpectCached | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
    } else {
      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_in,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag |
                                 kFlattenImportsCharsetMismatch);
      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_in,
                                 kExpectCached | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
    }
  }

  // Test the css_flatten_max_bytes() setting.
  void TestLimit(const StringPiece test_id,
                 bool limit_exceeded,
                 int flattening_limit,
                 int actual_amount,
                 const GoogleString& css_in,
                 const GoogleString& css_out) {
    options()->ClearSignatureForTesting();
    options()->set_css_flatten_max_bytes(flattening_limit);
    server_context()->ComputeSignature(options());

    SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                  kSimpleCss, 100);

    ValidationFlags extra_flag = kNoFlags;
    if (limit_exceeded) {
      extra_flag = kFlattenImportsLimitExceeded;
      DebugWithMessage(StringPrintf("<!--Flattening failed: "
                                    "Flattening limit (%d) exceeded (%d)-->",
                                    flattening_limit, actual_amount));
    } else {
      DebugWithMessage("");
    }

    ValidateRewriteExternalCss(test_id, css_in, css_out,
                               kExpectSuccess | kNoClearFetcher | extra_flag);
    // We do not specify kNoClearFetcher, so the fetcher is cleared. Thus,
    // content must be pulled from the cache. kNoOtherContexts because
    // other contexts won't have this value cached.
    ValidateRewriteExternalCss(test_id, css_in, css_out,
                               kExpectCached | kNoOtherContexts | extra_flag);
  }

  // Test relative URLs in CSS that itself is referenced via a relative URL.
  void TestRelativeImageUrlInRelativeCssUrl(bool trim_urls, bool cache_extend) {
    // Setup the image we refer to.
    const char kFooPng[] = "images/foo.png";
    const GoogleString foo_png_path = StrCat(kTestDomain, "a/", kFooPng);
    const char kImageData[] = "Invalid PNG but does not matter for this test";
    SetResponseWithDefaultHeaders(foo_png_path, kContentTypePng,
                                  kImageData, 100);
    // Setup the CSS that refers to it.
    const char kSimpleCssTemplate[] =
        ".background_red{background-color:red}"
        ".foreground_yellow{color:#ff0}"
        ".body{background-image:url(%s)}";
    // The input CSS refers to ../images/test.jpg from the file /a/b/simple.css,
    // so the image's path is /a/images/test.jpg, which is what should be used
    // when the CSS is flattened into the base document (with base of '/').
    const GoogleString simple_css_path =
        StrCat(kTestDomain, "a/b/", kSimpleCssFile);
    const GoogleString relative_simple_css_in =
        StringPrintf(kSimpleCssTemplate, StrCat("../", kFooPng).c_str());
    SetResponseWithDefaultHeaders(simple_css_path, kContentTypeCss,
                                  relative_simple_css_in, 100);
    const GoogleString import_simple_css =
        StrCat("@import url(", simple_css_path, ");");
    const GoogleString foo_png_output =
        (cache_extend
         ? Encode(StrCat(trim_urls ? "" : kTestDomain, "a/images/"),
                  "ce", "0", "foo.png", "png")
         : StrCat(trim_urls ? "" : kTestDomain, "a/", kFooPng));
    const GoogleString simple_css_out =
        StringPrintf(kSimpleCssTemplate, foo_png_output.c_str());
    ValidateRewriteInlineCss("flatten_relative",
                             import_simple_css, simple_css_out,
                             kExpectSuccess);
  }

  GoogleString kOneLevelDownContents1;
  GoogleString kOneLevelDownContents2;
  GoogleString kTopCssContents;
  GoogleString kFlattenedTopCssContents;
  GoogleString kFlattenedOneLevelDownContents1;
};

TEST_F(CssFlattenImportsTest, FlattenInlineCss) {
  const char css_in[] =
      "@import url(http://test.com/simple.css);";

  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                kSimpleCss, 100);

  ValidateRewriteInlineCss("flatten_simple", css_in, kSimpleCss,
                           kExpectSuccess);
  // TODO(sligocki): This suggests that we grew the number of bytes, which is
  // misleading because originally, the user would have loaded both files
  // and now they will only load one. So total bytes are less.
  // I think this should be listing bytes saved as STATIC_STRLEN(css_in).
  int64 expected_savings =
      static_cast<int64>(STATIC_STRLEN(css_in)) -
      static_cast<int64>(STATIC_STRLEN(kSimpleCss));
  EXPECT_EQ(expected_savings, total_bytes_saved_->Get());
}

TEST_F(CssFlattenImportsTest, DontFlattenAttributeCss) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  server_context()->ComputeSignature(options());

  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                kSimpleCss, 100);

  // Test that rewriting of attributes is enabled and working.
  ValidateExpected("rewrite-attribute-setup",
                   "<div style='background-color: #f00; color: yellow;'/>",
                   "<div style='background-color:red;color:#ff0'/>");

  // Test that we don't rewrite @import's in attributes since that's invalid.
  ValidateNoChanges("rewrite-attribute",
                    "<div style='@import url(http://test.com/simple.css)'/>");
}

TEST_F(CssFlattenImportsTest, FlattenNoop) {
  ValidateRewriteExternalCss("flatten_noop", kSimpleCss, kSimpleCss,
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, Flatten404) {
  DebugWithMessage("<!--4xx status code, preventing rewriting of"
                   " http://test.com/404.css-->");
  const char css_in[] =
      "@import url(http://test.com/404.css);";

  ValidateRewriteExternalCss("flatten_404", css_in, css_in,
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, DontFlattenWithUnauthorizedCSS) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  const char kFailureReason[] = "<!--Flattening failed: Cannot import "
                                "http://unauth.com/assets/styles.css "
                                "as it is on an unauthorized domain-->";
  DebugWithMessage(kFailureReason);
  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                kSimpleCss, 100);
  const char kUnauthorizedImportCss[] =
      "@import url(http://unauth.com/assets/styles.css);\n"
      "@import url(http://test.com/simple.css);\n"
      "a { color:red }";
  const char kRewrittenUnauthorizedImportCss[] =
      "@import url(http://unauth.com/assets/styles.css);"
      "@import url(http://test.com/simple.css);"
      "a{color:red}";
  ValidateRewriteExternalCss("dont_flatten_unauthorized_css_import",
      kUnauthorizedImportCss, kRewrittenUnauthorizedImportCss,
      kExpectSuccess | kNoClearFetcher);

  const char kAuthorizedTopLevelCss[] =
      "@import url(auth_parent_with_unauth_child_import.css);"
      "b { color: blue }";
  const char kRewrittenAuthorizedTopLevelCss[] =
      "@import url(auth_parent_with_unauth_child_import.css);"
      "b{color:#00f}";
  SetResponseWithDefaultHeaders("auth_parent_with_unauth_child_import.css",
                                kContentTypeCss,
                                kUnauthorizedImportCss, 100);
  ValidateRewriteExternalCss("dont_flatten_nested_unauthorized_css_import",
      kAuthorizedTopLevelCss, kRewrittenAuthorizedTopLevelCss,
      kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenInvalidCSS) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--CSS rewrite failed: Parse error in %url%-->");
  const char kInvalidMediaCss[] = "@media }}";
  ValidateRewriteExternalCss("flatten_invalid_css_media",
                             kInvalidMediaCss, kInvalidMediaCss,
                             kExpectFailure);

  const char kFilename[] = "styles.css";
  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, kSimpleCss, 100);

  // This gets a parse error but thanks to the idea of "unparseable sections"
  // in the CSS parser it's not treated as an error and the "bad" text is kept.
  // Because the error was in the bogus @import statement, we do NOT flatten.
  DebugWithMessage("");
  const char kUnparseableImportCss[] = "@import styles.css; a { color:red }";
  const char kFlattenedImportCss[] = "@import styles.css;a{color:red}";
  ValidateRewriteExternalCss("flatten_unparseable_css_import",
                             kUnparseableImportCss, kFlattenedImportCss,
                             kExpectSuccess | kNoClearFetcher);

  // Same as above, but since the @import itself is valid we DO flatten.
  const char kUnparseableCss[] = "@import url(styles.css) ;a{ #color: 333 }";
  GoogleString kFlattenedInvalidCss = StrCat(kSimpleCss, "a{#color: 333 }");

  DebugWithMessage("");
  ValidateRewriteExternalCss("flatten_unparseable_css_rule",
                             kUnparseableCss, kFlattenedInvalidCss,
                             kExpectSuccess | kNoClearFetcher);

  // This gets a non-recoverable parse error because of mismatched {}s.
  // We do not want to recover from these types of parse errors because
  // combining/flattening files like this would spread the breakage.
  // Note: This specific case is probably technically safe to flatten
  // because the broken CSS is at the end, but we choose not to dance
  // on that knife edge and just disallow flattening.
  DebugWithMessage("<!--CSS rewrite failed: Parse error in %url%-->");
  const char kErrorCss[] = "@import url(styles.css);a{{ color:red }";
  ValidateRewriteExternalCss("no_flatten_error_css_rule",
                             kErrorCss, kErrorCss,
                             kExpectFailure | kNoClearFetcher);

  // Make sure we don't flatten if the @imported CSS has a non-recoverable
  // parse error.
  SetResponseWithDefaultHeaders("error.css", kContentTypeCss,
                                "a {{ color: red }", 100);
  const char kImportErrorCss[] =
      "@import url(error.css); body { color: #000 }";
  const char kRewrittenImportErrorCss[] =
      "@import url(error.css);body{color:#000}";
  // Note: Rewrite succeeds, but flatten fails.
  // TODO(jmaessen): Should contain parse error for nested file as well,
  // once nested rewrites propagate correctly.
  DebugWithMessage("<!--Flattening failed: Cannot parse the CSS "
                   "in http://test.com/error.css-->");
  ValidateRewriteExternalCss("no_flatten_error_in_import",
                             kImportErrorCss, kRewrittenImportErrorCss,
                             kExpectSuccess | kFlattenImportsMinifyFailed |
                             kNoClearFetcher);
  // Note that this is not run as ValidateRerwite(), because that would re-use
  // the rewritten value and thus not increment kFlattenImportsMinifyFailed
  // the second time.
}

TEST_F(CssFlattenImportsTest, FlattenEmptyMedia) {
  ValidateRewriteExternalCss("flatten_empty_media",
                             "@media {}", "", kExpectSuccess);
}

TEST_F(CssFlattenImportsTest, FlattenSimple) {
  const char css_in[] =
      "@import url(http://test.com/simple.css);";

  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                kSimpleCss, 100);

  ValidateRewriteExternalCss("flatten_simple", css_in, kSimpleCss,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_simple", css_in, kSimpleCss,
                             kExpectCached | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenUnderLargeLimit) {
  // The default limit is 2k, large enough to flatten everything into.
  // Note that the top level CSS is not minified on input but is on output.
  const char css_in[] =
      "@import url(http://test.com/simple.css);\n"
      "@import url(http://test.com/simple.css);\n";
  const GoogleString css_out = StrCat(kSimpleCss, kSimpleCss);

  TestLimit("flatten_under_limit", false /* limit_exceeded */,
            1 + css_out.size() /* flattening_limit */,
            0 /* no limit exceeded comment */,
            css_in, css_out);
}

TEST_F(CssFlattenImportsTest, DontFlattenOverMediumLimit) {
  // This limit will result in simple.css being flattened OK, but the outer
  // CSS that @imports it twice won't fit so flattening will fail.
  // Note that the top level CSS is not minified on input but is on output.
  const char css_in[] =
      "@import url(http://test.com/simple.css);\n"
      "@import url(http://test.com/simple.css);\n";
  const char css_out[] =
      "@import url(http://test.com/simple.css);"
      "@import url(http://test.com/simple.css);";

  TestLimit("dont_flatten_over_limit", true /* limit_exceeded */,
            1 + STATIC_STRLEN(css_out) /* flattening_limit */,
            54 + STATIC_STRLEN(css_out) /* actual_amount */,
            css_in, css_out);
}

TEST_F(CssFlattenImportsTest, DontFlattenOverTinyLimit) {
  // This limit will result in even simple.css not being flattened.
  // Note that the top level CSS is not minified on input but is on output.
  const char css_in[] =
      "@import url(http://test.com/simple.css);\n"
      "@import url(http://test.com/simple.css);\n";
  const char css_out[] =
      "@import url(http://test.com/simple.css);"
      "@import url(http://test.com/simple.css);";

  TestLimit("dont_flatten_over_tiny_limit", true /* limit_exceeded */,
            10 /* flattening_limit */, 67 /* actual_amount */,
            css_in, css_out);
}

TEST_F(CssFlattenImportsTest, FlattenEmpty) {
  // We intentionally do not inline any empty resources.
  const char kFilename[] = "empty.css";
  const char css_in[] = "@import url(http://test.com/empty.css);";
  const char empty_content[] = "";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, empty_content, 100);

  ValidateRewriteExternalCss("flatten_empty", css_in, css_in,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  // We do not specify kNoClearFetcher, so the fetcher is cleared. Thus,
  // content must be pulled from the cache. kNoOtherContexts because
  // other contexts won't have this value cached.
  ValidateRewriteExternalCss("flatten_empty", css_in, css_in,
                             kExpectCached | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenSimpleRewriteOnTheFly) {
  // import.css @import's simple.css
  // simple.css contains some simple CSS
  // Fetch the rewritten filename of import.css and we should get the
  // flattened and minimized contents, namely simple.css's contents.

  const char kImportFilename[] = "import.css";
  const char css_import[] =
      "@import url(http://test.com/simple.css);";
  SetResponseWithDefaultHeaders(kImportFilename, kContentTypeCss,
                                css_import, 100);

  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss,
                                kSimpleCss, 100);

  // Check that nothing is up my sleeve ...
  EXPECT_EQ(0, lru_cache()->num_elements());
  EXPECT_EQ(0, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());

  GoogleString content;
  EXPECT_TRUE(FetchResource(kTestDomain, RewriteOptions::kCssFilterId,
                            "import.css", "css", &content));
  EXPECT_EQ(kSimpleCss, content);

  // Check for 6 misses and 6 inserts giving 6 elements at the end:
  // 3 URLs (import.css/simple.css/rewritten) x 2 (partition key + contents).
  EXPECT_EQ(6, lru_cache()->num_elements());
  EXPECT_EQ(6, lru_cache()->num_inserts());
  EXPECT_EQ(5, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
}

TEST_F(CssFlattenImportsTest, FlattenNested) {
  const GoogleString css_in = StrCat("@import url(http://test.com/",
                                     kTopCssFile, ");");

  ValidateRewriteExternalCss("flatten_nested",
                             css_in, kFlattenedTopCssContents,
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenFromCacheDirectly) {
  // Prime the pumps by loading all the CSS files into the cache.
  // Verifying that the resources fetched below _are_ cached is non-trivial
  // because they are stored against their partition key and determining that
  // from this level requires access to and reimplementation of the inner
  // working of RewriteContext and various sub-classes. At the time of writing
  // I verified in the debugger that they are cached.
  GoogleString css_in = StrCat("@import url(http://test.com/",
                               kTopCssFile, ");");
  ValidateRewriteExternalCss("flatten_from_cache_directly",
                             css_in, kFlattenedTopCssContents,
                             kExpectSuccess | kNoClearFetcher);

  // Check cache activity: everything cached has been inserted, no reinserts,
  // no deletes. Then note values we check against below.
  EXPECT_EQ(lru_cache()->num_elements(), lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  size_t num_elements = lru_cache()->num_elements();
  ClearStats();

  // Check things work when data is already cached, though the stats are
  // messed up because we don't do any actual rewriting in that instance:
  // num_blocks_rewritten_->Get() == 0 instead of 1
  // total_bytes_saved_->Get() == 0 instead of negative something.
  //
  // We do not specify kNoClearFetcher, so the fetcher is cleared. Thus,
  // content must be pulled from the cache. kNoOtherContexts because
  // other contexts won't have this value cached.
  ValidateRewriteExternalCss("flatten_from_cache_directly",
                             css_in, kFlattenedTopCssContents,
                             kExpectCached | kNoOtherContexts);

  // Check that everything was read from the cache.
  EXPECT_EQ(num_elements, lru_cache()->num_elements());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_hits());
  ClearStats();
  num_elements = lru_cache()->num_elements();

  // Access one of the cached ones directly.
  //
  // We do not specify kNoClearFetcher, so the fetcher is cleared. Thus,
  // content must be pulled from the cache. kNoOtherContexts because
  // other contexts won't have this value cached.
  css_in = StrCat("@import url(http://test.com/", kTwoLevelsDownFile1, ");");
  ValidateRewriteExternalCss("flatten_from_cache_directly_repeat",
                             css_in, kTwoLevelsDownContents1,
                             kExpectSuccess | kNoOtherContexts);

  // The sequence in this case, for the new external link (_repeat on the end):
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // HIT    for the already-cached kTwoLevelsDownFile1's partition key.
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //     ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 3 new elements, 2 new misses, 2 new hits.
  EXPECT_EQ(num_elements + 3, lru_cache()->num_elements());
  EXPECT_EQ(2, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_hits());
}

TEST_F(CssFlattenImportsTest, FlattenFromCacheIndirectly) {
  // Prime the pumps by loading all the CSS files into the cache.
  // Verifying that the resources fetched below _are_ cached is non-trivial
  // because they are stored against their partition key and determining that
  // from this level requires access to and reimplementation of the inner
  // working of RewriteContext and various sub-classes. At the time of writing
  // I verified in the debugger that they are cached.
  GoogleString css_in = StrCat("@import url(http://test.com/",
                               kTopCssFile, ");");
  ValidateRewriteExternalCss("flatten_from_cache_indirectly",
                             css_in, kFlattenedTopCssContents,
                             kExpectSuccess | kNoClearFetcher);

  // Check cache activity: everything cached has been inserted, no reinserts,
  // no deletes. Then note values we check against below.
  EXPECT_EQ(lru_cache()->num_elements(), lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  size_t num_elements = lru_cache()->num_elements();
  ClearStats();

  // Access one of the cached ones from a different file (via @import).
  const char filename[] = "alternative.css";
  css_in = StrCat("@import url(http://test.com/", filename, ");");
  GoogleString contents = StrCat("@import url(", kOneLevelDownFile1, ");");
  SetResponseWithDefaultHeaders(filename, kContentTypeCss, contents, 100);
  ValidateRewriteExternalCss("flatten_from_cache_indirectly_repeat",
                             css_in, kFlattenedOneLevelDownContents1,
                             kExpectSuccess | kNoClearFetcher);

  // The sequence in this case, for the new external link (_repeat on the end):
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // MISS   for alternative.css's partition key.
  // MISS   for alternative.css's URL.
  // INSERT for the fetched alternative.css.
  // HIT    for the already-cached kOneLevelDownFile1's partition key.
  // INSERT for the rewritten alternative.css's URL.
  // INSERT for the rewritten alternative.css's partition key.
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //     ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 6 new elements, 4 new misses, 2 new hits.
  EXPECT_EQ(num_elements + 6, lru_cache()->num_elements());
  EXPECT_EQ(4, lru_cache()->num_misses());
  // TODO(matterbury):  In 100 runs this was right 97 times but 3 times it
  // was +4 not +2. I don't know why and don't especially care right now.
  EXPECT_LE(2, lru_cache()->num_hits());
}

TEST_F(CssFlattenImportsTest, CacheExtendsAfterFlattening) {
  // Check that we flatten -then- cache extend the PNG in the resulting CSS.
  const char kCssFilename[] = "image.css";
  const GoogleString css_before =
      "body {\n"
      "  background-image: url(foo.png);\n"
      "}\n";
  const GoogleString css_after =
      StrCat("body{background-image:url(",
             Encode("", "ce", "0", "foo.png", "png"),
             ")}");
  SetResponseWithDefaultHeaders(kCssFilename, kContentTypeCss, css_before, 100);

  const char kFooPngFilename[] = "foo.png";
  const char kImageData[] = "Invalid PNG but it does not matter for this test";
  SetResponseWithDefaultHeaders(kFooPngFilename, kContentTypePng,
                                kImageData, 100);

  ValidateRewriteExternalCss("flatten_then_cache_extend",
                             css_before, css_after,
                             kExpectSuccess | kNoClearFetcher);

  // Test when everything is already cached.
  ValidateRewriteExternalCss("flatten_then_cache_extend",
                             css_before, css_after,
                             kExpectCached | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, CacheExtendsAfterFlatteningNestedAbsoluteUrls) {
  TestCacheExtendsAfterFlatteningNested(false);
}

TEST_F(CssFlattenImportsTest, CacheExtendsAfterFlatteningNestedRelativeUrls) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  TestCacheExtendsAfterFlatteningNested(true);
}

TEST_F(CssFlattenImportsTest, FlattenRecursion) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "Recursive @import of http://test.com/recursive.css-->");

  const char kFilename[] = "recursive.css";
  const GoogleString css_in =
      StrCat("@import url(http://test.com/", kFilename, ");");

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_in, 100);

  ValidateRewriteExternalCss("flatten_recursive",
                             css_in, css_in,
                             kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsRecursion);
}

TEST_F(CssFlattenImportsTest, FlattenSimpleMedia) {
  const GoogleString css_in =
      StrCat("@import url(http://test.com/", kSimpleCssFile, ") screen;");
  const GoogleString css_out =
      StrCat("@media screen{", kSimpleCss, "}");

  SetResponseWithDefaultHeaders(kSimpleCssFile, kContentTypeCss, css_out, 100);

  ValidateRewriteExternalCss("flatten_simple_media", css_in, css_out,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  // We do not specify kNoClearFetcher, so the fetcher is cleared. Thus,
  // content must be pulled from the cache. kNoOtherContexts because
  // other contexts won't have this value cached.
  ValidateRewriteExternalCss("flatten_simple_media", css_in, css_out,
                             kExpectCached | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenNestedMedia) {
  const char kStylesFilename[] = "styles.css";
  const GoogleString kStylesContents = StrCat(
      "@import url(print.css) print;"
      "@import url(screen.css) screen;"
      "@media all{",
      kSimpleCss,
      "}");
  SetResponseWithDefaultHeaders(kStylesFilename, kContentTypeCss,
                                kStylesContents, 100);

  const char kPrintFilename[] = "print.css";
  const char kPrintCss[] =
      ".background_cyan{background-color:#0ff}"
      ".foreground_pink{color:#ffc0cb}";
  const char kPrintAllCss[] =
      ".background_green{background-color:#0f0}"
      ".foreground_rose{color:rose}";
  const GoogleString kPrintContents = StrCat(
      "@import url(screen.css) screen;",  // discarded because print != screen
      kPrintCss,
      "@media all{",                      // subsetted to print
      kPrintAllCss,
      "}");
  SetResponseWithDefaultHeaders(kPrintFilename, kContentTypeCss,
                                kPrintContents, 100);

  const char kScreenFilename[] = "screen.css";
  const char kScreenCss[] =
      ".background_blue{background-color:#00f}"
      ".foreground_gray{color:gray}";
  const char kScreenAllCss[] =
      ".background_white{background-color:#fff}"
      ".foreground_black{color:#000}";
  const GoogleString kScreenContents = StrCat(
      "@import url(print.css) print;",  // discarded because screen != print
      kScreenCss,
      "@media all{",                    // subsetted to screen
      kScreenAllCss,
      "}");
  SetResponseWithDefaultHeaders(kScreenFilename, kContentTypeCss,
                                kScreenContents, 100);

  const char css_in[] =
      "@import url(http://test.com/styles.css);";
  const GoogleString css_out = StrCat(
      StrCat("@media print{",
             kPrintCss,
             kPrintAllCss,
             "}"),
      StrCat("@media screen{",
             kScreenCss,
             kScreenAllCss,
             "}"),
      kSimpleCss);

  ValidateRewriteExternalCss("flatten_nested_media", css_in, css_out,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_nested_media",
                             css_in, css_out,
                             kExpectCached | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenAllMedia) {
  const GoogleString kStylesContents = "@import url(all.css) all;";
  const char kAllContents[] = "*{display: inline-block;}";
  SetResponseWithDefaultHeaders("all.css", kContentTypeCss, kAllContents, 100);

  const char kMinifiedAllContents[] = "*{display:inline-block}";

  ValidateRewriteExternalCss("flatten_all_media", kStylesContents,
                             kMinifiedAllContents,
                             kExpectSuccess | kNoClearFetcher);

  // Now double-check against an incompatible one.
  const GoogleString kStylesContentsPrint = "@import url(print.css) print;";
  const char kPrintContents[] = "img{display: none;}";
  SetResponseWithDefaultHeaders("print.css", kContentTypeCss,
                                kPrintContents, 100);
  ValidateRewriteExternalCss("flatten_all_media2", kStylesContentsPrint,
                             "@media print{img{display:none}}",
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenFontFace) {
  const char kStylesFont[] = "@import url(font.css);";
  SetResponseWithDefaultHeaders("font.css", kContentTypeCss,
                                "@font-face { font-family: 'cyborgo'; }", 100);
  ValidateRewriteExternalCss("flatten_font_face", kStylesFont,
                             "@font-face{font-family:'cyborgo'}",
                             kExpectSuccess | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenCacheDependsOnMedia) {
  const GoogleString css_screen =
      StrCat("@media screen{", kSimpleCss, "}");
  const char css_print[] =
      "@media print{"
      ".background_white{background-color:#fff}"
      ".foreground_black{color:#000}"
      "}";

  const char kFilename[] = "mixed.css";
  const GoogleString css_contents = StrCat(css_screen, css_print);
  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_contents, 100);

  // When we @import with media screen we should cache the file in its
  // entirety, and the screen-specific results, separately.
  const GoogleString screen_in =
      StrCat("@import url(http://test.com/", kFilename, ") screen;");
  ValidateRewriteExternalCss("flatten_mixed_media_screen",
                             screen_in, css_screen,
                             kExpectSuccess | kNoClearFetcher);
  // The sequence is:
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // MISS   for mixed.css's partition key (for 'screen').
  // MISS   for mixed.css's URL.
  // INSERT for the fetched mixed.css's URL.
  // INSERT for the rewritten mixed.css's URL (for 'screen').
  // INSERT for the fetched mixed.css's partition key (for 'screen').
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //        ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 6 inserts, 4 misses, 1 hit.
  EXPECT_EQ(6, lru_cache()->num_elements());
  EXPECT_EQ(6, lru_cache()->num_inserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
  EXPECT_EQ(4, lru_cache()->num_misses());
  EXPECT_EQ(1, lru_cache()->num_hits());

  // When we @import with media print we should find the cached file but
  // generate and cache the print-specific results.
  const GoogleString print_in =
      StrCat("@import url(http://test.com/", kFilename, ") print;");
  ValidateRewriteExternalCss("flatten_mixed_media_print",
                             print_in, css_print,
                             kExpectSuccess | kNoClearFetcher);

  // The sequence in this case, for the new external link (_repeat on the end):
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // MISS   for mixed.css's partition key (for 'print').
  // HIT    for mixed.css's URL.
  // DELETE for the rewritten mixed.css's URL (for 'screen').
  // INSERT for the rewritten mixed.css's URL (for 'print').
  // INSERT for the fetched mixed.css's partition key (for 'print').
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //        ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 5 inserts, 1 delete, 3 misses, 2 hits.
  EXPECT_EQ(10, lru_cache()->num_elements());
  EXPECT_EQ(11, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_deletes());
  EXPECT_EQ(7, lru_cache()->num_misses());
  EXPECT_EQ(3, lru_cache()->num_hits());

  // Now when we @import with media screen we should find cached data.
  // Even though the cached data for mixed.css's URL is wrong for screen
  // it doesn't matter because the data we use is accessed via its partition
  // key which has the correct data for screen.
  ValidateRewriteExternalCss("flatten_mixed_media_screen_repeat",
                             screen_in, css_screen,
                             kExpectSuccess | kNoClearFetcher);
  // The sequence is:
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // HIT    for mixed.css's partition key (for 'screen').
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //        ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 3 inserts, 2 misses, 2 hit.
  EXPECT_EQ(13, lru_cache()->num_elements());
  EXPECT_EQ(14, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_deletes());
  EXPECT_EQ(9, lru_cache()->num_misses());
  EXPECT_EQ(5, lru_cache()->num_hits());

  // Ditto for re-fetching print.
  ValidateRewriteExternalCss("flatten_mixed_media_print_repeat",
                             print_in, css_print,
                             kExpectSuccess | kNoClearFetcher);
  // The sequence is:
  // MISS   for the external link's partition key.
  // MISS   for the external link's URL.
  // INSERT for the fetched external link.
  // HIT    for mixed.css's partition key (for 'print').
  // INSERT for the rewritten external link's URL.
  // INSERT for the rewritten external link's partition key.
  // HIT    for the rewritten external link's URL (from the fetch done by
  //        ValidateRewriteExternalCss with the kExpectSuccess flag).
  // So, 3 inserts, 2 misses, 2 hit.
  EXPECT_EQ(16, lru_cache()->num_elements());
  EXPECT_EQ(17, lru_cache()->num_inserts());
  EXPECT_EQ(1, lru_cache()->num_deletes());
  EXPECT_EQ(11, lru_cache()->num_misses());
  EXPECT_EQ(7, lru_cache()->num_hits());
}

TEST_F(CssFlattenImportsTest, FlattenNestedCharsetsOk) {
  // HTML = utf-8 (1st argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("utf-8", "", "", true);
}

TEST_F(CssFlattenImportsTest, FlattenNestedCharsetsMismatch) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "The charset of http://test.com/styles.css "
                   "(utf-8 from headers) is different from that of its parent "
                   "(inline): iso-8859-1 from unknown-->");

  // HTML = iso-8859-1 (default), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "", "", false);
}

TEST_F(CssFlattenImportsTest, FlattenFailsIfLinkHasWrongCharset) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "The charset of the HTML (iso-8859-1, the default) "
                   "is different from the charset attribute "
                   "on the preceding element (utf-8)-->");

  const char kStylesFilename[] = "styles.css";
  SetResponseWithDefaultHeaders(kStylesFilename, kContentTypeCss,
                                kSimpleCss, 100);

  const char css_in[] =
      "@import url(http://test.com/styles.css);";

  // TODO(sligocki): Why does this need kNoOtherContexts?
  ValidateRewriteExternalCss("flatten_link_charset", css_in, css_in,
                             kExpectSuccess | kNoOtherContexts |
                             kNoClearFetcher | kLinkCharsetIsUTF8 |
                             kFlattenImportsCharsetMismatch);
}

TEST_F(CssFlattenImportsTest, FlattenRespectsMetaTagCharset) {
  // HTML = utf-8 (2nd argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "utf-8", "", true);
}

TEST_F(CssFlattenImportsTest, FlattenRespectsHttpEquivCharset) {
  // HTML = utf-8 (3rd argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "", "utf-8", true);
}

TEST_F(CssFlattenImportsTest, FlattenRespectsHttpEquivCharsetUnquoted) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "The charset of http://test.com/styles.css "
                   "(utf-8 from headers) is different from that of its parent "
                   "(inline): ISO-8859-1 from unknown-->");

  // HTML = iso-8859-1 (3rd argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "", "iso-8859-1", false);
}

TEST_F(CssFlattenImportsTest, HeaderTakesPrecendenceOverMetaTag1) {
  // HTML = utf-8 (1st argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("utf-8", "iso-8859-1", "", true);
}

TEST_F(CssFlattenImportsTest, HeaderTakesPrecendenceOverMetaTag2) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "The charset of http://test.com/styles.css "
                   "(utf-8 from headers) is different from that of its parent "
                   "(inline): iso-8859-1 from unknown-->");

  // HTML = iso-8859-1 (1st argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("iso-8859-1", "utf-8", "", false);
}

// Make sure we deal correctly with invalid URL in child.
TEST_F(CssFlattenImportsTest, InvalidGrandchildUrl) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "Invalid import URL //// in http://test.com/child.css-->");

  // Invalid URL.
  SetResponseWithDefaultHeaders("child.css", kContentTypeCss,
                                "@import url(////);", 100);

  // TODO(sligocki): Why did this fail when run as ValidateRewrite()?
  ValidateRewriteExternalCss("invalid_url",
                             "@import 'child.css';",
                             "@import url(child.css);",
                             kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsInvalidUrl);
}

// Test that we do not flatten @imports that have complex media queries.
TEST_F(CssFlattenImportsTest, NoFlattenMediaQueries) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "Complex media queries in the @import of inline-->");

  ValidateRewrite("media_queries",
                  // We do not flatten @imports with complex media queries.
                  "@import url(child.css) not screen;",
                  "@import url(child.css) not screen;",
                  kExpectSuccess | kFlattenImportsComplexQueries);
}

// Still don't flatten because child @import has complex media query.
TEST_F(CssFlattenImportsTest, NoFlattenMediaQueriesChild) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: "
                   "Complex media queries in the @import of inline-->");

  SetResponseWithDefaultHeaders("child.css", kContentTypeCss,
                                "@import url(g.css) screen and (color);", 100);

  // TODO(sligocki): Why did this fail when run as ValidateRewrite()?
  ValidateRewriteExternalCss("invalid_url",
                             "@import 'child.css';",
                             "@import url(child.css);",
                             kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsComplexQueries);
}

// See https://github.com/apache/incubator-pagespeed-mod/issues/1092
TEST_F(CssFlattenImportsTest, FlattenTooComplexNested) {
  GoogleString css_in = StrCat("@import url(http://test.com/",
                               kComplexCssFile, ");");

  // First time should load the CSS files into cache.
  ValidateRewriteExternalCss("flatten_too_complex_nested", css_in,
                             css_in, kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsComplexQueries);

  // Re-optimize. The CSS output should equal the input (and especially not be
  // an empty string).
  ValidateRewriteExternalCss("flatten_too_complex_nested_repeat",
                             css_in, css_in, kExpectSuccess |
                             kFlattenImportsComplexQueries | kNoClearFetcher);
}

// Test that we correctly deal with @import media types with possibly complex
// @media media queries in flattened CSS.
//
// Currently we just fail to flatten any CSS file with complex media queries.
// TODO(matterbury): Merge complex media queries for @media statements with
// simple media types allowed in @import statements.
// Note: This will require some thought and will almost certainly not be
// possible for some media queries (like those with "not"). We should only
// do this if there we think it is impacting our performance.
TEST_F(CssFlattenImportsTest, MergeMediaQueries) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: A media query "
                   "is too complex in http://test.com/child.css-->");

  const char child_contents[] =
      "@media screen and (color) { .a { color: red; } }\n"
      "@media print and (max-width: 400px), only screen { .b { color: blue } }";
  SetResponseWithDefaultHeaders("child.css", kContentTypeCss,
                                child_contents, 100);

  // TODO(sligocki): Why did this fail when run as ValidateRewrite()?
  ValidateRewriteExternalCss("invalid_url",
                             "@import url(child.css) screen;",
                             "@import url(child.css) screen;",
                             /* TODO(sligocki): This could be:
                             "@media screen and (color){.a{color: red}}"
                             "@media only screen{.b{color:#00f}}",
                             */
                             kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsComplexQueries);
}

// Intersections of media queries with "only" & "and" can be resolved relatively
// eaily, however it is almost impossible with "not". Ex: What is the
// intersection of "screen" and "not print and (max-width: 400px)"?
// "screen and (not-max-width: 400px)"?? We just give up with "not".
TEST_F(CssFlattenImportsTest, NoFlattenMediaQueriesAtMedia) {
  // Turn on debug to get the flattening failure reason in an HTML comment.
  DebugWithMessage("<!--Flattening failed: A media query "
                   "is too complex in http://test.com/child.css-->");

  const char child_contents[] =
      "@media screen and (color) { .a { color: red; } }\n"
      "@media not print and (max-width: 400px) { .b { color: blue; } }\n";
  SetResponseWithDefaultHeaders("child.css", kContentTypeCss,
                                child_contents, 100);

  // TODO(sligocki): Why did this fail when run as ValidateRewrite()?
  ValidateRewriteExternalCss("invalid_url",
                             "@import url(child.css) screen;",
                             "@import url(child.css) screen;",
                             kExpectSuccess | kNoClearFetcher |
                             kFlattenImportsComplexQueries);
}

TEST_F(CssFlattenImportsTest, FlattenInlineCssWithRelativeImage) {
  // Proves that URLs are fixed when CSS is rewritten.
  TestRelativeImageUrlInRelativeCssUrl(false, true);
}

class CssFlattenImportsOnlyTest : public CssFlattenImportsTest {
 protected:
  virtual void SetUpFilters() {
    options()->SetRewriteLevel(RewriteOptions::kPassThrough);
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->set_always_rewrite_css(true);
    rewrite_driver()->AddFilters();
  }
};

TEST_F(CssFlattenImportsOnlyTest, FlattenInlineCssWithRelativeImage) {
  // Proves that URLs are absolutified when CSS is flattened but not rewritten.
  TestRelativeImageUrlInRelativeCssUrl(false, false);
  TestFlattenNested(false, false);
}

TEST_F(CssFlattenImportsOnlyTest, FlattenAndTrimInlineCssWithRelativeImage) {
  // Proves that URLs are fixed when CSS is flattened -and- -trimmed- but not
  // rewritten.
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  server_context()->ComputeSignature(options());
  TestRelativeImageUrlInRelativeCssUrl(true, false);
  TestFlattenNested(true, false);
}

class CssFlattenImportsAndRewriteImagesTest : public CssFlattenImportsTest {
 protected:
  virtual void SetUpFilters() {
    options()->SetRewriteLevel(RewriteOptions::kPassThrough);
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->EnableFilter(RewriteOptions::kRecompressPng);
    options()->set_always_rewrite_css(true);
    rewrite_driver()->AddFilters();
  }
};

TEST_F(CssFlattenImportsAndRewriteImagesTest, UnauthorizedImageDomain) {
  // Setup the image we refer to.
  const char kFooPng[] = "http://unauth.com/images/foo.png";
  const char kImageData[] = "Invalid PNG but does not matter for this test";
  SetResponseWithDefaultHeaders(kFooPng, kContentTypePng, kImageData, 100);
  // Setup the CSS that refers to it.
  const char kSimpleCssTemplate[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}"
      ".body{background-image:url(%s)}";
  // The input CSS refers to ../images/test.jpg from the file /a/b/simple.css,
  // so the image's path is /a/images/test.jpg, which is what should be used
  // when the CSS is flattened into the base document (with base of '/').
  const GoogleString simple_css_path =
      StrCat(kTestDomain, "a/b/", kSimpleCssFile);
  const GoogleString simple_css_in = StringPrintf(kSimpleCssTemplate, kFooPng);
  SetResponseWithDefaultHeaders(
      simple_css_path, kContentTypeCss, simple_css_in, 100);
  const GoogleString import_simple_css =
      StrCat("@import url(", simple_css_path, ");");
  DebugWithMessage(StrCat("<!--Cannot rewrite ", kFooPng,
                          " as it is on an unauthorized domain-->"));
  ValidateRewriteExternalCss("unauthorized_image_domain",
                             import_simple_css, simple_css_in,
                             kExpectSuccess | kNoClearFetcher);
}

}  // namespace

}  // namespace net_instaweb
