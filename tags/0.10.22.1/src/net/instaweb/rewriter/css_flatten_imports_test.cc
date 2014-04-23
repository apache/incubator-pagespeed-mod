/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: matterbury@google.com (Matt Atterbury)

#include <cstddef>
#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/resource_manager.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/basictypes.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";

const char kTopCssFile[] = "assets/styles.css";
const char kOneLevelDownFile1[] = "assets/nested1.css";
const char kOneLevelDownFile2[] = "assets/nested2.css";
const char kTwoLevelsDownFile1[] = "assets/nested/nested1.css";
const char kTwoLevelsDownFile2[] = "assets/nested/nested2.css";
const char k404CssFile[] = "404.css";

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

  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kFlattenCssImports);
    options()->EnableFilter(RewriteOptions::kExtendCacheImages);
    CssRewriteTestBase::SetUp();
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
    SetFetchResponse404(k404CssFile);
  }

  // General routine to test that we flatten -then- cache extend the PNG in
  // the resulting CSS while absolutifying the PNGs' URLs while flattening
  // then [not] relativizing them while rewriting them.
  void TestCacheExtendsAfterFlatteningNested(bool trim_urls) {
    // foo.png
    const char kFooPngFilename[] = "foo.png";
    const char kImageData[] = "Invalid PNG but does not matter for this test";
    SetResponseWithDefaultHeaders(kFooPngFilename, kContentTypePng,
                                  kImageData, 100);

    // image1.css loads foo.png as a background image.
    const char kCss1Filename[] = "image1.css";
    const GoogleString css1_before =
        StrCat("body {\n"
               "  background-image: url(", kFooPngFilename, ");\n"
               "}\n");
    const GoogleString css1_after =
        StrCat("body{background-image:url(",
               Encode(trim_urls ? "" : kTestDomain,
                      "ce", "0", kFooPngFilename, "png"),
               ")}");
    SetResponseWithDefaultHeaders(kCss1Filename, kContentTypeCss,
                                  css1_before, 100);

    // bar.png
    const char kBarPngFilename[] = "bar.png";
    SetResponseWithDefaultHeaders(StrCat("nested/", kBarPngFilename),
                                  kContentTypePng, kImageData, 100);

    // image2.css loads bar.png as a background image.
    const char kCss2Filename[] = "nested/image2.css";  // because its CSS is!
    const GoogleString css2_before =
        StrCat("body {\n"
               "  background-image: url(", kBarPngFilename, ");\n"
               "}\n");
    const GoogleString css2_after =
        StrCat("body{background-image:url(",
               Encode(trim_urls ? "nested/" : StrCat(kTestDomain, "nested/"),
                      "ce", "0", kBarPngFilename, "png"),
               ")}");
    SetResponseWithDefaultHeaders(kCss2Filename, kContentTypeCss,
                                  css2_before, 100);

    // foo-then-bar.css @imports image1.css then image2.css
    const char kTop1CssFilename[] = "foo-then-bar.css";
    const GoogleString top1_before =
        StrCat("@import url(", kCss1Filename, ");",
               "@import url(", kCss2Filename, ");");
    const GoogleString top1_after = StrCat(css1_after, css2_after);
    SetResponseWithDefaultHeaders(kTop1CssFilename, kContentTypeCss,
                                  top1_before, 100);

    // bar-then-foo.css @imports image2.css then image1.css
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
                               kExpectSuccess |
                               kNoOtherContexts | kNoClearFetcher);
    ValidateRewriteExternalCss("flatten_then_cache_extend_nested2",
                               top2_before, top2_after,
                               kExpectSuccess |
                               kNoOtherContexts | kNoClearFetcher);
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
    const char kStylesCss[] =
        ".background_red{background-color:red}"
        ".foreground_yellow{color:#ff0}";
    const GoogleString kStylesContents = StrCat(
        "@charset \"uTf-8\";",
        "@import url(print.css);",
        "@import url(screen.css);",
        kStylesCss);

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
    ValidationFlags meta_tag_flag = static_cast<ValidationFlags>(0);
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

    const char css_in[] = "@import url(http://test.com/styles.css) ;";
    if (should_succeed) {
      const GoogleString css_out = StrCat(kPrintCss, kScreenCss, kStylesCss);

      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_out,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
      // Check things work when data is already cached.
      ValidateRewriteExternalCss("flatten_nested_media_repeat",
                                 css_in, css_out,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
    } else {
      ValidateRewriteExternalCss("flatten_nested_media",
                                 css_in, css_in,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
      ValidateRewriteExternalCss("flatten_nested_media_repeat",
                                 css_in, css_in,
                                 kExpectSuccess | kNoOtherContexts |
                                 kNoClearFetcher | meta_tag_flag);
    }
  }

  GoogleString kOneLevelDownContents1;
  GoogleString kOneLevelDownContents2;
  GoogleString kTopCssContents;
  GoogleString kFlattenedTopCssContents;
  GoogleString kFlattenedOneLevelDownContents1;
};

