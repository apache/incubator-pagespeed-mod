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


#include "pagespeed/kernel/base/escaping.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

class EscapingTest : public testing::Test {
 public:
  void ExpectEscape(const char* name, const char* expect, const char* in) {
    GoogleString out_unquoted, out_quoted;
    EscapeToJsStringLiteral(in, false, &out_unquoted);
    EscapeToJsStringLiteral(in, true, &out_quoted);
    EXPECT_STREQ(expect, out_unquoted) << " on test " << name;
    EXPECT_STREQ(StrCat("\"", expect, "\""), out_quoted) << " on test " << name;
  }
};

TEST_F(EscapingTest, JsEscapeBasic) {
  ExpectEscape("normal", "abc", "abc");
  ExpectEscape("quote", "abc\\\"d", "abc\"d");
  ExpectEscape("backslash", "abc\\\\d", "abc\\d");
  ExpectEscape("carriage_control", "abc\\n\\rde", "abc\n\rde");
}

TEST_F(EscapingTest, JsAvoidCloseScript) {
  ExpectEscape("avoid_close_script", "Foo<\\/script>Bar", "Foo</script>Bar");
  ExpectEscape("not_heavily_excessive_escaping", "/s", "/s");
}

TEST_F(EscapingTest, JsAvoidCloseScriptSpace) {
  ExpectEscape("avoid_close_script2",
               "Foo<\\/script  >Bar", "Foo</script  >Bar");
}

TEST_F(EscapingTest, JsAvoidCloseScriptCase) {
  ExpectEscape("avoid_close_script3",
               "Foo<\\/scrIpt>Bar", "Foo</scrIpt>Bar");
}

TEST_F(EscapingTest, JsCloseScriptConservativeBehavior) {
  // We don't need to escape </scripty>, but it's safe to do so.
  ExpectEscape("close_script_conservative",
               "Foo<\\/scripty>Bar", "Foo</scripty>Bar");
}

TEST_F(EscapingTest, JsSingleQuotes) {
  GoogleString out_unquoted, out_quoted;
  const char kIn[] = "foo'";
  EscapeToJsStringLiteral(kIn, false, &out_unquoted);
  EscapeToJsStringLiteral(kIn, true, &out_quoted);
  EXPECT_STREQ("foo\\'", out_unquoted);
  EXPECT_STREQ("\"foo'\"", out_quoted);
}

TEST_F(EscapingTest, JsAvoidWeirdParsingSequence) {
  // Some sequences have an effect on HTML parsing, so we want to avoid them.
  GoogleString out;
  EscapeToJsStringLiteral("a <ScrIpt", false, &out);
  EXPECT_EQ("a \\u003cScrIpt", out);

  out.clear();
  EscapeToJsStringLiteral("Foo <!-- ", false, &out);
  EXPECT_EQ("Foo \\u003c!-- ", out);

  out.clear();
  EscapeToJsStringLiteral("Bar ---> ", false, &out);
  EXPECT_EQ("Bar -\\u002d-> ", out);
}

TEST_F(EscapingTest, JsDontEscapeWayTooMuch) {
  GoogleString out;
  EscapeToJsStringLiteral("<div", false, &out);
  EXPECT_EQ("<div", out);

  out.clear();
  EscapeToJsStringLiteral("-----!", false, &out);
  EXPECT_EQ("-----!", out);
}

TEST_F(EscapingTest, JsonEscapeBasic) {
  GoogleString out;
  EscapeToJsonStringLiteral("abc\1\3\n\t\"\\", true, &out);
  EXPECT_EQ("\"abc\\u0001\\u0003\\u000a\\u0009\\u0022\\u005c\"", out);
}

TEST_F(EscapingTest, JsonEscapeAppend) {
  GoogleString out;
  EscapeToJsonStringLiteral("ab", true, &out);
  EscapeToJsStringLiteral("cd", false, &out);
  EXPECT_EQ("\"ab\"cd", out);
}

}  // namespace

}  // namespace net_instaweb
