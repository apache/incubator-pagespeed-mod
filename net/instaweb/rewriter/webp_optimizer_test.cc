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

// Unit tests for the routines in webp_optimizer.cc.

#include "net/instaweb/rewriter/public/webp_optimizer.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"  // for StrCat
#include "pagespeed/kernel/image/test_utils.h"

#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#include "webp/encode.h"
#else
#include "third_party/libwebp/src/webp/decode.h"
#endif

namespace net_instaweb {

using pagespeed::image_compression::ReadFile;

const char kTestData[] = "/net/instaweb/rewriter/testdata/";
const char kTransparentWebP[] = "chromium-24.webp";

TEST(WebpOptimizerTest, ReduceWebpImageQualityPreservesAlpha) {
  // This test verifies that ReduceWebpImageQuality preserves the
  // alpha channel.

  GoogleString input_image, output_image;
  EXPECT_TRUE(ReadFile(StrCat(GTestSrcDir(), kTestData, kTransparentWebP),
                       &input_image));

  WebPBitstreamFeatures features;
  EXPECT_EQ(VP8_STATUS_OK,
            WebPGetFeatures(
                reinterpret_cast<const uint8_t*>(input_image.c_str()),
                input_image.length(), &features));
  EXPECT_TRUE(features.has_alpha);

  EXPECT_TRUE(ReduceWebpImageQuality(input_image, 50, &output_image));
  EXPECT_NE(input_image, output_image);

  EXPECT_EQ(VP8_STATUS_OK,
            WebPGetFeatures(
                reinterpret_cast<const uint8_t*>(output_image.c_str()),
                output_image.length(), &features));
  EXPECT_TRUE(features.has_alpha);
}
}  // namespace net_instaweb
