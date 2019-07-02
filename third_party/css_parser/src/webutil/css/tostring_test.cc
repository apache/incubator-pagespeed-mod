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



#include "webutil/css/tostring.h"

#include <string>

#include <memory>
#include "base/scoped_ptr.h"

#include "base/stringprintf.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "webutil/css/parser.h"
#include "webutil/css/string.h"

namespace {

#define TESTDECLARATIONS(css) TestDeclarations(css, __LINE__)
#define TESTSTYLESHEET(css) TestStylesheet(css, __LINE__)

class ToStringTest : public testing::Test {
 protected:
  // Checks if ParseDeclarations()->ToString() returns identity.
  void TestDeclarations(const string& css, int line) {
    SCOPED_TRACE(StringPrintf("at line %d", line));
    Css::Parser parser(css);
    scoped_ptr<Css::Declarations> decls(parser.ParseDeclarations());
    EXPECT_EQ(css, decls->ToString());
  }

  // Checks if ParseStylesheet()->ToString() returns identity.
  void TestStylesheet(const string& css, int line) {
    SCOPED_TRACE(StringPrintf("at line %d", line));
    Css::Parser parser(css);
    scoped_ptr<Css::Stylesheet> stylesheet(parser.ParseStylesheet());
    EXPECT_EQ(css, stylesheet->ToString());
  }
};

TEST_F(ToStringTest, declarations) {
  TESTDECLARATIONS("left: inherit; "
                   "color: #abcdef; "
                   "content: \"text\"; "
                   "top: 1; right: 2px !important; "
                   "background-image: url(link\\(a,b,\\\"c\\\"\\).html)");
  TESTDECLARATIONS("content: counter(); clip: rect(auto 1px 2em auto)");
  TESTDECLARATIONS("font-family: arial,serif,\"Courier New\"");

  // FONT is special
  Css::Parser parser("font: 3px/1.1 Arial");
  scoped_ptr<Css::Declarations> decls(parser.ParseDeclarations());
  EXPECT_EQ("font: 3px/1.1 Arial; "
            "font-style: normal; font-variant: normal; font-weight: normal; "
            "font-size: 3px; line-height: 1.1; font-family: Arial",
            decls->ToString());
}

TEST_F(ToStringTest, selectors) {
  TESTSTYLESHEET("/* AUTHOR */\n\n\n\n"
                 "a, *, b#id, c.class, :hover:focus {top: 1}\n");
  TESTSTYLESHEET("/* AUTHOR */\n\n\n\n"
                 "table[width], [disable=\"no\"], [x~=\"y\"], [lang|=\"fr\"] "
                 "{top: 1}\n");
  TESTSTYLESHEET("/* AUTHOR */\n\n\n\n"
                 "img[height=\"1\"] {display: block}\n"
                 "[class^=\"icon-\"], [class*=\" icon-\"] {top: 1}\n");
  TESTSTYLESHEET("/* AUTHOR */\n\n\n\n"
                 "a > b, a + b, a b + c > d > e f {top: 1}\n");
}

TEST_F(ToStringTest, misc) {
  TESTSTYLESHEET("/* AUTHOR */\n\n"
                 "@import url(\"a.html\") ;\n"
                 "@import url(\"b.html\") print;\n"
                 "\n"
                 "@media print, screen { a {top: 1} }\n"
                 "b {color: #ff0000}\n");

  scoped_ptr<Css::Parser> parser(new Css::Parser("a {top: 1}"));
  scoped_ptr<Css::Stylesheet> stylesheet(parser->ParseStylesheet());
  stylesheet->set_type(Css::Stylesheet::SYSTEM);
  EXPECT_EQ("/* SYSTEM */\n\n\n\n"
            "a {top: 1}\n",
            stylesheet->ToString());

  // Make sure we correctly deal with escaped newline.
  parser.reset(new Css::Parser("a { content: 'line 1\\\n"
                               "line 2'; }"));
  stylesheet.reset(parser->ParseStylesheet());
  EXPECT_EQ("/* AUTHOR */\n\n\n\n"
            "a {content: \"line 1line 2\"}\n", stylesheet->ToString());
}

TEST_F(ToStringTest, SpecialChars) {
  Css::Parser parser("content: \"Special chars: \\n\\r\\t\\A \\D \\9\"");
  scoped_ptr<Css::Declarations> decls(parser.ParseDeclarations());
  EXPECT_EQ("content: \"Special chars: nrt\\A \\D \\9 \"", decls->ToString());
}

TEST_F(ToStringTest, MediaQueries) {
  TESTSTYLESHEET("/* AUTHOR */\n\n"
                 "@import url(\"a.css\") not screen;\n"
                 "@import url(\"b.css\") (color) and (max-width: 38px);\n"
                 "\n"
                 "@media only print and (color) { .a {right: 1} }\n");
}

TEST_F(ToStringTest, EscapeIdentifier) {
  // We should escape all special chars, but not UTF8.
  EXPECT_EQ("\\*Hello\\,\\ दुनिया\\!", Css::EscapeIdentifier("*Hello, दुनिया!"));
}

}  // namespace
