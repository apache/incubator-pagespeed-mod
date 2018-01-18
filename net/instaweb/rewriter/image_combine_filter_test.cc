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


#include <algorithm>
#include <memory>

#include "net/instaweb/rewriter/public/css_rewrite_test_base.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

// Filenames of resource files.
const char kBikePngFile[] = "BikeCrashIcn.png";
const char kCuppaPngFile[] = "Cuppa.png";
const char kPuzzleJpgFile[] = "Puzzle.jpg";
const char kChefGifFile[] = "IronChef2.gif";

const char* kHtmlTemplate3Divs = "<head><style>"
    "#div1{background:url(%s) 0 0;width:10px;height:10px}"
    "#div2{background:url(%s) 0 %s;width:%s;height:10px}"
    "#div3{background:url(%s) 0 %s;width:10px;height:10px}"
    "</style></head>";

// Image spriting tests.
class CssImageCombineTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kSpriteImages);
    CssRewriteTestBase::SetUp();
    AddFileToMockFetcher(StrCat(kTestDomain, kBikePngFile), kBikePngFile,
                         kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile), kCuppaPngFile,
                         kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kChefGifFile), kChefGifFile,
                         kContentTypeGif, 100);
  }

  void TestSpriting(const char* bike_position, const char* expected_position,
                    bool should_sprite) {
    const GoogleString sprite_string =
        Encode("", "is", "0", MultiUrl(kCuppaPngFile, kBikePngFile), "png");
    const char* sprite = sprite_string.c_str();
    // The JPEG will not be included in the sprite because we only handle PNGs.
    const char* html = "<head><style>"
        "#div1{background-image:url(%s);"
        "background-position:0 0;width:10px;height:10px}"
        "#div2{background:transparent url(%s);"
        "background-position:%s;width:10px;height:10px}"
        "#div3{background-image:url(%s);width:10px;height:10px}"
        "</style></head>";
    GoogleString before = StringPrintf(
        html, kCuppaPngFile, kBikePngFile, bike_position, kPuzzleJpgFile);
    GoogleString after = StringPrintf(
        html, sprite, sprite, expected_position, kPuzzleJpgFile);

    ValidateExpected("sprites_images", before, should_sprite ? after : before);

    // Try it again, this time using the background shorthand with a couple
    // different orderings
    const char* html2 = "<head><style>"
        "#div1{background:0 0 url(%s) no-repeat transparent scroll;"
        "width:10px;height:10px}"
        "#div2{background:url(%s) %s repeat fixed;width:10px;height:10px}"
        "#div3{background-image:url(%s);width:10px;height:10px}"
        "</style></head>";

    before = StringPrintf(
        html2, kCuppaPngFile, kBikePngFile, bike_position, kPuzzleJpgFile);
    after = StringPrintf(
        html2, sprite, sprite, expected_position, kPuzzleJpgFile);

    ValidateExpected("sprites_images", before, should_sprite ? after : before);
  }
};

TEST_F(CssImageCombineTest, SpritesImages) {
  // For each of these, expect the following:
  // If spriting is possible, the first image (Cuppa.png)
  // ends up on top and the second image (BikeCrashIcn.png) ends up on the
  // bottom.
  // Cuppa.png 65px wide by 70px high.
  // BikeCrashIcn.png is 100px wide by 100px high.
  // Therefore if you want to see just BikeCrashIcn.png, you need to
  // align the image 70px above the div (i.e. -70px).
  // All the divs are 10px by 10px (which affects the resulting
  // alignments).
  TestSpriting("0 0", "0 -70px", true);
  TestSpriting("left top", "0 -70px", true);
  TestSpriting("top 10px", "10px -70px", true);
  // TODO(nforman): Have spriting reject this since the 5px will
  // display part of the image above this one.
  TestSpriting("-5px 5px", "-5px -65px", true);
  // We want pixels 45 to 55 out of the image, therefore align the image
  // 45 pixels to the left of the div.
  TestSpriting("center top", "-45px -70px", true);
  // Same as above, but this time select the middle 10 pixels vertically,
  // as well (45 to 55, but offset by 70 for the image above).
  TestSpriting("center center", "-45px -115px", true);
  // We want the bottom, right corner of the image, i.e. pixels
  // 90 to 100 (both vertically and horizontally), so align the image
  // 90 pixels to the left and 160 pixels (70 from Cuppa.png) above.
  TestSpriting("right bottom", "-90px -160px", true);
  // Here we need the vertical center (45 to 55, plus the 70 offset),
  // and the horizontal right (90 to 100).
  TestSpriting("center right", "-90px -115px", true);
  // This is equivalent to "center right".
  TestSpriting("right", "-90px -115px", true);
  // This is equivalent to "top center".
  TestSpriting("top", "-45px -70px", true);
}

