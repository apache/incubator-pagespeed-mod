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


#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

namespace {

const char kCssFile[] = "assets/styles.css";
const char kCssTail[] = "styles.css";
const char kCssSubdir[] = "assets/";
const char kCssData[] = ".blue {color: blue; src: url(dummy.png);}";

class CssInlineImportToLinkFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    SetHtmlMimetype();
  }

  // Test general situations.
  void ValidateStyleToLink(const GoogleString& input_style,
                           const GoogleString& expected_style) {
    const GoogleString html_input =
        "<head>\n" +
        input_style +
        "</head>\n"
        "<body>Hello, world!</body>\n";

    // Rewrite the HTML page.
    ParseUrl("http://test.com/test.html", html_input);

    // Check the output HTML.
    const GoogleString expected_output =
        "<head>\n" +
        expected_style +
        "</head>\n"
        "<body>Hello, world!</body>\n";
    EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  }

  void ValidateStyleUnchanged(const GoogleString& import_equals_output) {
    ValidateStyleToLink(import_equals_output, import_equals_output);
  }
};

TEST_F(CssInlineImportToLinkFilterTest, CssPreserveURLOff) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->set_css_preserve_urls(false);
  static const char kLink[] =
      "<link rel=\"stylesheet\" href=\"assets/styles.css\">";
  rewrite_driver()->AddFilters();
  ValidateStyleToLink("<style>@import url(assets/styles.css);</style>", kLink);
}

TEST_F(CssInlineImportToLinkFilterTest, AlwaysAllowUnauthorizedDomain) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->set_css_preserve_urls(false);
  rewrite_driver()->AddFilters();
  ValidateStyleToLink(
      "<style>@import url(http://unauth.com/assets/styles.css);</style>",
      "<link rel=\"stylesheet\" href=\"http://unauth.com/assets/styles.css\">");
}

// Tests for converting styles to links.
TEST_F(CssInlineImportToLinkFilterTest, ConvertGoodStyle) {
  AddFilter(RewriteOptions::kInlineImportToLink);

  static const char kLink[] =
      "<link rel=\"stylesheet\" href=\"assets/styles.css\">";

  // These all get converted to the above link.
  ValidateStyleToLink("<style>@import url(assets/styles.css);</style>", kLink);
  ValidateStyleToLink("<style>@import url(\"assets/styles.css\");</style>",
                      kLink);
  ValidateStyleToLink("<style>\n\t@import \"assets/styles.css\"\t;\n\t</style>",
                      kLink);
  ValidateStyleToLink("<style>@import 'assets/styles.css';</style>", kLink);
  ValidateStyleToLink("<style>@import url( assets/styles.css);</style>", kLink);
  ValidateStyleToLink("<style>@import url('assets/styles.css');</style>",
                      kLink);
  ValidateStyleToLink("<style>@import url( 'assets/styles.css' );</style>",
                      kLink);

  // According to the latest DRAFT CSS spec this is invalid due to the missing
  // final semicolon, however according to the 2003 spec it is valid. Some
  // browsers seem to accept it and some don't, so we will accept it.
  ValidateStyleToLink("<style>@import url(assets/styles.css)</style>", kLink);
}

