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

#include "net/instaweb/rewriter/public/support_noscript_filter.h"

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

const char kChromeUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.4 (KHTML, like Gecko) "
    "Chrome/22.0.1229.64 Safari/537.4";
const char kUnsupportedUserAgent[] = "Unsupported";

class SupportNoscriptFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->EnableFilter(RewriteOptions::kDelayImages);
    SetResponseWithDefaultHeaders(
        "http://test.com/1.jpeg", kContentTypeJpeg,
        "bogusimage but it is not parsed", 100 /* sec */);
    SetResponseWithDefaultHeaders(
        "http://test.com/2.jpeg", kContentTypeJpeg,
        "bogusimage but it is not parsed", 100 /* sec */);
  }
};

TEST_F(SupportNoscriptFilterTest, TestNoscript) {
  GoogleString input_html =
      "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/></body>";
  GoogleString output_html =
      "<head></head><body>"
      "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;url="
      "'http://test.com/support_noscript&#39;%22.html?PageSpeed=noscript'\" "
      "/><style><!--table,div,span,font,p{display:none} --></style>"
      "<div style=\"display:block\">Please click <a href="
      "\"http://test.com/support_noscript&#39;%22.html?PageSpeed=noscript\">"
      "here</a> if you are not redirected within a few seconds.</div>"
      "</noscript><img src=\"http://test.com/1.jpeg\"/></body>";
  SetCurrentUserAgent(kChromeUserAgent);
  ValidateExpected("support_noscript'\"", input_html, output_html);
}

TEST_F(SupportNoscriptFilterTest, TestNoscriptMultipleBodies) {
  GoogleString input_html =
      "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  GoogleString output_html =
      "<head></head><body>"
      "<noscript><meta HTTP-EQUIV=\"refresh\" content=\"0;"
      "url='http://test.com/support_noscript.html?PageSpeed=noscript'\" />"
      "<style><!--table,div,span,font,p{display:none} --></style>"
      "<div style=\"display:block\">Please click "
      "<a href=\"http://test.com/support_noscript.html?PageSpeed=noscript\">"
      "here</a> if you are not redirected within a few seconds.</div>"
      "</noscript><img src=\"http://test.com/1.jpeg\"/></body>"
      "<body><img src=\"http://test.com/2.jpeg\"/></body>";
  SetCurrentUserAgent(kChromeUserAgent);
  ValidateExpected("support_noscript", input_html, output_html);
}

TEST_F(SupportNoscriptFilterTest, TestNoBody) {
  GoogleString input_html =
      "<head></head>";
  SetCurrentUserAgent(kChromeUserAgent);
  ValidateExpected("support_noscript", input_html, input_html);
}

TEST_F(SupportNoscriptFilterTest, TestUnsupportedUserAgent) {
  GoogleString input_html =
      "<head></head><body>"
      "<img src=\"http://test.com/1.jpeg\"/></body>";
  SetCurrentUserAgent(kUnsupportedUserAgent);
  ValidateExpected("support_noscript", input_html, input_html);
}

}  // namespace net_instaweb
