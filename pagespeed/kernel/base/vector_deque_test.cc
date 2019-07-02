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


// Unit-test a custom implementation of deque.

#include "pagespeed/kernel/base/vector_deque.h"
#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

class VectorDequeTest : public testing::Test {
};

TEST_F(VectorDequeTest, Queue) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    for (int i = 0; i < 10; ++i) {
      dv.push_back(i);
    }
    for (int i = 0; i < 10; ++i) {
      int val = dv.front();
      dv.pop_front();
      EXPECT_EQ(i, val);
    }
    EXPECT_EQ(0, dv.size());
    EXPECT_TRUE(dv.empty());
    EXPECT_EQ(16, dv.capacity());
  }
}

TEST_F(VectorDequeTest, Stack) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    for (int i = 0; i < 10; ++i) {
      dv.push_back(i);
    }
    for (int i = 9; i >= 0; --i) {
      int val = dv.back();
      dv.pop_back();
      EXPECT_EQ(i, val);
    }
    EXPECT_EQ(16, dv.capacity());
    EXPECT_EQ(0, dv.size());
    EXPECT_TRUE(dv.empty());
  }
}

TEST_F(VectorDequeTest, Chase1Back) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    for (int i = 0; i < 10; ++i) {
      dv.push_back(i);
      int val = dv.front();
      dv.pop_front();
      EXPECT_EQ(i, val);
    }
    EXPECT_EQ(0, dv.size());
    EXPECT_TRUE(dv.empty());
    EXPECT_EQ(VectorDeque<int>::initial_capacity(), dv.capacity());
  }
}

TEST_F(VectorDequeTest, Chase2Back) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    dv.push_back(-1);
    for (int i = 0; i < 10; ++i) {
      dv.push_back(i);
      int val = dv.front();
      dv.pop_front();
      EXPECT_EQ(i - 1, val);
    }
    EXPECT_EQ(9, dv.front());
    dv.pop_front();
    EXPECT_EQ(0, dv.size());
    EXPECT_EQ(VectorDeque<int>::initial_capacity(), dv.capacity());
  }
}

TEST_F(VectorDequeTest, Chase1Front) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    for (int i = 0; i < 10; ++i) {
      dv.push_front(i);
      int val = dv.back();
      dv.pop_back();
      EXPECT_EQ(i, val);
    }
    EXPECT_EQ(0, dv.size());
    EXPECT_TRUE(dv.empty());
    EXPECT_EQ(VectorDeque<int>::initial_capacity(), dv.capacity());
  }
}

TEST_F(VectorDequeTest, Chase2Front) {
  VectorDeque<int> dv;
  for (int j = 0; j < 5; ++j) {
    dv.push_front(-1);
    for (int i = 0; i < 10; ++i) {
      dv.push_front(i);
      int val = dv.back();
      dv.pop_back();
      EXPECT_EQ(i - 1, val);
    }
    EXPECT_EQ(9, dv.back());
    dv.pop_back();
    EXPECT_EQ(0, dv.size());
    EXPECT_TRUE(dv.empty());
    EXPECT_EQ(VectorDeque<int>::initial_capacity(), dv.capacity());
  }
}

}  // namespace
}  // namespace net_instaweb
