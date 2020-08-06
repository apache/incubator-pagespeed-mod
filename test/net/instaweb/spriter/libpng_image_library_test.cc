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

#include "net/instaweb/spriter/libpng_image_library.h"

#include <memory>

#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "test/pagespeed/kernel/base/gtest.h"

namespace {

const char kTestData[] = "/test/net/instaweb/rewriter/testdata/";
const char kCuppa[] = "Cuppa.png";
const char kBikeCrash[] = "BikeCrashIcn.png";

}  // namespace

namespace net_instaweb {
namespace spriter {

class LibpngImageLibraryTest : public testing::Test {
 protected:
  class LogDelegate : public ImageLibraryInterface::Delegate {
    void OnError(const GoogleString& error) const override {
      ASSERT_TRUE(false) << "Unexpected error: " << error;
    }
  };
  void SetUp() override {
    delegate_ = std::make_unique<LogDelegate>();
    mkdir(GTestTempDir().c_str(), 0777);
    src_library_ = std::make_unique<LibpngImageLibrary>(
        StrCat(GTestSrcDir(), kTestData), StrCat(GTestTempDir(), "/"),
        delegate_.get());
    tmp_library_ = std::make_unique<LibpngImageLibrary>(
        StrCat(GTestTempDir(), "/"), StrCat(GTestTempDir(), "/"),
        delegate_.get());
  }
  ImageLibraryInterface::Image* ReadFromFile(const StringPiece& filename) {
    return src_library_->ReadFromFile(filename.as_string());
  }
  ImageLibraryInterface::Canvas* CreateCanvas(int width, int height) {
    return src_library_->CreateCanvas(width, height);
  }
  ImageLibraryInterface::Image* WriteAndRead(ImageLibraryInterface::Canvas* c) {
    bool success = c->WriteToFile("out.png", PNG);
    return success ? tmp_library_->ReadFromFile("out.png") : nullptr;
  }

  // A library that reads in our test source data and writes in our temp dir.
  std::unique_ptr<LibpngImageLibrary> src_library_;
  // A library that reads and writes in our temp dir.
  std::unique_ptr<LibpngImageLibrary> tmp_library_;
  std::unique_ptr<LogDelegate> delegate_;
};

TEST_F(LibpngImageLibraryTest, TestCompose) {
  //  65x70
  std::unique_ptr<ImageLibraryInterface::Image> image1(ReadFromFile(kCuppa));
  // 100x100
  std::unique_ptr<ImageLibraryInterface::Image> image2(
      ReadFromFile(kBikeCrash));
  ASSERT_TRUE(image1.get() != nullptr);
  ASSERT_TRUE(image2.get() != nullptr);
  std::unique_ptr<ImageLibraryInterface::Canvas> canvas(CreateCanvas(100, 170));
  ASSERT_TRUE(canvas != nullptr);
  ASSERT_TRUE(canvas->DrawImage(image1.get(), 0, 0));
  ASSERT_TRUE(canvas->DrawImage(image2.get(), 0, 70));
  ASSERT_TRUE(canvas->WriteToFile("out.png", PNG));
  std::unique_ptr<ImageLibraryInterface::Image> image3(
      WriteAndRead(canvas.get()));
  ASSERT_TRUE(image3.get() != nullptr);
  int width, height;
  ASSERT_TRUE(image3->GetDimensions(&width, &height));
  EXPECT_EQ(100, width);
  EXPECT_EQ(170, height);
}

}  // namespace spriter
}  // namespace net_instaweb