// Image spriting tests with debug enabled.
class CssImageCombineUnauthorizedTest : public CssRewriteTestBase {
 protected:
  virtual void SetUp() {
    options()->EnableFilter(RewriteOptions::kDebug);
    options()->EnableFilter(RewriteOptions::kSpriteImages);
    CssRewriteTestBase::SetUp();
    AddFileToMockFetcher(StrCat("http://unauth.com/", kBikePngFile),
                         kBikePngFile, kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kCuppaPngFile), kCuppaPngFile,
                         kContentTypePng, 100);
    AddFileToMockFetcher(StrCat(kTestDomain, kPuzzleJpgFile), kPuzzleJpgFile,
                         kContentTypeJpeg, 100);
  }
};

TEST_F(CssImageCombineTest, UnauthorizedDomain) {
  const GoogleString bike_path = StrCat("http://unauth.com/", kBikePngFile);
  AddFileToMockFetcher(bike_path, kBikePngFile, kContentTypePng, 100);
  options()->ClearSignatureForTesting();
  options()->EnableFilter(RewriteOptions::kDebug);
  options()->ComputeSignature();

  const GoogleString kDebugMessage = StrCat(
      "<!--Flattening failed: Cannot rewrite ", bike_path,
      " as it is on an unauthorized domain-->");
  const char kDebugStatistics[] = "";
  const char* html = "<head><style>"
      "#div2{background:transparent url(%s);"
      "background-position:0 0;width:10px;height:10px}"
      "</style>%s</head>%s";
  GoogleString before = StringPrintf(html, bike_path.c_str(), "", "");
  GoogleString after = StringPrintf(html, bike_path.c_str(),
                                    kDebugMessage.c_str(), kDebugStatistics);

  ValidateExpected("unauthorized_domain", before, after);
}

TEST_F(CssImageCombineTest, DontLeak) {
  // Regression test for a leak: we had trouble when a single position was
  // merely "0%".
  const char kHtml[] = "<style>"
      "#div2{background:transparent url(Cuppa.png) no-repeat scroll 0%;"
      "background-position:0 0;width:10px;height:10px}"
      "</style>";

  ValidateNoChanges("single_pos", kHtml);
}

class CssImageCombineTestCustomOptions : public CssImageCombineTest {
 protected:
  // Derived classes should set their options and then call
  // CssImageCombineTest::SetUp().
  virtual void SetUp() {}
};

TEST_F(CssImageCombineTest, SpritesMultiple) {
  GoogleString before, after, sprite;
  // With the same image present 3 times, there should be no sprite.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "10px", kBikePngFile, "0");
  ValidateNoChanges("no_sprite_3_bikes", before);

  // With 2 of the same and 1 different, there should be a sprite without
  // duplication.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "10px", kCuppaPngFile, "0");
  sprite = Encode("", "is", "0",
                  MultiUrl(kBikePngFile, kCuppaPngFile), "png").c_str();
  after = StringPrintf(kHtmlTemplate3Divs, sprite.c_str(),
                       sprite.c_str(), "0", "10px", sprite.c_str(), "-100px");
  ValidateExpected("sprite_2_bikes_1_cuppa", before, after);

  // If the second occurrence of the image is unspriteable (e.g. if the div is
  // larger than the image), then don't sprite anything.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "999px", kCuppaPngFile, "0");
  ValidateNoChanges("sprite_none_dimensions", before);
}

