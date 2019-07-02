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

//
// Unit tests for ScanFilter.

#include "net/instaweb/rewriter/public/scan_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

// Test fixture for ScanFilter unit tests.
class ScanFilterTest : public RewriteTestBase {
};

TEST_F(ScanFilterTest, EmptyPage) {
  // By default the base is the URL, which is set by ValidateNoChanges.
  const char kTestName[] = "empty_page";
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ(StrCat(kTestDomain, kTestName, ".html"),
               rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, SetBase) {
  // The default base (the URL) is overridden by a base tag.
  const char kTestName[] = "set_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head>"
                           "<base href=\"", kNewBase, "\">"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, RefsAfterBase) {
  // Check that we don't flag refs after the base tag.
  const char kTestName[] = "refs_after_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head profile='no problem'>"
                           "<base href=\"", kNewBase, "\">"
                           "<a href=\"help.html\">link</a>"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_FALSE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, RefsBeforeBase) {
  // Check that we do flag refs before the base tag.
  const char kTestName[] = "refs_after_base";
  const char kNewBase[] = "http://example.com/index.html";
  ValidateNoChanges(kTestName,
                    StrCat("<head>"
                           "<a href=\"help.html\">link</a>"
                           "<base href=\"", kNewBase, "\">"
                           "</head>"));
  EXPECT_STREQ(kNewBase, rewrite_driver()->base_url().Spec());
  EXPECT_TRUE(rewrite_driver()->refs_before_base());
}

TEST_F(ScanFilterTest, NoCharset) {
  // Check that the charset is empty if we don't set it in any way.
  const char kTestName[] = "no_charset";
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_TRUE(rewrite_driver()->containing_charset().empty());
}

TEST_F(ScanFilterTest, CharsetFromResponseHeaders) {
  // Check that the charset is taken from the response headers.
  const char kTestName[] = "charset_from_response_headers";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromBomDoesntOverride) {
  // Check that a BOM does not override the charset from the headers.
  const char kTestName[] = "charset_from_bom_doesnt_override";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromBom) {
  // Check that a BOM sets the charset.
  const char kTestName[] = "charset_from_bom";
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName, "<head></head>");
  EXPECT_STREQ(kUtf8Charset, rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagDoesntOverrideHeaders) {
  // Check that a meta tag does not override the charset from the headers.
  const char kTestName[] = "charset_from_meta_tag_doesnt_override_headers";
  ResponseHeaders headers;
  headers.MergeContentType("text/html; charset=iso-8859-1");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("iso-8859-1", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagDoesntOverrideBom) {
  // Check that a meta tag does not override the charset from a BOM.
  const char kTestName[] = "charset_from_meta_tag_doesnt_override_bom";
  SetDoctype(kUtf8Bom);
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"us-ascii\">"
                    "</head>");
  EXPECT_STREQ(kUtf8Charset, rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTag) {
  // Check that a meta tag sets the charset.
  const char kTestName[] = "charset_from_meta_tag";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("UTF-8", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromFirstMetaTag) {
  // Check that the first meta tag is used.
  const char kTestName[] = "charset_from_first_meta_tag";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=\"Content-Type\" "
                          "content=\"text/xml; charset=us-ascii\">"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("us-ascii", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromFirstMetaTagWithCharset) {
  // Check that the first meta tag is used.
  const char kTestName[] = "charset_from_first_meta_tag_with_charset";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=\"Content-Type\">"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("UTF-8", rewrite_driver()->containing_charset());
}

TEST_F(ScanFilterTest, CharsetFromMetaTagMissingQuotes) {
  // Check that the first meta tag is used even if it's missing the quotes.
  const char kTestName[] = "charset_from_meta_tag_missing_quotes";
  ValidateNoChanges(kTestName,
                    "<head>"
                    "<meta http-equiv=Content-Type "
                          "content=text/html; charset=us-ascii>"
                    "<meta charset=\"UTF-8\">"
                    "</head>");
  EXPECT_STREQ("us-ascii", rewrite_driver()->containing_charset());
}


TEST_F(ScanFilterTest, CspParse) {
  ResponseHeaders headers;
  headers.Add("Content-Security-Policy", "img-src https:");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges("csp_parse",
                    "<meta http-equiv=\"Content-Security-Policy\" "
                    "content=\"img-src www.example.com\">");
  EXPECT_EQ(2, rewrite_driver()->content_security_policy().policies_size());
  EXPECT_TRUE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("https://www.example.com/foo.png"), CspDirective::kImgSrc));
  EXPECT_FALSE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("http://www.example.com/foo.png"), CspDirective::kImgSrc));
  EXPECT_FALSE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("https://www.example.org/foo.png"), CspDirective::kImgSrc));
  EXPECT_FALSE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("http://www.example.org/foo.png"), CspDirective::kImgSrc));
}

TEST_F(ScanFilterTest, CspParseOff) {
  options()->set_honor_csp(false);

  ResponseHeaders headers;
  headers.Add("Content-Security-Policy", "img-src https:");
  rewrite_driver()->set_response_headers_ptr(&headers);
  ValidateNoChanges("csp_parse",
                    "<meta http-equiv=\"Content-Security-Policy\" "
                    "content=\"img-src www.example.com\">");
  EXPECT_EQ(0, rewrite_driver()->content_security_policy().policies_size());
  EXPECT_TRUE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("https://www.example.com/foo.png"), CspDirective::kImgSrc));
  EXPECT_TRUE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("http://www.example.com/foo.png"), CspDirective::kImgSrc));
  EXPECT_TRUE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("https://www.example.org/foo.png"), CspDirective::kImgSrc));
  EXPECT_TRUE(rewrite_driver()->IsLoadPermittedByCsp(
      GoogleUrl("http://www.example.org/foo.png"), CspDirective::kImgSrc));
}

TEST_F(ScanFilterTest, CspBase1) {
  rewrite_driver()->AddFilters();
  EnableDebug();
  // The default base (the URL) is overridden by a base tag.
  static const char kTestName[] = "set_base";
  static const char kNewBase[] = "http://example.com/index.html";
  static const char kCsp[] = "<meta http-equiv=\"Content-Security-Policy\" "
                             "content=\"img-src www.example.com\">";
  ValidateNoChanges(kTestName,
                    StrCat("<head>",
                           kCsp,
                           "<base href=\"", kNewBase, "\">"
                           "</head>"));
  EXPECT_FALSE(rewrite_driver()->other_base_problem());
}


TEST_F(ScanFilterTest, CspBase2) {
  rewrite_driver()->AddFilters();
  EnableDebug();
  // The default base (the URL) is overridden by a base tag.
  static const char kTestName[] = "set_base";
  static const char kNewBase[] = "http://example.com/index.html";
  static const char kCsp[] = "<meta http-equiv=\"Content-Security-Policy\" "
                             "content=\"base-uri www.example.com\">";
  ValidateExpected(
      kTestName,
      StrCat("<head>",
            kCsp,
            "<base href=\"", kNewBase, "\">"
            "</head>"),
      StrCat("<head>",
            kCsp,
            "<base href=\"", kNewBase, "\">"
            "<!--Unable to check safety of a base with CSP base-uri, "
            "proceeding conservatively.-->"
            "</head>"));
  EXPECT_TRUE(rewrite_driver()->other_base_problem());
}

}  // namespace net_instaweb
