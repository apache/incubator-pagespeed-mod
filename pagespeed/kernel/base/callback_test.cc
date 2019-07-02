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


#include "pagespeed/kernel/base/callback.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

namespace {

const int kNumRunsForPermanentCallbacks = 5;

class TestClass {
 public:
  TestClass()
      : x_(0),
        runs_(0) {
  }

  void Method1(int x) {
    x_ = x;
    ++runs_;
  }

  void Method1ConstRefArg(const int& x) {
    x_ = 2 * x;
    ++runs_;
  }

  void Method2(int a, int b) {
    x_ = a + b;
    ++runs_;
  }

  void Method2ConstRefArg(const int& a, int b) {
    x_ = a + b;
    ++runs_;
  }

  void Method3(int a, int b, int c) {
    x_ = a + b + c;
    ++runs_;
  }

  void Method3ConstRefArg(const int& a, int b, int c) {
    x_ = a + b + c;
    ++runs_;
  }

  int x() const { return x_; }
  int runs() const { return runs_; }

 private:
  int x_;
  int runs_;
};

TEST(CallbackTest, MemberCallback_0_1) {
  TestClass test_class;
  Callback1<int>* cb = NewCallback(&test_class, &TestClass::Method1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(100, test_class.x());
}

TEST(CallbackTest, MemberCallback_0_1_ConstRefArg) {
  TestClass test_class;
  Callback1<const int&>* cb = NewCallback(&test_class,
                                          &TestClass::Method1ConstRefArg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100);
  EXPECT_EQ(200, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_1) {
  TestClass test_class;
  Callback1<int>* cb = NewCallback(&test_class, &TestClass::Method2, 1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(2);
  EXPECT_EQ(3, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_1ConstRefArg) {
  TestClass test_class;
  int arg = 1;
  Callback1<int>* cb = NewCallback(&test_class,
                                   &TestClass::Method2ConstRefArg,
                                   arg);
  // Increment x to show that the reference is followed, rather having
  // arg's current value of 1 captured at the time the callback was
  // instantiated.
  ++arg;
  EXPECT_EQ(2, arg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(2);
  // The callback should have the bound value of 1, even though it was passed
  // by reference.
  EXPECT_EQ(3, test_class.x());
}

TEST(CallbackTest, PermanentMemberCallback_0_1) {
  TestClass test_class;
  scoped_ptr<Callback1<int> > cb(
      NewPermanentCallback(&test_class, &TestClass::Method1));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(100);
    EXPECT_EQ(100, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_0_1_ConstRefArg) {
  TestClass test_class;
  scoped_ptr<Callback1<const int&> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method1ConstRefArg));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(100);
    EXPECT_EQ(200, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_1_1) {
  TestClass test_class;
  scoped_ptr<Callback1<int> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method2, 1));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(2);
    EXPECT_EQ(3, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_1_1ConstRefArg) {
  TestClass test_class;
  int arg = 1;
  scoped_ptr<Callback1<int> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method2ConstRefArg, arg));
  // Increment x showing the reference in the callback tracks the increment.
  ++arg;
  EXPECT_EQ(2, arg);
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(2);
    // The callback should have the bound value of 1, even though it was passed
    // by reference.
    EXPECT_EQ(3, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, MemberCallback_0_2) {
  TestClass test_class;
  Callback2<int, int>* cb = NewCallback(&test_class, &TestClass::Method2);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100, 200);
  EXPECT_EQ(300, test_class.x());
}

TEST(CallbackTest, MemberCallback_0_2_ConstRefArg) {
  TestClass test_class;
  Callback2<const int&, int>* cb =
      NewCallback(&test_class, &TestClass::Method2ConstRefArg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(100, 1000);
  EXPECT_EQ(1100, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_2) {
  TestClass test_class;
  Callback2<int, int>* cb = NewCallback(&test_class, &TestClass::Method3, 1);
  EXPECT_EQ(0, test_class.x());
  cb->Run(10, 100);
  EXPECT_EQ(111, test_class.x());
}

TEST(CallbackTest, MemberCallback_1_2ConstRefArg) {
  TestClass test_class;
  int arg = 1;
  Callback2<int, int>* cb = NewCallback(&test_class,
                                        &TestClass::Method3ConstRefArg,
                                        arg);
  // Increment x.
  ++arg;
  EXPECT_EQ(2, arg);
  EXPECT_EQ(0, test_class.x());
  cb->Run(10, 100);
  // The callback should include the bound value of 1 (+ 100 + 10),
  // even though it was passed by reference.
  EXPECT_EQ(111, test_class.x());
}

TEST(CallbackTest, PermanentMemberCallback_0_2) {
  TestClass test_class;
  scoped_ptr<Callback2<int, int> > cb(
      NewPermanentCallback(&test_class, &TestClass::Method2));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(100, 1000);
    EXPECT_EQ(1100, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_0_2_ConstRefArg) {
  TestClass test_class;
  scoped_ptr<Callback2<const int&, int> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method2ConstRefArg));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(100, 1000);
    EXPECT_EQ(1100, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_1_2) {
  TestClass test_class;
  scoped_ptr<Callback2<int, int> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method3, 1));
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(10, 100);
    EXPECT_EQ(111, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

TEST(CallbackTest, PermanentMemberCallback_1_2ConstRefArg) {
  TestClass test_class;
  int arg = 1;
  scoped_ptr<Callback2<int, int> > cb(NewPermanentCallback(
      &test_class, &TestClass::Method3ConstRefArg, arg));
  ++arg;
  EXPECT_EQ(0, test_class.x());
  for (int i = 0; i < kNumRunsForPermanentCallbacks; ++i) {
    cb->Run(10, 100);
    // The callback should have the bound value of 1, even though it was passed
    // by reference.
    EXPECT_EQ(111, test_class.x());
  }
  EXPECT_EQ(kNumRunsForPermanentCallbacks, test_class.runs());
}

}  // namespace

}  // namespace net_instaweb