TEST_F(CssInlineImportToLinkFilterTest, DoNotConvertScoped) {
  // <style scoped> can't be converted to a link.
  // (https://github.com/apache/incubator-pagespeed-mod/issues/918)
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleUnchanged("<style type=\"text/css\" scoped>"
                         "@import url(assets/styles.css);</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithMultipleImports) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleToLink(
      "<style>"
      "@import \"first.css\" all;\n"
      "@import url(\"second.css\" );\n"
      "@import 'third.css';\n"
      "</style>",
      "<link rel=\"stylesheet\" href=\"first.css\" media=\"all\">"
      "<link rel=\"stylesheet\" href=\"second.css\">"
      "<link rel=\"stylesheet\" href=\"third.css\">");
  ValidateStyleToLink(
      "<style>"
      "@import \"first.css\" screen;\n"
      "@import \"third.css\" print;\n"
      "</style>",
      "<link rel=\"stylesheet\" href=\"first.css\" media=\"screen\">"
      "<link rel=\"stylesheet\" href=\"third.css\" media=\"print\">");
  // Example from modpagespeed issue #491. Note that all the attributes from
  // the style are copied to the end of every link.
  ValidateStyleToLink(
      "<style type=\"text/css\" title=\"currentStyle\" media=\"screen\">"
      "   @import \"http://example.com/universal.css?63310\";"
      "       @import \"http://example.com/navigation_beta.css?123\";"
      "   @import \"http://example.com/navigation.css?321\";"
      "   @import \"http://example.com/teases.css\";"
      "   @import \"http://example.com/homepage.css?nocache=987\";"
      "   @import \"http://example.com/yourPicks.css?nocache=123\";"
      "   @import \"http://example.com/sportsTabsHomepage.css\";"
      "   @import \"http://example.com/businessTabsHomepage.css\";"
      "   @import \"http://example.com/slider.css?09\";"
      "   @import \"http://example.com/weather.css\";"
      "  @import \"http://example.com/style3.css\";"
      "  @import \"http://example.com/style3_tmp.css\";"
      "</style>",
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/universal.css?63310\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/navigation_beta.css?123\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/navigation.css?321\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/teases.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/homepage.css?nocache=987\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/yourPicks.css?nocache=123\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/sportsTabsHomepage.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/businessTabsHomepage.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/slider.css?09\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/weather.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/style3.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">"
      "<link rel=\"stylesheet\""
      " href=\"http://example.com/style3_tmp.css\" type=\"text/css\""
      " title=\"currentStyle\" media=\"screen\">");

  // Pull out @import statements, even if there is trailing CSS.
  ValidateStyleToLink(
      "<style>"
      "@import \"first.css\" all;\n"
      "@import url('second.css' );\n"
      "@import \"third.css\";\n"
      ".a { background-color: red }"
      "</style>",

      "<link rel=\"stylesheet\" href=\"first.css\" media=\"all\">"
      "<link rel=\"stylesheet\" href=\"second.css\">"
      "<link rel=\"stylesheet\" href=\"third.css\">"
      "<style>"
      ".a { background-color: red }"
      "</style>");

  // Variations where there's more than just valid @imports.
  // We do not convert because of the invalid @import.
  ValidateStyleUnchanged("<style>"
                         "@import \"first.css\" all;\n"
                         "@import url( );\n"
                         "@import \"third.css\";\n"
                         "</style>");
  // We do not convert because of the @charset
  ValidateStyleUnchanged("<style>"
                         "@charset \"ISO-8859-1\";\n"
                         "@import \"first.css\" all;\n"
                         "@import url('second.css' );\n"
                         "@import \"third.css\";\n"
                         "</style>");

  // These could be handled as it's "obvious" what the right thing is, but
  // at the moment we don't handle all perms-and-combs of media [queries].
  // The first 4 could "ignore" the style's media as it includes the imports.
  ValidateStyleUnchanged("<style>"
                         "@import \"first.css\" screen;\n"
                         "@import \"third.css\" not screen;\n"
                         "</style>");
  ValidateStyleUnchanged("<style media=\"all\">"
                         "@import \"first.css\" screen;\n"
                         "@import \"third.css\" print;\n"
                         "</style>");
  ValidateStyleUnchanged("<style media=\"all\">"
                         "@import \"first.css\" screen;\n"
                         "@import \"third.css\" not screen;\n");
  ValidateStyleUnchanged("<style media=\"screen, not screen\">"
                         "@import \"first.css\" screen;\n"
                         "@import \"third.css\" not screen;\n"
                         "</style>");
  // This one could determine that the intersection of screen & not screen
  // is the empty set and therefore drop the 2nd import/link completely.
  ValidateStyleUnchanged("<style media=\"screen\">"
                         "@import \"first.css\" screen;\n"
                         "@import \"third.css\" not screen;\n"
                         "</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, OnlyConvertPrefix) {
  AddFilter(RewriteOptions::kInlineImportToLink);

  // Trailing content.
  ValidateStyleToLink("<style>@import url(assets/styles.css);\n"
                      "a { color: red; }</style>",

                      "<link rel=\"stylesheet\" href=\"assets/styles.css\">"
                      "<style>a { color: red; }</style>");

  // Nonsense @-rule.
  ValidateStyleToLink("<style>@import url(assets/styles.css);\n"
                      "@foobar</style>",

                      "<link rel=\"stylesheet\" href=\"assets/styles.css\">"
                      "<style>@foobar</style>");

  // @import later in the CSS.
  ValidateStyleToLink("<style>@import url(a.css);\n"
                      "@font-face { src: url(b.woff) }\n"
                      "@import url(c.css);</style>",

                      "<link rel=\"stylesheet\" href=\"a.css\">"
                      "<style>@font-face { src: url(b.woff) }\n"
                      "@import url(c.css);</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithAttributes) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleToLink("<style type=\"text/css\">"
                      "@import url(assets/styles.css);</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen\">"
                      "@import url(assets/styles.css);</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen\">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithSameMedia) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleToLink("<style>@import url(assets/styles.css) all</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " media=\"all\">");
  ValidateStyleToLink("<style type=\"text/css\">"
                      "@import url(assets/styles.css) all;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"all\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen\">"
                      "@import url(assets/styles.css) screen;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\"screen,printer\">"
                      "@import url(assets/styles.css) printer,screen;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"screen,printer\">");
  ValidateStyleToLink("<style type=\"text/css\" media=\" screen , printer \">"
                      "@import 'assets/styles.css' printer, screen ;</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\" screen , printer \">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertStyleWithDifferentMedia) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"screen\">"
                         "@import url(assets/styles.css) all;</style>");
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"screen,printer\">"
                         "@import url(assets/styles.css) screen;</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, MediaQueries) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  // If @import has no media, we'll keep the complex media query in the
  // media attribute.
  ValidateStyleToLink("<style type=\"text/css\" media=\"not screen\">"
                      "@import url(assets/styles.css);</style>",
                      "<link rel=\"stylesheet\" href=\"assets/styles.css\""
                      " type=\"text/css\" media=\"not screen\">");

  // Generally we just give up on complex media queries. Note, these could
  // be rewritten in the future, just change the tests to produce sane results.
  ValidateStyleUnchanged("<style type=\"text/css\">"
                         "@import url(assets/styles.css) not screen;</style>");
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"not screen\">"
                         "@import url(assets/styles.css) not screen;</style>");
  ValidateStyleUnchanged("<style media=\"not screen and (color), only print\">"
                         "@import url(assets/styles.css)"
                         " not screen and (color), only print;</style>");
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"not screen\">"
                         "@import url(assets/styles.css) screen;</style>");
  ValidateStyleUnchanged("<style type=\"text/css\" media=\"screen and (x)\">"
                         "@import url(assets/styles.css) screen;</style>");
}

