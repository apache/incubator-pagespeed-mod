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


#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

namespace {

class CssMoveToHeadFilterTest : public RewriteTestBase {
 protected:
};

TEST_F(CssMoveToHeadFilterTest, MovesCssToHead) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  static const char html_input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>\n"  // no indent
      "  <style type='text/css'>a {color: red }</style>\n"
      "  World!\n"
      "  <link rel='stylesheet' href='c.css' type='text/css'>\n"
      "</body>\n";

  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<link rel='stylesheet' href='a.css' type='text/css'>"  // no newline
      "<link rel='stylesheet' href='b.css' type='text/css'>"  // no newline
      "<style type='text/css'>a {color: red }</style>"        // no newline
      "<link rel='stylesheet' href='c.css' type='text/css'>"  // no newline
      "</head>\n"
      "<body>\n"
      "  Hello,\n"
      "  \n"
      "  \n"
      "  World!\n"
      "  \n"
      "</body>\n";

  ValidateExpected("move_css_to_head", html_input, expected_output);
}

TEST_F(CssMoveToHeadFilterTest, DoesntMoveOutOfNoScript) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <style>a {color: red}</style>\n"
      "  <noscript>\n"
      "    <link rel='stylesheet' href='a.css'>\n"
      "  </noscript>\n"
      "  <style>p {color: blue}</style>\n"
      "</body>\n";
  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<style>a {color: red}</style>"
      "</head>\n"
      "<body>\n"
      "  \n"
      "  <noscript>\n"
      "    <link rel='stylesheet' href='a.css'>\n"
      "  </noscript>\n"
      "  <style>p {color: blue}</style>\n"
      "</body>\n";

  ValidateExpected("noscript", input_html, expected_html);
}

TEST_F(CssMoveToHeadFilterTest, DoesntMoveScopedStyle) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <style>div {color: green}</style>\n"
      "  <style scoped>a {color: red}</style>\n"
      "  <style scoped='scoped'>p {color: blue}</style>\n"
      "  <link rel='stylesheet' href='a.css'>\n"
      "  <p>Blue with a <a href='scoped_style.html'>red link</a>"
      "</body>\n";
  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<style>div {color: green}</style>"
      "</head>\n"
      "<body>\n"
      "  \n"
      "  <style scoped>a {color: red}</style>\n"
      "  <style scoped='scoped'>p {color: blue}</style>\n"
      "  <link rel='stylesheet' href='a.css'>\n"
      "  <p>Blue with a <a href='scoped_style.html'>red link</a>"
      "</body>\n";
  ValidateExpected("scoped_style", input_html, expected_html);
}

TEST_F(CssMoveToHeadFilterTest, MovePastScopedDiv) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  static const char input_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <style>div {color: green}</style>\n"
      "  <div scoped>Just a div, move along!</div>\n"
      "  <style>p {color: blue}</style>\n"
      "  <link rel='stylesheet' href='a.css'>\n"
      "</body>\n";
  static const char expected_html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "<style>div {color: green}</style>"
      "<style>p {color: blue}</style>"
      "<link rel='stylesheet' href='a.css'>"
      "</head>\n"
      "<body>\n"
      "  \n"
      "  <div scoped>Just a div, move along!</div>\n"
      "  \n"
      "  \n"
      "</body>\n";
  ValidateExpected("scoped_div", input_html, expected_html);
}

TEST_F(CssMoveToHeadFilterTest, DoesntReorderCss) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  static const char html[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "</head>\n"
      "<body>\n"
      "  <link rel='stylesheet' href='a.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "  <style type='text/css'>a { color: red }</style>\n"
      "  <link rel='stylesheet' href='d.css' type='text/css'>\n"
      "</body>\n";

  Parse("no_reorder_css", html);
  LOG(INFO) << "output_buffer_ = " << output_buffer_;
  size_t a_loc = output_buffer_.find("href='a.css'");
  size_t b_loc = output_buffer_.find("href='b.css'");
  size_t c_loc = output_buffer_.find("a { color: red }");
  size_t d_loc = output_buffer_.find("href='d.css'");

  // Make sure that all attributes are in output_buffer_ ...
  EXPECT_NE(output_buffer_.npos, a_loc);
  EXPECT_NE(output_buffer_.npos, b_loc);
  EXPECT_NE(output_buffer_.npos, c_loc);
  EXPECT_NE(output_buffer_.npos, d_loc);

  // ... and that they are still in the right order (specifically, that
  // the last link wasn't moved above the style).
  EXPECT_LE(a_loc, b_loc);
  EXPECT_LE(b_loc, c_loc);
  EXPECT_LE(c_loc, d_loc);
}