TEST_F(CssFlattenImportsTest, FlattenInlineCss) {
  const char kFilename[] = "simple.css";
  const char css_in[] =
      "@import url(http://test.com/simple.css) ;";
  const char css_out[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_out, 100);

  ValidateRewriteInlineCss("flatten_simple", css_in, css_out, kExpectSuccess);
  // TODO(sligocki): This suggests that we grew the number of bytes, which is
  // misleading because originally, the user would have loaded both files
  // and now they will only load one. So total bytes are less.
  // I think this should be listing bytes saved as STATIC_STRLEN(css_in).
  EXPECT_EQ(STATIC_STRLEN(css_in) - STATIC_STRLEN(css_out),
            total_bytes_saved_->Get());
}

TEST_F(CssFlattenImportsTest, DontFlattenAttributeCss) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kRewriteStyleAttributes);
  resource_manager()->ComputeSignature(options());

  const char kFilename[] = "simple.css";
  const char css_out[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_out, 100);

  // Test that rewriting of attributes is enabled and working.
  ValidateExpected("rewrite-attribute-setup",
                   "<div style='background-color: #f00; color: yellow;'/>",
                   "<div style='background-color:red;color:#ff0'/>");

  // Test that we don't rewrite @import's in attributes since that's invalid.
  ValidateNoChanges("rewrite-attribute",
                    "<div style='@import url(http://test.com/simple.css)'/>");
}