// Try the last test from SpritesMultiple with a cold cache.
TEST_F(CssImageCombineTest, NoSpritesMultiple) {
  // If the second occurrence of the image is unspriteable (e.g. if the div is
  // larger than the image), then don't sprite anything.
  GoogleString in_text =
      StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                   kBikePngFile, "0", "999px", kCuppaPngFile, "0");
  ValidateNoChanges("no_sprite", in_text);
}

TEST_F(CssImageCombineTest, NoCrashUnknownType) {
  // Make sure we don't crash trying to sprite an image with an unknown mimetype
  ResponseHeaders response_headers;
  SetDefaultLongCacheHeaders(&kContentTypePng, &response_headers);
  response_headers.Replace(HttpAttributes::kContentType, "image/x-bewq");
  response_headers.ComputeCaching();
  SetFetchResponse(StrCat(kTestDomain, "bar.bewq"),
                   response_headers, "unused payload");
  SetResponseWithDefaultHeaders("foo.png", kContentTypePng,
                                "unused payload", 100);

  const GoogleString before =
      "<head><style>"
      "#div1 { background-image:url('bar.bewq');"
      "width:10px;height:10px}"
      "#div2 { background:transparent url('foo.png');width:10px;height:10px}"
      "</style></head>";

  ParseUrl(kTestDomain, before);
}

TEST_F(CssImageCombineTest, SpritesImagesExternal) {
  SetupWaitFetcher();

  const GoogleString beforeCss = StrCat(" "  // extra whitespace allows rewrite
      "#div1{background-image:url(", kCuppaPngFile, ");"
      "width:10px;height:10px}"
      "#div2{background:transparent url(", kBikePngFile,
                                        ");width:10px;height:10px}");
  GoogleString cssUrl(kTestDomain);
  cssUrl += "style.css";
  // At first try, not even the CSS gets loaded, so nothing gets
  // changed at all.
  ValidateRewriteExternalCss("wip", beforeCss, beforeCss,
                             kExpectNoChange | kNoClearFetcher);

  // Allow the images to load
  CallFetcherCallbacks();

  // On the second run, we get spriting.
  const GoogleString sprite =
      Encode("", "is", "0", MultiUrl(kCuppaPngFile, kBikePngFile), "png");
  const GoogleString spriteCss = StrCat(
      "#div1{background-image:url(", sprite, ");"
      "width:10px;height:10px;"
      "background-position:0 0}"
      "#div2{background:transparent url(", sprite,
      ");width:10px;height:10px;background-position:0 -70px}");
  // kNoStatCheck because ImageCombineFilter uses different stats.
  ValidateRewriteExternalCss("wip", beforeCss, spriteCss,
                             kExpectSuccess | kNoClearFetcher | kNoStatCheck);
}

TEST_F(CssImageCombineTest, SpritesOkAfter404) {
  // Make sure the handling of a 404 is correct, and doesn't interrupt spriting
  // (nor check fail, as it used to before).
  AddFileToMockFetcher(StrCat(kTestDomain, "bike2.png"), kBikePngFile,
                       kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kTestDomain, "bike3.png"), kBikePngFile,
                       kContentTypePng, 100);
  SetFetchResponse404("404.png");

  const char kHtmlTemplate[] = "<head><style>"
      "#div1{background:url(%s);width:10px;height:10px}"
      "#div2{background:url(%s);width:10px;height:10px}"
      "#div3{background:url(%s);width:10px;height:10px}"
      "#div4{background:url(%s);width:10px;height:10px}"
      "#div5{background:url(%s);width:10px;height:10px}"
      "</style></head>";

  GoogleString html = StringPrintf(kHtmlTemplate,
                                   kBikePngFile,
                                   kCuppaPngFile,
                                   "404.png",
                                   "bike2.png",
                                   "bike3.png");
  Parse("sprite_with_404", html);  // Parse
  EXPECT_NE(GoogleString::npos,
            output_buffer_.find(
                Encode("", "is", "0",
                       MultiUrl(kBikePngFile, kCuppaPngFile,
                                "bike2.png", "bike3.png"),
                       "png")));
}