TEST_F(CssMoveToHeadFilterTest, MovesAboveFirstScript) {
  AddFilter(RewriteOptions::kMoveCssAboveScripts);

  static const char input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  <script src='b.js'></script>\n"
      "  <!-- Comment -->\n"
      "  <style>.foo { color: red }</style>\n"
      "  <script src='c.js'></script>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  <link rel='stylesheet' href='e.css'>\n"
      "</head>\n"
      "<body>\n"
      "  <link rel='stylesheet' type='text/css' href='f.css'>\n"
      "</body>\n";
  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  "
      "<style>.foo { color: red }</style>"
      "<link rel='stylesheet' href='e.css'>"
      "<link rel='stylesheet' type='text/css' href='f.css'>"
      "<script src='b.js'></script>\n"
      "  <!-- Comment -->\n"
      "  \n"
      "  <script src='c.js'></script>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  \n"
      "</head>\n"
      "<body>\n"
      "  \n"
      "</body>\n";
  ValidateExpected("move_above_first_script", input, expected_output);
}

TEST_F(CssMoveToHeadFilterTest, MovesAboveScriptAfterHead) {
  AddFilter(RewriteOptions::kMoveCssAboveScripts);

  static const char input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  <!-- Comment -->\n"
      "  <style>.foo { color: red }</style>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  <link rel='stylesheet' href='e.css'>\n"
      "</head>\n"
      "<body>\n"
      "  <script src='b.js'></script>\n"
      "  <link rel='stylesheet' type='text/css' href='f.css'>\n"
      "</body>\n";
  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  <!-- Comment -->\n"
      "  <style>.foo { color: red }</style>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  <link rel='stylesheet' href='e.css'>\n"
      "</head>\n"
      "<body>\n"
      "  <link rel='stylesheet' type='text/css' href='f.css'>"
      "<script src='b.js'></script>\n"
      "  \n"
      "</body>\n";
  ValidateExpected("move_above_script_after_head", input, expected_output);
}

TEST_F(CssMoveToHeadFilterTest, MovesToHeadEvenIfScriptAfter) {
  options()->EnableFilter(RewriteOptions::kMoveCssToHead);
  options()->EnableFilter(RewriteOptions::kMoveCssAboveScripts);
  rewrite_driver_->AddFilters();

  static const char input[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  <!-- Comment -->\n"
      "  <style>.foo { color: red }</style>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  <link rel='stylesheet' href='e.css'>\n"
      "</head>\n"
      "<body>\n"
      "  <script src='b.js'></script>\n"
      "  <link rel='stylesheet' type='text/css' href='f.css'>\n"
      "</body>\n";
  static const char expected_output[] =
      "<head>\n"
      "  <title>Example</title>\n"
      "  <link rel='stylesheet' type='text/css' href='a.css'>\n"
      "  <meta name='application-name' content='Foo'>\n"
      "  <!-- Comment -->\n"
      "  <style>.foo { color: red }</style>\n"
      "  <link rel='icon' href='d.png'>\n"
      "  <link rel='stylesheet' href='e.css'>\n"
      "<link rel='stylesheet' type='text/css' href='f.css'>"
      "</head>\n"
      "<body>\n"
      "  <script src='b.js'></script>\n"
      "  \n"
      "</body>\n";
  ValidateExpected("move_above_first_script", input, expected_output);
}

TEST_F(CssMoveToHeadFilterTest, MoveToHeadFlushEdge) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html>\n"
                              "  <head>\n"
                              "    <title>Example</title>");
  rewrite_driver()->Flush();
  // Make it so that the </head> is the first thing in this flush window.
  // Test to make sure we don't break this corner case.
  rewrite_driver()->ParseText(
      // NOTE: It is important there are not spaces, etc. before the <script>
      // tag, those would become the first event.
      "</head>\n"
      "  <body>\n"
      "    <link rel='stylesheet' type='text/css' href='f.css'>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("\n"
                              "  </body>\n"
                              "</html>\n");
  rewrite_driver()->FinishParse();

  // Check that we do still move the <link> tag to the edge of the flush window.
  // And more importantly that we don't lose the <link> or crash, etc.
  EXPECT_EQ("<html>\n"
            "  <head>\n"
            "    <title>Example</title>"
            "<link rel='stylesheet' type='text/css' href='f.css'>"
            "</head>\n"
            "  <body>\n"
            "    \n"
            "  </body>\n"
            "</html>\n", output_buffer_);
}