TEST_F(CssFlattenImportsTest, FlattenNoop) {
  const char contents[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  ValidateRewriteExternalCss("flatten_noop",
                             contents, contents,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, Flatten404) {
  const char css_in[] =
      "@import url(http://test.com/404.css) ;";

  ValidateRewriteExternalCss("flatten_404",
                             css_in, css_in,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenInvalidCSS) {
  const char kInvalidMediaCss[] = "@media }}";
  ValidateRewriteExternalCss("flatten_invalid_css_media",
                             kInvalidMediaCss, kInvalidMediaCss,
                             kExpectFailure);
  EXPECT_EQ(1, num_parse_failures_->Get());

  const char kInvalidImportCss[] = "@import styles.css; a { color:red }";
  ValidateRewriteExternalCss("flatten_invalid_css_import",
                             kInvalidImportCss, kInvalidImportCss,
                             kExpectFailure);
  EXPECT_EQ(1, num_parse_failures_->Get());

  // This gets a parse error but thanks to the idea of "unparseable sections"
  // in the CSS parser it's not treated as an error as such and the "bad" text
  // is kept, and since the @import itself is valid we DO flatten.
  const char kInvalidRuleCss[] = "@import url(styles.css) ;a{{ color:red }";
  const char kFilename[] = "styles.css";
  const char kStylesCss[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";
  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, kStylesCss, 100);

  GoogleString kFlattenedInvalidCss = StrCat(kStylesCss, "a{{ color:red }");

  ValidateRewriteExternalCss("flatten_invalid_css_rule",
                             kInvalidRuleCss, kFlattenedInvalidCss,
                             kExpectSuccess | kNoClearFetcher);
  EXPECT_EQ(0, num_parse_failures_->Get());
}

TEST_F(CssFlattenImportsTest, FlattenEmptyMedia) {
  ValidateRewriteExternalCss("flatten_empty_media",
                             "@media {}", "", kExpectSuccess);
}

TEST_F(CssFlattenImportsTest, FlattenSimple) {
  const char kFilename[] = "simple.css";
  const char css_in[] =
      "@import url(http://test.com/simple.css) ;";
  const char css_out[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_out, 100);

  ValidateRewriteExternalCss("flatten_simple",
                             css_in, css_out,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_simple_repeat",
                             css_in, css_out,
                             kExpectSuccess | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenEmpty) {
  const char kFilename[] = "empty.css";
  const char css_in[] = "@import url(http://test.com/empty.css) ;";
  const char css_out[] = "";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_out, 100);

  ValidateRewriteExternalCss("flatten_empty",
                             css_in, css_out,
                             kExpectSuccess | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_empty_repeat",
                             css_in, css_out,
                             kExpectSuccess | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenSimpleRewriteOnTheFly) {
  // import.css @import's simple.css
  // simple.css contains some simple CSS
  // Fetch the rewritten filename of import.css and we should get the
  // flattened and minimized contents, namely simple.css's contents.

  const char kImportFilename[] = "import.css";
  const char css_import[] =
      "@import url(http://test.com/simple.css) ;";
  SetResponseWithDefaultHeaders(kImportFilename, kContentTypeCss,
                                css_import, 100);

  const char kSimpleFilename[] = "simple.css";
  const char css_simple[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";
  SetResponseWithDefaultHeaders(kSimpleFilename, kContentTypeCss,
                                css_simple, 100);

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
  EXPECT_EQ(css_simple, content);

  // Check for 6 misses and 6 inserts giving 6 elements at the end:
  // 3 URLs (import.css/simple.css/rewritten) x 2 (partition key + contents).
  EXPECT_EQ(6, lru_cache()->num_elements());
  EXPECT_EQ(6, lru_cache()->num_inserts());
  EXPECT_EQ(6, lru_cache()->num_misses());
  EXPECT_EQ(0, lru_cache()->num_hits());
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
  EXPECT_EQ(0, lru_cache()->num_deletes());
}

TEST_F(CssFlattenImportsTest, FlattenNested) {
  const GoogleString css_in = StrCat("@import url(http://test.com/",
                                     kTopCssFile, ") ;");

  ValidateRewriteExternalCss("flatten_nested",
                             css_in, kFlattenedTopCssContents,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenFromCacheDirectly) {
  // Prime the pumps by loading all the CSS files into the cache.
  // Verifying that the resources fetched below _are_ cached is non-trivial
  // because they are stored against their partition key and determining that
  // from this level requires access to and reimplementation of the inner
  // working of RewriteContext and various sub-classes. At the time of writing
  // I verified in the debugger that they are cached.
  GoogleString css_in = StrCat("@import url(http://test.com/",
                               kTopCssFile, ") ;");
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
  ValidateRewriteExternalCss("flatten_from_cache_directly",
                             css_in, kFlattenedTopCssContents,
                             kExpectSuccess | kNoStatCheck | kNoOtherContexts);

  // Check that everything was read from the cache in one hit, taking into
  // account that ValidateRewriteExternalCss with kExpectSuccess also reads
  // the resource after rewriting it, hence there will be TWO cache hits.
  EXPECT_EQ(num_elements, lru_cache()->num_elements());
  EXPECT_EQ(0, lru_cache()->num_misses());
  EXPECT_EQ(2, lru_cache()->num_hits());
  ClearStats();
  num_elements = lru_cache()->num_elements();

  // Access one of the cached ones directly.
  css_in = StrCat("@import url(http://test.com/", kTwoLevelsDownFile1, ") ;");
  ValidateRewriteExternalCss("flatten_from_cache_directly_repeat",
                             css_in, kTwoLevelsDownContents1,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);

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
                               kTopCssFile, ") ;");
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
  css_in = StrCat("@import url(http://test.com/", filename, ") ;");
  GoogleString contents = StrCat("@import url(", kOneLevelDownFile1, ") ;");
  SetResponseWithDefaultHeaders(filename, kContentTypeCss, contents, 100);
  ValidateRewriteExternalCss("flatten_from_cache_indirectly_repeat",
                             css_in, kFlattenedOneLevelDownContents1,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);

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
             Encode(kTestDomain, "ce", "0", "foo.png", "png"),
             ")}");
  SetResponseWithDefaultHeaders(kCssFilename, kContentTypeCss, css_before, 100);

  const char kFooPngFilename[] = "foo.png";
  const char kImageData[] = "Invalid PNG but it does not matter for this test";
  SetResponseWithDefaultHeaders(kFooPngFilename, kContentTypePng,
                                kImageData, 100);

  ValidateRewriteExternalCss("flatten_then_cache_extend",
                             css_before, css_after,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);

  // Test when everything is already cached.
  ValidateRewriteExternalCss("flatten_then_cache_extend_repeat",
                             css_before, css_after,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, CacheExtendsAfterFlatteningNestedAbsoluteUrls) {
  TestCacheExtendsAfterFlatteningNested(false);
}

TEST_F(CssFlattenImportsTest, CacheExtendsAfterFlatteningNestedRelativeUrls) {
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kLeftTrimUrls);
  resource_manager()->ComputeSignature(options());
  TestCacheExtendsAfterFlatteningNested(true);
}

TEST_F(CssFlattenImportsTest, FlattenRecursion) {
  const char kFilename[] = "recursive.css";
  const GoogleString css_in =
      StrCat("@import url(http://test.com/", kFilename, ") ;");

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_in, 100);

  ValidateRewriteExternalCss("flatten_recursive",
                             css_in, css_in,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
}

TEST_F(CssFlattenImportsTest, FlattenSimpleMedia) {
  const char kFilename[] = "simple.css";
  const GoogleString css_in =
      StrCat("@import url(http://test.com/", kFilename, ") screen ;");
  const char css_out[] =
      "@media screen{"
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}"
      "}";

  SetResponseWithDefaultHeaders(kFilename, kContentTypeCss, css_out, 100);

  ValidateRewriteExternalCss("flatten_simple_media",
                             css_in, css_out,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_simple_media_repeat",
                             css_in, css_out,
                             kExpectSuccess | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenNestedMedia) {
  const char kStylesFilename[] = "styles.css";
  const char kStylesCss[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";
  const GoogleString kStylesContents = StrCat(
      "@import url(print.css) print;",
      "@import url(screen.css) screen;",
      "@media all{",
      kStylesCss,
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
      "@import url(screen.css) screen;", // discarded because print != screen
      kPrintCss,
      "@media all{",                     // subsetted to print
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
      "@import url(http://test.com/styles.css) ;";
  const GoogleString css_out = StrCat(
      StrCat("@media print{",
             kPrintCss,
             kPrintAllCss,
             "}"),
      StrCat("@media screen{",
             kScreenCss,
             kScreenAllCss,
             "}"),
      kStylesCss);

  ValidateRewriteExternalCss("flatten_nested_media",
                             css_in, css_out,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
  // Check things work when data is already cached.
  ValidateRewriteExternalCss("flatten_nested_media_repeat",
                             css_in, css_out,
                             kExpectSuccess | kNoOtherContexts);
}

TEST_F(CssFlattenImportsTest, FlattenCacheDependsOnMedia) {
  const char css_screen[] =
      "@media screen{"
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}"
      "}";
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
      StrCat("@import url(http://test.com/", kFilename, ") screen ;");
  ValidateRewriteExternalCss("flatten_mixed_media_screen",
                             screen_in, css_screen,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
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
      StrCat("@import url(http://test.com/", kFilename, ") print ;");
  ValidateRewriteExternalCss("flatten_mixed_media_print",
                             print_in, css_print,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);

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
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
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
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher);
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
  // HTML = iso-8859-1 (default), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "", "", false);
}

TEST_F(CssFlattenImportsTest, FlattenFailsIfLinkHasWrongCharset) {
  const char kStylesFilename[] = "styles.css";
  const char kStylesCss[] =
      ".background_red{background-color:red}"
      ".foreground_yellow{color:#ff0}";
  SetResponseWithDefaultHeaders(kStylesFilename, kContentTypeCss,
                                kStylesCss, 100);

  const char css_in[] =
      "@import url(http://test.com/styles.css) ;";

  ValidateRewriteExternalCss("flatten_link_charset",
                             css_in, css_in,
                             kExpectSuccess |
                             kNoOtherContexts | kNoClearFetcher |
                             kLinkCharsetIsUTF8);
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
  // HTML = iso-8859-1 (3rd argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("", "", "iso-8859-1", false);
}

TEST_F(CssFlattenImportsTest, HeaderTakesPrecendenceOverMetaTag1) {
  // HTML = utf-8 (1st argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("utf-8", "iso-8859-1", "", true);
}

TEST_F(CssFlattenImportsTest, HeaderTakesPrecendenceOverMetaTag2) {
  // HTML = iso-8859-1 (1st argument), CSS = utf-8 (always).
  TestFlattenWithHtmlCharset("iso-8859-1", "utf-8", "", false);
}

}  // namespace

}  // namespace net_instaweb