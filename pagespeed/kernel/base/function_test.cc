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


#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"

namespace {

const char kCharData = 'x';
const int kIntData = 42;
const double kDoubleData = 5.5;
const bool kBoolData = true;

}  // namespace

namespace net_instaweb {

class FunctionTest : public testing::Test {
 public:
  FunctionTest() {
    Clear();
  }

  void Clear() {
    char_ = '\0';
    int_ = 0;
    double_ = 0.0;
    bool_ = false;
    was_run_ = false;
    was_cancelled_ = false;
  }

  void Run0() {
    was_run_ = true;
  }

  void Run1(char c) {
    char_ = c;
    was_run_ = true;
  }

  void Run2(char c, int i) {
    char_ = c;
    int_ = i;
    was_run_ = true;
  }

  void Run3(char c, int i, double d) {
    char_ = c;
    int_ = i;
    double_ = d;
    was_run_ = true;
  }

  void Run4(char c, int i, double d, bool b) {
    char_ = c;
    int_ = i;
    double_ = d;
    bool_ = b;
    was_run_ = true;
  }

  void Cancel0() {
    was_cancelled_ = true;
  }

  void Cancel1(char c) {
    was_cancelled_ = true;
  }

  void Cancel2(char c, int i) {
    was_cancelled_ = true;
  }

  void Cancel3(char c, int i, double d) {
    was_cancelled_ = true;
  }

  void Cancel4(char c, int i, double d, bool b) {
    was_cancelled_ = true;
  }

  bool Matches(char c, int i, double d, bool b) const {
    return ((c == char_) && (i == int_) && (d == double_) && (b == bool_));
  }

 protected:
  char char_;
  int int_;
  double double_;
  bool bool_;
  bool was_run_;
  bool was_cancelled_;
};

TEST_F(FunctionTest, Run0NoCancel) {
  FunctionTest* function_test = this;
  Function* f = MakeFunction(function_test, &FunctionTest::Run0);
  f->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run0NoCancelNoAutoDelete) {
  FunctionTest* function_test = this;
  Function* f = MakeFunction(function_test, &FunctionTest::Run0);
  f->set_delete_after_callback(false);
  f->CallRun();
  delete f;
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run0WithCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run0,
               &FunctionTest::Cancel0)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));

  Clear();
  MakeFunction(function_test, &FunctionTest::Run0,
               &FunctionTest::Cancel0)->CallCancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run1NoCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run1,
               kCharData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, 0, 0.0, false));
}

TEST_F(FunctionTest, Run1WithCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run1, &FunctionTest::Cancel1,
               kCharData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, 0, 0.0, false));

  Clear();
  MakeFunction(function_test, &FunctionTest::Run1, &FunctionTest::Cancel1,
               kCharData)->CallCancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run2NoCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run2,
               kCharData, kIntData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, 0.0, false));
}

TEST_F(FunctionTest, Run2WithCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run2, &FunctionTest::Cancel2,
               kCharData, kIntData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, 0.0, false));

  Clear();
  MakeFunction(function_test, &FunctionTest::Run2, &FunctionTest::Cancel2,
               kCharData, kIntData)->CallCancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run3NoCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run3,
               kCharData, kIntData, kDoubleData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, kDoubleData, false));
}

TEST_F(FunctionTest, Run3WithCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run3, &FunctionTest::Cancel3,
               kCharData, kIntData, kDoubleData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, kDoubleData, false));

  Clear();
  MakeFunction(function_test, &FunctionTest::Run3, &FunctionTest::Cancel3,
               kCharData, kIntData, kDoubleData)->CallCancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

TEST_F(FunctionTest, Run4NoCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run4,
               kCharData, kIntData, kDoubleData, kBoolData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, kDoubleData, kBoolData));
}

TEST_F(FunctionTest, Run4WithCancel) {
  FunctionTest* function_test = this;
  MakeFunction(function_test, &FunctionTest::Run4, &FunctionTest::Cancel4,
               kCharData, kIntData, kDoubleData, kBoolData)->CallRun();
  EXPECT_TRUE(was_run_);
  EXPECT_FALSE(was_cancelled_);
  EXPECT_TRUE(Matches(kCharData, kIntData, kDoubleData, kBoolData));

  Clear();
  MakeFunction(function_test, &FunctionTest::Run4, &FunctionTest::Cancel4,
               kCharData, kIntData, kDoubleData, kBoolData)->CallCancel();
  EXPECT_FALSE(was_run_);
  EXPECT_TRUE(was_cancelled_);
  EXPECT_TRUE(Matches('\0', 0, 0.0, false));
}

}  // namespace net_instaweb
