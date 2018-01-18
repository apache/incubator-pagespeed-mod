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


#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/test_utils.h"

namespace {

static const char kInvalidImageFormat[] = "Invalid image format";
static const char kInvalidPixelFormat[] = "Invalid pixel format";

const char kGifImage[] = "transparent.gif";
const char kPngImage[] = "this_is_a_test.png";
const char kJpegImage[] = "sjpeg1.jpg";
const char kWebpOpaqueImage[] = "opaque_32x20.webp";
const char kWebpLosslessImage[] = "img3.webpla";
const char kWebpAnimatedImage[] = "animated.webp";
// icc_xmp_ex.webp contains only one lossily compressed image, but has "VP8X"
// chunk because of XMP and ICC.
const char kWebpIccXmpImage[] = "icc_xmp_ex.webp";

// Enums
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::PixelFormat;

// Image formats.
using pagespeed::image_compression::IMAGE_UNKNOWN;
using pagespeed::image_compression::IMAGE_JPEG;
using pagespeed::image_compression::IMAGE_PNG;
using pagespeed::image_compression::IMAGE_GIF;
using pagespeed::image_compression::IMAGE_WEBP;

// Pixel formats.
using pagespeed::image_compression::UNSUPPORTED;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
using pagespeed::image_compression::GRAY_8;

// WebP formats.
using pagespeed::image_compression::WEBP_NONE;
using pagespeed::image_compression::WEBP_LOSSY;
using pagespeed::image_compression::WEBP_LOSSLESS;
using pagespeed::image_compression::WEBP_ANIMATED;

// Folders for testing images.
using pagespeed::image_compression::kGifTestDir;
using pagespeed::image_compression::kJpegTestDir;
using pagespeed::image_compression::kPngTestDir;
using pagespeed::image_compression::kWebpTestDir;

using pagespeed::image_compression::ComputeImageType;
using pagespeed::image_compression::PreferredLibwebpLevel;
using pagespeed::image_compression::ReadTestFileWithExt;

TEST(ImageUtilTest, ImageFormatToMimeTypeString) {
  EXPECT_STREQ("image/unknown", ImageFormatToMimeTypeString(IMAGE_UNKNOWN));
  EXPECT_STREQ("image/jpeg", ImageFormatToMimeTypeString(IMAGE_JPEG));
  EXPECT_STREQ("image/png", ImageFormatToMimeTypeString(IMAGE_PNG));
  EXPECT_STREQ("image/gif", ImageFormatToMimeTypeString(IMAGE_GIF));
  EXPECT_STREQ("image/webp", ImageFormatToMimeTypeString(IMAGE_WEBP));
  EXPECT_STREQ("image/webp", ImageFormatToMimeTypeString(IMAGE_WEBP));
  EXPECT_STREQ(kInvalidImageFormat,
               ImageFormatToMimeTypeString(static_cast<ImageFormat>(5)));
}

TEST(ImageUtilTest, ImageFormatToString) {
  EXPECT_STREQ("IMAGE_UNKNOWN", ImageFormatToString(IMAGE_UNKNOWN));
  EXPECT_STREQ("IMAGE_JPEG", ImageFormatToString(IMAGE_JPEG));
  EXPECT_STREQ("IMAGE_PNG", ImageFormatToString(IMAGE_PNG));
  EXPECT_STREQ("IMAGE_GIF", ImageFormatToString(IMAGE_GIF));
  EXPECT_STREQ("IMAGE_WEBP", ImageFormatToString(IMAGE_WEBP));
  EXPECT_STREQ(kInvalidImageFormat,
               ImageFormatToMimeTypeString(static_cast<ImageFormat>(5)));
}

TEST(ImageUtilTest, GetPixelFormatString) {
  EXPECT_STREQ("UNSUPPORTED", GetPixelFormatString(UNSUPPORTED));
  EXPECT_STREQ("RGB_888", GetPixelFormatString(RGB_888));
  EXPECT_STREQ("RGBA_8888", GetPixelFormatString(RGBA_8888));
  EXPECT_STREQ("GRAY_8", GetPixelFormatString(GRAY_8));
}

TEST(ImageUtilTest, GetBytesPerPixel) {
  EXPECT_EQ(0, GetBytesPerPixel(UNSUPPORTED));
  EXPECT_EQ(3, GetBytesPerPixel(RGB_888));
  EXPECT_EQ(4, GetBytesPerPixel(RGBA_8888));
  EXPECT_EQ(1, GetBytesPerPixel(GRAY_8));
}

TEST(ImageUtilTest, ImageFormat) {
  GoogleString buffer;

  ASSERT_TRUE(ReadTestFileWithExt(kGifTestDir, kGifImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_GIF, ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kPngTestDir, kPngImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_PNG, ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kJpegTestDir, kJpegImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_JPEG, ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kWebpTestDir, kWebpOpaqueImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_WEBP, ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kWebpTestDir, kWebpLosslessImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_WEBP_LOSSLESS_OR_ALPHA,
            ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kWebpTestDir, kWebpAnimatedImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_WEBP_ANIMATED,
            ComputeImageType(buffer));

  ASSERT_TRUE(ReadTestFileWithExt(kWebpTestDir, kWebpIccXmpImage, &buffer));
  EXPECT_EQ(net_instaweb::IMAGE_WEBP, ComputeImageType(buffer));
}

}  // namespace
