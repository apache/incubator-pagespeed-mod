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

// Some common routines and constants for tests dealing with Images

#include "net/instaweb/rewriter/public/image_test_base.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/image/image_util.h"

namespace net_instaweb {

const char ImageTestBase::kTestData[] = "/net/instaweb/rewriter/testdata/";
const char ImageTestBase::kAppSegments[] = "AppSegments.jpg";
const char ImageTestBase::kBikeCrash[] = "BikeCrashIcn.png";
const char ImageTestBase::kCradle[] = "CradleAnimation.gif";
const char ImageTestBase::kCuppa[] = "Cuppa.png";
const char ImageTestBase::kCuppaTransparent[] = "CuppaT.png";
const char ImageTestBase::kIronChef[] = "IronChef2.gif";
const char ImageTestBase::kPuzzle[] = "Puzzle.jpg";
const char ImageTestBase::kScenery[] = "Scenery.webp";
const char ImageTestBase::kTransparent[] = "transparent.gif";

// From: http://libpng.org/pub/png/png-RedbrushAlpha.html
const char ImageTestBase::kRedbrush[] = "RedbrushAlpha-0.5.png";

ImageTestBase::~ImageTestBase() {
}

// We use the output_type (ultimate expected output type after image
// processing) to set up rewrite permissions for the resulting Image object.
Image* ImageTestBase::ImageFromString(
    ImageType output_type, const GoogleString& name,
    const GoogleString& contents, bool progressive) {
  net_instaweb::Image::CompressionOptions* image_options =
      new net_instaweb::Image::CompressionOptions();
  if (output_type == IMAGE_WEBP) {
    image_options->preferred_webp = pagespeed::image_compression::WEBP_LOSSY;
  } else if (output_type == IMAGE_WEBP_LOSSLESS_OR_ALPHA) {
    image_options->preferred_webp = pagespeed::image_compression::WEBP_LOSSLESS;
  } else {
    image_options->preferred_webp = pagespeed::image_compression::WEBP_NONE;
  }
  image_options->jpeg_quality = -1;
  image_options->progressive_jpeg = progressive;
  image_options->convert_png_to_jpeg =  output_type == IMAGE_JPEG;
  image_options->recompress_png = true;

  return NewImage(contents, name, GTestTempDir(), image_options,
                  &timer_, &message_handler_);
}

Image* ImageTestBase::ReadFromFileWithOptions(
    const char* name, GoogleString* contents,
    Image::CompressionOptions* options) {
  EXPECT_TRUE(file_system_.ReadFile(
      StrCat(GTestSrcDir(), kTestData, name).c_str(),
      contents, &message_handler_));
  return NewImage(*contents, name, GTestTempDir(), options,
                  &timer_, &message_handler_);
}

Image* ImageTestBase::ReadImageFromFile(
    ImageType output_type, const char* filename, GoogleString* buffer,
    bool progressive) {
  EXPECT_TRUE(file_system_.ReadFile(
      StrCat(GTestSrcDir(), kTestData, filename).c_str(),
      buffer, &message_handler_));
  return ImageFromString(output_type, filename, *buffer, progressive);
}

}  // namespace net_instaweb