TEST_F(CssImageCombineTest, SpritesMultiSite) {
  // Make sure we do something sensible when we're forced to split into multiple
  // partitions due to different host names -- at lest when it doesn't require
  // us to keep track of multiple partitions intelligently.
  const char kAltDomain[] = "http://images.example.com/";
  AddDomain(kAltDomain);

  AddFileToMockFetcher(StrCat(kAltDomain, kBikePngFile), kBikePngFile,
                        kContentTypePng, 100);
  AddFileToMockFetcher(StrCat(kAltDomain, kCuppaPngFile), kCuppaPngFile,
                        kContentTypePng, 100);

  const char kHtmlTemplate[] = "<head><style>"
      "#div1{background:url(%s);width:10px;height:10px%s}"
      "#div2{background:url(%s);width:10px;height:10px%s}"
      "#div3{background:url(%s);width:10px;height:10px%s}"
      "#div4{background:url(%s);width:10px;height:10px%s}"
      "</style></head>";

  GoogleString test_bike = StrCat(kTestDomain, kBikePngFile);
  GoogleString alt_bike = StrCat(kAltDomain, kBikePngFile);
  GoogleString test_cup = StrCat(kTestDomain, kCuppaPngFile);
  GoogleString alt_cup = StrCat(kAltDomain, kCuppaPngFile);
  GoogleString test_sprite = Encode(
      kTestDomain, "is", "0", MultiUrl(kBikePngFile, kCuppaPngFile), "png");
  GoogleString alt_sprite = Encode(
      kAltDomain, "is", "0", MultiUrl(kBikePngFile, kCuppaPngFile), "png");

  GoogleString before = StringPrintf(kHtmlTemplate,
                                     test_bike.c_str(), "",
                                     alt_bike.c_str(), "",
                                     test_cup.c_str(), "",
                                     alt_cup.c_str(), "");

  GoogleString after = StringPrintf(
      kHtmlTemplate,
      test_sprite.c_str(), ";background-position:0 0",
      alt_sprite.c_str(), ";background-position:0 0",
      test_sprite.c_str(), ";background-position:0 -100px",
      alt_sprite.c_str(), ";background-position:0 -100px");
  ValidateExpected("multi_site", before, after);

  // For this test, a partition should get created for the alt_bike image,
  // but it should end up getting canceled and deleted since the partition
  // will have only one image in it.
  before = StringPrintf(kHtmlTemplate,
                        alt_bike.c_str(), "",
                        test_bike.c_str(), "",
                        test_cup.c_str(), "",
                        test_bike.c_str(), "");
  after = StringPrintf(kHtmlTemplate,
                       alt_bike.c_str(), "",
                       test_sprite.c_str(), ";background-position:0 0",
                       test_sprite.c_str(), ";background-position:0 -100px",
                       test_sprite.c_str(), ";background-position:0 0");
  ValidateExpected("multi_site_one_sprite", before, after);
}

// TODO(nforman): Add a testcase that synthesizes a spriting situation where
// the total size of the constructed segment (not including the domain or
// .pagespeed.* parts) is larger than RewriteOptions::kDefaultMaxUrlSegmentSize
// (1024).
TEST_F(CssImageCombineTest, ServeFiles) {
  GoogleString sprite_str =
      Encode(kTestDomain, "is", "0",
             MultiUrl(kCuppaPngFile, kBikePngFile), "png");
  GoogleString output;
  EXPECT_TRUE(FetchResourceUrl(sprite_str, &output));
  ServeResourceFromManyContexts(sprite_str, output);
}