TEST_F(CssInlineImportToLinkFilterTest, DoNotConvertBadStyle) {
  AddFilter(RewriteOptions::kInlineImportToLink);
  // These all are problematic in some way so are not changed at all.
  ValidateStyleUnchanged("<style/>");
  ValidateStyleUnchanged("<style></style>");
  ValidateStyleUnchanged("<style>@import assets/styles.css;</style>");
  ValidateStyleUnchanged("<style>@import assets/styles.css</style>");
  ValidateStyleUnchanged("<style>@import styles.css</style>");
  ValidateStyleUnchanged("<style>@import foo</style>");
  ValidateStyleUnchanged("<style>@import url (assets/styles.css);</style>");
  ValidateStyleUnchanged("<style>@ import url(assets/styles.css)</style>");
  ValidateStyleUnchanged("<style>*border: 0px</style>");
  ValidateStyleUnchanged("<style>@charset \"ISO-8859-1\";\n"
                         "@import \"mystyle.css\" all;</style>");
  ValidateStyleUnchanged("<style><p/>@import url(assets/styles.css)</style>");
  ValidateStyleUnchanged("<style><![CDATA[@import url(assets/styles.css);]]\n");
  ValidateStyleUnchanged("<style><![CDATA[\njunky junk junk!\n]]\\>\n"
                         "@import url(assets/styles.css);</style>");
  ValidateStyleUnchanged("<style><!-- comment -->"
                         "@import url(assets/styles.css);</style>");
  ValidateStyleUnchanged("<style href='x'>@import url(styles.css);</style>");
  ValidateStyleUnchanged("<style rel='x'>@import url(styles.css);</style>");
  ValidateStyleUnchanged("<style type=\"text/javascript\">"
                         "@import url(assets/styles.css);</style>");
  ValidateStyleUnchanged("<style>@import url(styles.css)<style/></style>");

  // These are fine to convert. These have errors, but only after valid
  // @import statements. Turning them into links is safe.
  ValidateStyleToLink(
      "<style>@import url(assets/styles.css);<p/</style>",

      "<link rel=\"stylesheet\" href=\"assets/styles.css\">"
      "<style><p/</style>");

  ValidateStyleToLink(
      "<style>@import url(assets/styles.css);\n"
      "<![CDATA[\njunky junk junk!\n]]\\></style>",

      "<link rel=\"stylesheet\" href=\"assets/styles.css\">"
      "<style><![CDATA[\njunky junk junk!\n]]\\></style>");

  ValidateStyleToLink(
      "<style>@import url(assets/styles.css);"
      "<!-- comment --></style>",

      "<link rel=\"stylesheet\" href=\"assets/styles.css\">"
      "<style><!-- comment --></style>");
}

