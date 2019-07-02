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

#include "pagespeed/opt/ads/show_ads_snippet_parser.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
namespace ads_attribute {
namespace {

class ShowAdsSnippetParserTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    ::testing::Test::SetUp();
    parsed_attributes_.clear();
  }

  void CheckParsedResults() {
    EXPECT_EQ(4, parsed_attributes_.size());
    EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
    EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
    EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
    EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
  }

  bool ParseStrict(const GoogleString& snippet) {
    return parser_.ParseStrict(
        snippet, &tokenizer_patterns_, &parsed_attributes_);
  }

  ShowAdsSnippetParser parser_;
  pagespeed::js::JsTokenizerPatterns tokenizer_patterns_;
  std::map<GoogleString, GoogleString> parsed_attributes_;
};

TEST_F(ShowAdsSnippetParserTest, ParseStrictEmpty) {
  EXPECT_TRUE(ParseStrict(""));

  EXPECT_EQ(0, parsed_attributes_.size());
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValid) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidSingleQuote) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_client = 'ca-pub-xxxxxxxxxxxxxx';"
      "/* ad served */"
      "google_ad_slot = 'xxxxxxxxx';"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidEmptyLines) {
  EXPECT_TRUE(ParseStrict(
      "\n\n\n\n\n"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";\n\n\n\n"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidEmptyStatement) {
  EXPECT_TRUE(ParseStrict(
      "\n\n\n\n\n"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";;;;;"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidWithoutSemicolon) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\"\n"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\"\n"
      "google_ad_width = 728\n"
      "google_ad_height = 90\n"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictValidWithEnclosingCommentTag) {
  EXPECT_TRUE(ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->"));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictValidWithEnclosingCommentTagAndWhitespaces) {
  EXPECT_TRUE(ParseStrict(
      "    <!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->    "));
  CheckParsedResults();
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormat) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"728x90\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));

  EXPECT_EQ(5, parsed_attributes_.size());
  EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
  EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
  EXPECT_EQ("728x90", parsed_attributes_["google_ad_format"]);
  EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
  EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
}

TEST_F(ShowAdsSnippetParserTest, ParseWeirdGoogleAdFormat1) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_format = \"728x90_as\";"));

  EXPECT_EQ(1, parsed_attributes_.size());
  EXPECT_EQ("728x90_as", parsed_attributes_["google_ad_format"]);
}

TEST_F(ShowAdsSnippetParserTest, ParseWeirdGoogleAdFormat2) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_format = \"180x90_0ads_al_s\";"));

  EXPECT_EQ(1, parsed_attributes_.size());
  EXPECT_EQ("180x90_0ads_al_s", parsed_attributes_["google_ad_format"]);
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormatWithWhiteSpaces) {
  EXPECT_TRUE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"  728x90  \";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));

  EXPECT_EQ(5, parsed_attributes_.size());
  EXPECT_EQ("ca-pub-xxxxxxxxxxxxxx", parsed_attributes_["google_ad_client"]);
  EXPECT_EQ("xxxxxxxxx", parsed_attributes_["google_ad_slot"]);
  EXPECT_EQ("  728x90  ", parsed_attributes_["google_ad_format"]);
  EXPECT_EQ("728", parsed_attributes_["google_ad_width"]);
  EXPECT_EQ("90", parsed_attributes_["google_ad_height"]);
}

TEST_F(ShowAdsSnippetParserTest, ParseShortAttribute) {
  EXPECT_TRUE(
      ParseStrict("google_language = \"de\""));

  EXPECT_EQ(1, parsed_attributes_.size());
  EXPECT_EQ("de", parsed_attributes_["google_language"]);
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictGoogleAdFormatWithUnexpectedPrefix) {
  EXPECT_FALSE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"test_722x92\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictGoogleAdFormatWithUnexpectedEnds) {
  EXPECT_FALSE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_format = \"test_722x92_rimg\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
}

TEST_F(ShowAdsSnippetParserTest,
       ParseStrictInvalidAttributeNameNotStartedWithGoogle) {
  EXPECT_FALSE(ParseStrict(
      "<!--"
      "dgoogle_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"  // Invalid.
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidAttributeNameIllegalChar) {
  EXPECT_FALSE(ParseStrict(
      "google_ad_invalid-name = \"ca-pub-xxxxxxxxxxxxxx\";"  // Invalid.
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidDuplicate) {
  EXPECT_FALSE(ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_slot = \"xxxxxxxxy\";"  // Duplicate assignment
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidMissingSemicolon) {
  EXPECT_FALSE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\" "  // ; or \n is missing
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\"\n"
      "google_ad_width = 728\n"
      "google_ad_height = 90\n"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidModified) {
  EXPECT_FALSE(ParseStrict(
      "<!--"
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "if (test) google_ad_client = \"ca-pub-xxxxxxxxxxxxxy\";"  // Invalid
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = 90;"
      "//-->"));
}

TEST_F(ShowAdsSnippetParserTest, ParseStrictInvalidAssignment) {
  EXPECT_FALSE(ParseStrict(
      "google_ad_client = \"ca-pub-xxxxxxxxxxxxxx\";"
      "/* ad served */"
      "google_ad_slot = \"xxxxxxxxx\";"
      "google_ad_width = 728;"
      "google_ad_height = google_ad_width;"));
}

TEST_F(ShowAdsSnippetParserTest, ParseColorArray) {
  // TODO(morlovich): This could in principle be handled, but it's unclear it's
  // common enough to be worth the effort.
  EXPECT_FALSE(ParseStrict(
      "google_color_border = [\"336699\",\"CC99CC\",\"578A24\",\"191933\"]"));
}

}  // namespace
}  // namespace ads_attribute
}  // namespace net_instaweb