// FYI: Takes ~10000 ms to run under Valgrind.
TEST_F(CssImageCombineTest, CombineManyFiles) {
  // Prepare an HTML fragment with too many image files to combine,
  // exceeding the char limit.
  const int kNumImages = 100;
  const int kImagesInCombination = 47;
  GoogleString html = "<head><style>";
  for (int i = 0; i < kNumImages; ++i) {
    GoogleString url = StringPrintf("%s%.02d%s", kTestDomain, i, kBikePngFile);
    AddFileToMockFetcher(url, kBikePngFile, kContentTypePng, 100);
    html.append(StringPrintf(
        "#div%d{background:url(%s) 0 0;width:10px;height:10px}",
        i, url.c_str()));
  }
  html.append("</style></head>");

  // We expect 3 combinations: 0-46, 47-93, 94-99
  StringVector combinations;
  int image_index = 0;
  while (image_index < kNumImages) {
    StringVector combo;
    int end_index = std::min(image_index + kImagesInCombination, kNumImages);
    while (image_index < end_index) {
      combo.push_back(StringPrintf("%.02d%s", image_index, kBikePngFile));
      ++image_index;
    }
    // Original URL is absolute, so rewritten one is as well.
    combinations.push_back(Encode(kTestDomain, "is", "0", combo, "png"));
  }

  image_index = 0;
  int combo_index = 0;
  GoogleString result = "<head><style>";
  while (image_index < kNumImages) {
    int offset = (image_index - (combo_index * kImagesInCombination)) * -100;
    GoogleString offset_str;
    if (offset == 0) {
      // Minification artifact.
      offset_str = "0";
    } else {
      offset_str = StringPrintf("%dpx", offset);
    }

    result.append(StringPrintf(
        "#div%d{background:url(%s) 0 %s;width:10px;height:10px}",
        image_index, combinations[combo_index].c_str(), offset_str.c_str()));
    ++image_index;
    if (image_index % kImagesInCombination == 0) {
      ++combo_index;
    }
  }
  result.append("</style></head>");

  ValidateExpected("manymanyimages", html, result);
}

TEST_F(CssImageCombineTest, SpritesBrokenUp) {
  // Make sure we include all spritable images, even if there are
  // un-spritable images in between.
  GoogleString before, after;

  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kPuzzleJpgFile, "0", "10px", kCuppaPngFile, "0");

  const GoogleString sprite_string =
      Encode("", "is", "0", MultiUrl(kBikePngFile, kCuppaPngFile), "png");
  const char* sprite = sprite_string.c_str();

  after = StringPrintf(kHtmlTemplate3Divs, sprite,
                       kPuzzleJpgFile, "0", "10px", sprite, "-100px");
  ValidateExpected("sprite_broken_up", before, after);
}

TEST_F(CssImageCombineTest, SpritesGifsWithPngs) {
  // Make sure we include all spritable images, even if there are
  // un-spritable images in between.
  GoogleString before, after;

  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kChefGifFile, "0", "10px", kCuppaPngFile, "0");

  const GoogleString sprite_string =
      Encode("", "is", "0", MultiUrl(kBikePngFile, kChefGifFile, kCuppaPngFile),
             "png");
  const char* sprite = sprite_string.c_str();

  // The BikePng is 100px tall, the ChefGif is 256px tall, so we
  // expect the Chef to be offset by -100, and the CuppaPng to be
  // offset by -356.
  after = StringPrintf(kHtmlTemplate3Divs, sprite,
                       sprite, "-100px", "10px", sprite, "-356px");
  ValidateExpected("sprite_with_gif", before, after);
}