class CssInlineImportToLinkFilterTestNoTags
    : public CssInlineImportToLinkFilterTest {
 public:
  virtual bool AddHtmlTags() const { return false; }
};

TEST_F(CssInlineImportToLinkFilterTestNoTags, UnclosedStyleGetsConverted) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  rewrite_driver()->AddFilters();
  ValidateExpected("unclosed_style",
                   "<style>@import url(assets/styles.css)",
                   "<link rel=\"stylesheet\" href=\"assets/styles.css\">");
}

TEST_F(CssInlineImportToLinkFilterTest, ConvertThenCacheExtend) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  // Cache for 100s.
  SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, kCssData, 100);

  ValidateExpected("script_to_link_then_cache_extend",
                   StrCat("<style>@import url(", kCssFile, ");</style>"),
                   StrCat("<link rel=\"stylesheet\" href=\"",
                          Encode(kCssSubdir, "ce", "0", kCssTail, "css"),
                          "\">"));
}

TEST_F(CssInlineImportToLinkFilterTest, DontConvertOrCacheExtend) {
  options()->EnableFilter(RewriteOptions::kInlineImportToLink);
  options()->EnableFilter(RewriteOptions::kExtendCacheCss);
  rewrite_driver()->AddFilters();
  // Cache for 100s.
  SetResponseWithDefaultHeaders(kCssFile, kContentTypeCss, kCssData, 100);

  // Note: This @import is not converted because it is preceded by a @foobar.
  const GoogleString kStyleElement = StrCat("<style>\n"
                                            "@foobar ;\n"
                                            "@import url(", kCssFile, ");\n",
                                            "body { color: red; }\n",
                                            "</style>");

  ValidateNoChanges("dont_touch_script_but_cache_extend", kStyleElement);
}

}  // namespace

}  // namespace net_instaweb