TEST_F(CssMoveToHeadFilterTest, MoveToHeadOverFlushEdge) {
  AddFilter(RewriteOptions::kMoveCssToHead);

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html>\n"
                              "  <head>\n"
                              "    <title>Example</title>"
                              "</head>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(
      "\n"
      "  <body>\n"
      "    <link rel='stylesheet' type='text/css' href='f.css'>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("\n"
                              "  </body>\n"
                              "</html>\n");
  rewrite_driver()->FinishParse();

  // </head> is out of flush window at rewrite time, so we don't move anything.
  EXPECT_EQ("<html>\n"
            "  <head>\n"
            "    <title>Example</title>"
            "</head>\n"
            "  <body>\n"
            "    <link rel='stylesheet' type='text/css' href='f.css'>\n"
            "  </body>\n"
            "</html>\n", output_buffer_);
}

TEST_F(CssMoveToHeadFilterTest, MoveAboveScriptsFlushEdge) {
  AddFilter(RewriteOptions::kMoveCssAboveScripts);

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html>\n"
                              "  <head>\n"
                              "    <title>Example</title>");
  rewrite_driver()->Flush();
  // Make it so that the <script> is the first thing in this flush window.
  // Test to make sure we don't break this corner case.
  rewrite_driver()->ParseText(
      // NOTE: It is important there are not spaces, etc. before the <script>
      // tag, those would become the first event.
      "<script src='b.js'></script>\n"
      "  </head>\n"
      "  <body>\n"
      "    <link rel='stylesheet' type='text/css' href='f.css'>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("\n"
                              "  </body>\n"
                              "</html>\n");
  rewrite_driver()->FinishParse();

  // Check that we do still move the <link> tag to the edge of the flush window.
  // And more importantly that we don't lose the <link> or crash, etc.
  EXPECT_EQ("<html>\n"
            "  <head>\n"
            "    <title>Example</title>"
            "<link rel='stylesheet' type='text/css' href='f.css'>"
            "<script src='b.js'></script>\n"
            "  </head>\n"
            "  <body>\n"
            "    \n"
            "  </body>\n"
            "</html>\n", output_buffer_);
}

TEST_F(CssMoveToHeadFilterTest, MoveAboveScriptsOverFlushEdge) {
  AddFilter(RewriteOptions::kMoveCssAboveScripts);

  SetupWriter();
  rewrite_driver()->StartParse(kTestDomain);
  rewrite_driver()->ParseText("<html>\n"
                              "  <head>\n"
                              "    <title>Example</title>"
                              "<script src='b.js'></script>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText(
      "\n"
      "  </head>\n"
      "  <body>\n"
      "    <link rel='stylesheet' type='text/css' href='f.css'>");
  rewrite_driver()->Flush();
  rewrite_driver()->ParseText("\n"
                              "  </body>\n"
                              "</html>\n");
  rewrite_driver()->FinishParse();

  // <script> is out of flush window at rewrite time, so we don't move anything.
  // TODO(sligocki): Technically, we could move it into <head> still, but
  // I'm guessing this situation won't come up too much.
  EXPECT_EQ("<html>\n"
            "  <head>\n"
            "    <title>Example</title>"
            "<script src='b.js'></script>\n"
            "  </head>\n"
            "  <body>\n"
            "    <link rel='stylesheet' type='text/css' href='f.css'>\n"
            "  </body>\n"
            "</html>\n", output_buffer_);
}

}  // namespace

}  // namespace net_instaweb
