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


// Unit-test the css utilities.

#include "net/instaweb/rewriter/public/css_util.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"
#include "webutil/css/selector.h"

namespace net_instaweb {

namespace css_util {

class CssUtilTest : public testing::Test {
 protected:
  CssUtilTest() { }

  GoogleMessageHandler message_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CssUtilTest);
};

TEST_F(CssUtilTest, TestGetDimensions) {
  HtmlParse html_parse(&message_handler_);
  HtmlElement* img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "height:50px;width:80px;border-width:0px;");

  scoped_ptr<StyleExtractor> extractor(new StyleExtractor(img));
  EXPECT_EQ(kHasBothDimensions, extractor->state());
  EXPECT_EQ(80, extractor->width());
  EXPECT_EQ(50, extractor->height());

  html_parse.DeleteNode(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;");
  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kNoDimensions, extractor->state());
  EXPECT_EQ(kNoValue, extractor->width());
  EXPECT_EQ(kNoValue, extractor->height());

  html_parse.DeleteNode(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;width:80px;");

  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kHasWidthOnly, extractor->state());
  EXPECT_EQ(kNoValue, extractor->height());
  EXPECT_EQ(80, extractor->width());

  html_parse.DeleteNode(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;height:200px");
  extractor.reset(new StyleExtractor(img));
  EXPECT_EQ(kHasHeightOnly, extractor->state());
  EXPECT_EQ(200, extractor->height());
  EXPECT_EQ(kNoValue, extractor->width());
  html_parse.DeleteNode(img);
}

TEST_F(CssUtilTest, TestAnyDimensions) {
  HtmlParse html_parse(&message_handler_);
  HtmlElement* img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "width:80px;border-width:0px;");
  scoped_ptr<StyleExtractor> extractor(new StyleExtractor(img));
  EXPECT_TRUE(extractor->HasAnyDimensions());
  EXPECT_EQ(kHasWidthOnly, extractor->state());

