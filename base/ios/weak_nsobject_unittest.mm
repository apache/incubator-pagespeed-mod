// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/weak_nsobject.h"
#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(WeakNSObjectTest, WeakNSObject) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  p1.reset();
  EXPECT_FALSE(w1);
}

TEST(WeakNSObjectTest, MultipleWeakNSObject) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2(w1);
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  EXPECT_TRUE(w1.get() == w2.get());
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

TEST(WeakNSObjectTest, WeakNSObjectDies) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  {
    WeakNSObject<NSObject> w1(p1);
    EXPECT_TRUE(w1);
  }
}

TEST(WeakNSObjectTest, WeakNSObjectReset) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  w1.reset();
  EXPECT_FALSE(w1);
  EXPECT_TRUE(p1);
  EXPECT_TRUE([p1 description]);
}

TEST(WeakNSObjectTest, WeakNSObjectResetWithObject) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  scoped_nsobject<NSObject> p2([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  w1.reset(p2);
  EXPECT_TRUE(w1);
  EXPECT_TRUE([p1 description]);
  EXPECT_TRUE([p2 description]);
}

TEST(WeakNSObjectTest, WeakNSObjectEmpty) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1;
  EXPECT_FALSE(w1);
  w1.reset(p1);
  EXPECT_TRUE(w1);
  p1.reset();
  EXPECT_FALSE(w1);
}

TEST(WeakNSObjectTest, WeakNSObjectCopy) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2(w1);
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

TEST(WeakNSObjectTest, WeakNSObjectAssignment) {
  scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2;
  EXPECT_FALSE(w2);
  w2 = w1;
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

// Touches |weak_data| by increasing its length by 1. Used to check that the
// weak object can be dereferenced.
void TouchWeakData(const WeakNSObject<NSMutableData>& weak_data) {
  if (!weak_data)
    return;
  [weak_data increaseLengthBy:1];
}

// Makes a copy of |weak_object| on the current thread and posts a task to touch
// the weak object on its original thread.
void CopyWeakNSObjectAndPost(const WeakNSObject<NSMutableData>& weak_object,
                             scoped_refptr<SingleThreadTaskRunner> runner) {
  // Copy using constructor.
  WeakNSObject<NSMutableData> weak_copy1(weak_object);
  runner->PostTask(FROM_HERE, BindOnce(&TouchWeakData, weak_copy1));
  // Copy using assignment operator.
  WeakNSObject<NSMutableData> weak_copy2 = weak_object;
  runner->PostTask(FROM_HERE, BindOnce(&TouchWeakData, weak_copy2));
}

// Tests that the weak object can be copied on a different thread.
TEST(WeakNSObjectTest, WeakNSObjectCopyOnOtherThread) {
  test::ScopedTaskEnvironment scoped_task_environment;
  Thread other_thread("WeakNSObjectCopyOnOtherThread");
  other_thread.Start();

  scoped_nsobject<NSMutableData> data([[NSMutableData alloc] init]);
  WeakNSObject<NSMutableData> weak(data);

  scoped_refptr<SingleThreadTaskRunner> runner = ThreadTaskRunnerHandle::Get();
  other_thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&CopyWeakNSObjectAndPost, weak, runner));
  other_thread.Stop();
  RunLoop().RunUntilIdle();

  // Check that TouchWeakData was called and the object touched twice.
  EXPECT_EQ(2u, [data length]);
}

}  // namespace
}  // namespace base