TEST_F(CssImageCombineTest, SpriteWrongMime) {
  // Make sure that a server messing up the content-type doesn't prevent
  // spriting.
  GoogleString wrong_bike = StrCat(kTestDomain, "w", kBikePngFile);
  GoogleString wrong_cuppa = StrCat(kTestDomain, "w", kCuppaPngFile);

  AddFileToMockFetcher(wrong_bike, kBikePngFile, kContentTypeJpeg, 100);
  AddFileToMockFetcher(wrong_cuppa, kCuppaPngFile,
                       kContentTypeJpeg, 100);

  GoogleString rel_sprite =
      Encode("", "is", "0",
             MultiUrl(StrCat("w", kBikePngFile),
                      StrCat("w", kCuppaPngFile),
                      kCuppaPngFile),
             "png");
  GoogleString abs_sprite = StrCat(kTestDomain, rel_sprite);

  GoogleString before, after;
  before = StringPrintf(kHtmlTemplate3Divs, wrong_bike.c_str(),
                        wrong_cuppa.c_str(), "0", "10px", kCuppaPngFile, "0");

  // The BikePng is 100px tall, the cuppa is 70px tall, so we
  // expect the cuppa to be offset by -100, and the right-path cuppa to be
  // offset by -170.
  //
  // First 2 original URLs were absolute, so rewritten ones are as well.
  // Last was relative, so it is preserved as relative.
  after = StringPrintf(kHtmlTemplate3Divs,
                       abs_sprite.c_str(), abs_sprite.c_str(), "-100px", "10px",
                       rel_sprite.c_str(), "-170px");
  ValidateExpected("wrong_mime", before, after);
}

class CssImageMultiFilterTest : public CssImageCombineTest {
  // Users must call CssImageCombineTest::SetUp().
  virtual void SetUp() {}
};

TEST_F(CssImageMultiFilterTest, SpritesAndNonSprites) {
  // We setup the options before the upcall so that the
  // CSS filter is created aware of these.
  options()->EnableFilter(RewriteOptions::kExtendCacheImages);
  CssImageCombineTest::SetUp();

  GoogleString before, after, encoded, cuppa_encoded, sprite;
  // With the same image present 3 times, there should be no sprite.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "10px", kBikePngFile, "0");
  encoded = Encode("", "ce", "0", kBikePngFile, "png");
  after = StringPrintf(kHtmlTemplate3Divs, encoded.c_str(),
                       encoded.c_str(), "0", "10px", encoded.c_str(), "0");
  ValidateExpected("no_sprite_3_bikes", before, after);

  // With 2 of the same and 1 different, there should be a sprite without
  // duplication.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "10px", kCuppaPngFile, "0");
  sprite = Encode("", "is", "0",
                  MultiUrl(kBikePngFile, kCuppaPngFile), "png").c_str();
  after = StringPrintf(kHtmlTemplate3Divs, sprite.c_str(),
                       sprite.c_str(), "0", "10px", sprite.c_str(), "-100px");
  ValidateExpected("sprite_2_bikes_1_cuppa", before, after);

  // If the second occurrence of the image is unspriteable (e.g. if the div is
  // larger than the image), we shouldn't sprite any of them.
  before = StringPrintf(kHtmlTemplate3Divs, kBikePngFile,
                        kBikePngFile, "0", "999px", kCuppaPngFile, "0");
  cuppa_encoded = Encode("", "ce", "0", kCuppaPngFile, "png");
  after = StringPrintf(kHtmlTemplate3Divs, encoded.c_str(), encoded.c_str(),
                       "0", "999px", cuppa_encoded.c_str(), "0");
  ValidateExpected("sprite_none_dimmensions", before, after);
}

// A test in which base URL inside CSS is different than inside HTML.
// Specifically CSS base URL is inside subdir/.
// This might also be the only test for external stylesheets.
TEST_F(CssImageCombineTest, CssDifferentBase) {
  // Set up resources.
  AddFileToMockFetcher("subdir/Cuppa.png", kCuppaPngFile, kContentTypePng, 100);
  AddFileToMockFetcher("subdir/BikeCrashIcn.png", kBikePngFile,
                       kContentTypePng, 100);
  GoogleString css_before =
      ".a {background: 0 0 url(Cuppa.png) no-repeat;"
      " width:10px; height:10px}"
      ".b {background: 0 0 url(BikeCrashIcn.png) no-repeat;"
      " width:10px; height:10px}";
  SetResponseWithDefaultHeaders("subdir/foo.css", kContentTypeCss,
                                css_before, 100);

  GoogleString expected_css_after =
      ".a{background:0 0"
      " url(Cuppa.png+BikeCrashIcn.png.pagespeed.is.0.png)"
      " no-repeat;width:10px;height:10px}"
      ".b{background:0 -70px"
      " url(Cuppa.png+BikeCrashIcn.png.pagespeed.is.0.png)"
      " no-repeat;width:10px;height:10px}";

  GoogleString rewritten_url = Encode("subdir/", "cf", "0", "foo.css", "css");
  ValidateExpected("diff_base",
                   CssLinkHref("subdir/foo.css"),
                   CssLinkHref(rewritten_url));

  GoogleString actual_css_after;
  ASSERT_TRUE(FetchResourceUrl(StrCat(kTestDomain, rewritten_url),
                               &actual_css_after));
  EXPECT_EQ(expected_css_after, actual_css_after);
}