  html_parse.DeleteNode(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;background-color:blue;");
  extractor.reset(new StyleExtractor(img));
  EXPECT_FALSE(extractor->HasAnyDimensions());

  html_parse.DeleteNode(img);
  img = html_parse.NewElement(NULL, HtmlName::kImg);
  html_parse.AddAttribute(img, HtmlName::kStyle,
                          "border-width:0px;width:30px;height:40px");
  extractor.reset(new StyleExtractor(img));
  EXPECT_TRUE(extractor->HasAnyDimensions());
}

TEST_F(CssUtilTest, VectorizeMediaAttribute) {
  const char kSimpleMedia[] = "screen";
  const char* kSimpleVector[] = { "screen" };
  StringVector simple_expected(kSimpleVector,
                               kSimpleVector + arraysize(kSimpleVector));
  StringVector simple_actual;
  VectorizeMediaAttribute(kSimpleMedia, &simple_actual);
  EXPECT_TRUE(simple_expected == simple_actual);

  const char kUglyMessMedia[] = "screen,, ,printer , screen ";
  const char* kUglyMessVector[] = { "screen", "printer", "screen" };
  StringVector ugly_expected(kUglyMessVector,
                             kUglyMessVector + arraysize(kUglyMessVector));
  StringVector ugly_actual;
  VectorizeMediaAttribute(kUglyMessMedia, &ugly_actual);
  EXPECT_TRUE(ugly_expected == ugly_actual);

  const char kAllSubsumesMedia[] = "screen,, ,printer , all ";
  StringVector subsumes_actual;
  VectorizeMediaAttribute(kAllSubsumesMedia, &subsumes_actual);
  EXPECT_TRUE(subsumes_actual.empty());
}

TEST_F(CssUtilTest, StringifyMediaVector) {
  const char kSimpleMedia[] = "screen";
  const char* kSimpleVector[] = { "screen" };
  StringVector simple_vector(kSimpleVector,
                             kSimpleVector + arraysize(kSimpleVector));
  GoogleString simple_media = StringifyMediaVector(simple_vector);
  EXPECT_EQ(kSimpleMedia, simple_media);

  const char kMultipleMedia[] = "screen,printer,screen";
  const char* kMultipleVector[] = { "screen", "printer", "screen" };
  StringVector multiple_vector(kMultipleVector,
                               kMultipleVector + arraysize(kMultipleVector));
  GoogleString multiple_media = StringifyMediaVector(multiple_vector);
  EXPECT_EQ(kMultipleMedia, multiple_media);

  StringVector all_vector;
  GoogleString all_media = StringifyMediaVector(all_vector);
  EXPECT_EQ(css_util::kAllMedia, all_media);
}

TEST_F(CssUtilTest, IsComplexMediaQuery) {
  Css::MediaQuery query;
  EXPECT_FALSE(css_util::IsComplexMediaQuery(query));

  query.set_media_type(UTF8ToUnicodeText("screen"));
  EXPECT_FALSE(css_util::IsComplexMediaQuery(query));

  query.set_qualifier(Css::MediaQuery::ONLY);
  EXPECT_TRUE(css_util::IsComplexMediaQuery(query));

  query.set_qualifier(Css::MediaQuery::NOT);
  EXPECT_TRUE(css_util::IsComplexMediaQuery(query));

  query.set_qualifier(Css::MediaQuery::NO_QUALIFIER);
  EXPECT_FALSE(css_util::IsComplexMediaQuery(query));

  query.add_expression(new Css::MediaExpression(UTF8ToUnicodeText("foo"),
                                                UTF8ToUnicodeText("bar")));
  EXPECT_TRUE(css_util::IsComplexMediaQuery(query));
}

// Helper function.
Css::MediaQuery* NewSimpleMedium(const StringPiece& media_type) {
  Css::MediaQuery* query = new Css::MediaQuery;
  query->set_media_type(
      UTF8ToUnicodeText(media_type.data(), media_type.size()));
  return query;
}

TEST_F(CssUtilTest, ConvertMediaQueriesToStringVector) {
  Css::MediaQueries queries;
  queries.push_back(NewSimpleMedium("screen"));
  queries.push_back(NewSimpleMedium(""));
  queries.push_back(NewSimpleMedium("  "));
  queries.push_back(NewSimpleMedium("printer"));
  queries.push_back(NewSimpleMedium("all"));

  const char* kExpectedVector[] = { "screen", "printer", "all" };
  StringVector expected_vector(kExpectedVector,
                               kExpectedVector + arraysize(kExpectedVector));
  StringVector actual_vector;
  EXPECT_TRUE(ConvertMediaQueriesToStringVector(queries, &actual_vector));
  EXPECT_EQ(expected_vector, actual_vector);

  // Complex media queries are not converted.
  Css::MediaQuery* complex = new Css::MediaQuery;
  complex->set_qualifier(Css::MediaQuery::ONLY);
  complex->set_media_type(UTF8ToUnicodeText("screen"));
  queries.push_back(complex);
  EXPECT_FALSE(ConvertMediaQueriesToStringVector(queries, &actual_vector));
  EXPECT_TRUE(actual_vector.empty());
}

TEST_F(CssUtilTest, ConvertStringVectorToMediaQueries) {
  const char* kInputVector[] = { "screen", "", " ", "print ", " all ",
                                 "not braille and (color)" };
  StringVector input_vector(kInputVector,
                            kInputVector + arraysize(kInputVector));
  Css::MediaQueries queries;
  ConvertStringVectorToMediaQueries(input_vector, &queries);

  ASSERT_EQ(4, queries.size());
  EXPECT_STREQ("screen", UnicodeTextToUTF8(queries[0]->media_type()));
  EXPECT_EQ(Css::MediaQuery::NO_QUALIFIER, queries[0]->qualifier());
  EXPECT_EQ(0, queries[0]->expressions().size());

  EXPECT_STREQ("print", UnicodeTextToUTF8(queries[1]->media_type()));
  EXPECT_EQ(Css::MediaQuery::NO_QUALIFIER, queries[1]->qualifier());
  EXPECT_EQ(0, queries[1]->expressions().size());

  EXPECT_STREQ("all", UnicodeTextToUTF8(queries[2]->media_type()));
  EXPECT_EQ(Css::MediaQuery::NO_QUALIFIER, queries[2]->qualifier());
  EXPECT_EQ(0, queries[2]->expressions().size());

  // NOTE: We do not parse media strings. Only assign them to media_type().
  EXPECT_STREQ("not braille and (color)",
               UnicodeTextToUTF8(queries[3]->media_type()));
  EXPECT_EQ(Css::MediaQuery::NO_QUALIFIER, queries[3]->qualifier());
  EXPECT_EQ(0, queries[3]->expressions().size());
}

TEST_F(CssUtilTest, ClearVectorIfContainsMediaAll) {
  const char* kInputVector[] = { "screen", "", " ", "print " };
  StringVector input_vector(kInputVector,
                            kInputVector + arraysize(kInputVector));

  // 1. No 'all' in there.
  StringVector output_vector = input_vector;
  ClearVectorIfContainsMediaAll(&output_vector);
  EXPECT_TRUE(input_vector == output_vector);

  // 2. 'all' in there.
  output_vector = input_vector;
  output_vector.push_back(kAllMedia);
  ClearVectorIfContainsMediaAll(&output_vector);
  EXPECT_TRUE(output_vector.empty());
}

TEST_F(CssUtilTest, CanMediaAffectScreenTest) {
  EXPECT_TRUE(css_util::CanMediaAffectScreen(""));
  EXPECT_TRUE(css_util::CanMediaAffectScreen("  \t\n "));
  EXPECT_TRUE(css_util::CanMediaAffectScreen("  screen  "));
  EXPECT_TRUE(css_util::CanMediaAffectScreen("all\n"));
  // Case insensitive, handles multiple (possibly junk) media types.
  EXPECT_TRUE(css_util::CanMediaAffectScreen("print, audio ,, ,sCrEeN"));
  EXPECT_TRUE(css_util::CanMediaAffectScreen(
      "not!?#?;valid,screen,@%*%@*"));
  // Some cases that fail.
  EXPECT_FALSE(css_util::CanMediaAffectScreen("print"));
  EXPECT_FALSE(css_util::CanMediaAffectScreen("not screen"));
  EXPECT_FALSE(css_util::CanMediaAffectScreen("print screen"));
  EXPECT_FALSE(css_util::CanMediaAffectScreen("not!?#?;valid"));
  // We must handle CSS3 media queries (http://www.w3.org/TR/css3-mediaqueries/)
  EXPECT_TRUE(css_util::CanMediaAffectScreen("not print"));
  EXPECT_TRUE(css_util::CanMediaAffectScreen(
      "only screen and (max-device-width: 480px) "));
  // "(parens)" are equivalent to "all and (parens)" -- thus screen-affecting.
  EXPECT_TRUE(css_util::CanMediaAffectScreen("(monochrome)"));
  EXPECT_TRUE(css_util::CanMediaAffectScreen("(print)"));
  EXPECT_FALSE(css_util::CanMediaAffectScreen("not (audio or print)"));
}

TEST_F(CssUtilTest, JsDetectableSelector) {
  // We set up a series of selectors, parse them permissively,
  // and check the result.
  const char kSelectors[] =
      "a, a:visited, p, :visited, p:visited a, p :visited a, p > :hover > a, "
      "hjf98a7o, img[src^=\"mod_pagespeed_examples/images\"]";
  const char *kExpected[] =
      {"a", "a", "p", "", "p a", "p", "p",
       "hjf98a7o", "img[src^=\"mod_pagespeed_examples/images\"]"};
  Css::Parser parser(kSelectors);
  parser.set_preservation_mode(true);
  parser.set_quirks_mode(false);
  scoped_ptr<const Css::Selectors> selectors(parser.ParseSelectors());
  EXPECT_EQ(Css::Parser::kNoError, parser.errors_seen_mask());
  CHECK(selectors.get() != NULL);
  EXPECT_EQ(arraysize(kExpected), selectors->size());
  for (int i = 0; i < selectors->size(); ++i) {
    EXPECT_EQ(kExpected[i], JsDetectableSelector(*(*selectors)[i]));
  }
}

TEST_F(CssUtilTest, EliminateElementsNotIn) {
  const char* kSmallVector[] = { "screen", "print", "alternate" };
  StringVector small_vector(kSmallVector,
                            kSmallVector + arraysize(kSmallVector));
  std::sort(small_vector.begin(), small_vector.end());
  const char* kLargeVector[] = { "aural", "visual", "screen",
                                 "tactile", "print", "olfactory" };
  StringVector large_vector(kLargeVector,
                            kLargeVector + arraysize(kLargeVector));
  std::sort(large_vector.begin(), large_vector.end());
  const char* kIntersectVector[] = { "screen", "print" };
  StringVector intersect_vector(kIntersectVector,
                                kIntersectVector + arraysize(kIntersectVector));
  std::sort(intersect_vector.begin(), intersect_vector.end());
  StringVector empty_vector;
  StringVector input_vector;

  // 1. empty + empty => empty
  EliminateElementsNotIn(&input_vector, empty_vector);
  EXPECT_TRUE(input_vector.empty());

  // 2. empty + non-empty => non-empty
  EliminateElementsNotIn(&input_vector, small_vector);
  EXPECT_TRUE(input_vector == small_vector);

  // 3. non-empty + empty => non-empty
  EliminateElementsNotIn(&input_vector, empty_vector);
  EXPECT_TRUE(input_vector == small_vector);

  // 4. non-empty + non-empty => items only in both
  input_vector = small_vector;
  EliminateElementsNotIn(&input_vector, large_vector);
  EXPECT_TRUE(input_vector == intersect_vector);
  input_vector = large_vector;
  EliminateElementsNotIn(&input_vector, small_vector);
  EXPECT_TRUE(input_vector == intersect_vector);
}

}  // namespace css_util

}  // namespace net_instaweb