TEST_F(CssImageMultiFilterTest, WithFlattening) {
  // We setup the options before the upcall so that the
  // CSS filter is created aware of these.
  options()->EnableFilter(RewriteOptions::kFlattenCssImports);
  CssImageCombineTest::SetUp();

  AddFileToMockFetcher("dir/Cuppa.png", kCuppaPngFile, kContentTypePng, 100);
  AddFileToMockFetcher("dir/BikeCrashIcn.png", kBikePngFile,
                       kContentTypePng, 100);

  const char kLeafCss[] =
      ".a {background: 0 0 url(Cuppa.png) no-repeat;"
      " width:10px; height:10px}"
      ".b {background: 0 0 url(BikeCrashIcn.png) no-repeat;"
      " width:10px; height:10px}";
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss, kLeafCss, 100);

  const char kBeforeHtml[] = "<style>@import url(dir/a.css);</style>";
  // Note: This is flattened and combined.
  // TODO(sligocki): Perhaps http://test.com/dir/Cuppa.png should be relative
  // given that the original URL in the original stylesheet was relative.
  const char kAfterHtml[] =
      "<style>"
      ".a{background:0 0"
      " url(http://test.com/dir/Cuppa.png+BikeCrashIcn.png.pagespeed.is.0.png)"
      " no-repeat;width:10px;height:10px}"
      ".b{background:0 -70px"
      " url(http://test.com/dir/Cuppa.png+BikeCrashIcn.png.pagespeed.is.0.png)"
      " no-repeat;width:10px;height:10px}"
      "</style>";

  ValidateExpected("with_flattening", kBeforeHtml, kAfterHtml);
}

TEST_F(CssImageMultiFilterTest, NoCombineAcrossFlattening) {
  // We setup the options before the upcall so that the
  // CSS filter is created aware of these.
  options()->EnableFilter(RewriteOptions::kFlattenCssImports);
  CssImageCombineTest::SetUp();

  AddFileToMockFetcher("dir/Cuppa.png", kCuppaPngFile, kContentTypePng, 100);

  const char kLeafCss[] =
      ".a {background: 0 0 url(Cuppa.png) no-repeat;"
      " width:10px; height:10px}";
  SetResponseWithDefaultHeaders("dir/a.css", kContentTypeCss, kLeafCss, 100);

  const char kBeforeHtml[] =
      "<style>\n"
      "@import url(dir/a.css);\n"
      ".b {background: 0 0 url(BikeCrashIcn.png) no-repeat;"
      " width:10px; height:10px}\n"
      "</style>";
  // TODO(sligocki): Any reason not to combine images across flattening
  // boundaries? Currently we don't seem to.
  // TODO(sligocki): Perhaps http://test.com/dir/Cuppa.png should be relative
  // given that the original URL in the original stylesheet was relative.
  const char kAfterHtml[] =
      "<style>"
      ".a{background:0 0 url(http://test.com/dir/Cuppa.png) no-repeat;"
      "width:10px;height:10px}"
      ".b{background:0 0 url(BikeCrashIcn.png) no-repeat;"
      "width:10px;height:10px}"
      "</style>";

  ValidateExpected("with_flattening", kBeforeHtml, kAfterHtml);
}

TEST_F(CssImageCombineTest, ContentTypeValidation) {
  ValidateFallbackHeaderSanitization("is");
}

}  // namespace

}  // namespace net_instaweb
