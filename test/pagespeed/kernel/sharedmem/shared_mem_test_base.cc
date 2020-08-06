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

#include "test/pagespeed/kernel/sharedmem/shared_mem_test_base.h"

#include <cstddef>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"
#include "test/pagespeed/kernel/base/gtest.h"
#include "test/pagespeed/kernel/base/mock_message_handler.h"

namespace net_instaweb {

namespace {
const char kTestSegment[] = "segment1";
const char kOtherSegment[] = "segment2";
}  // namespace

SharedMemTestEnv::~SharedMemTestEnv() {}

SharedMemTestBase::SharedMemTestBase(SharedMemTestEnv* test_env)
    : test_env_(test_env),
      shmem_runtime_(test_env->CreateSharedMemRuntime()),
      thread_system_(Platform::CreateThreadSystem()),
      handler_(thread_system_->NewMutex()) {}

bool SharedMemTestBase::CreateChild(TestMethod method) {
  Function* callback = new MemberFunction0<SharedMemTestBase>(method, this);
  return test_env_->CreateChild(callback);
}

void SharedMemTestBase::TestReadWrite(bool reattach) {
  std::unique_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TestReadWriteChild));

  if (reattach) {
    seg.reset(AttachDefault());
  }

  std::unique_ptr<AbstractMutex> mutex(AttachDefaultMutex(seg.get()));

  // Wait for kid to write out stuff
  mutex->Lock();
  while (*seg->Base() != '1') {
    mutex->Unlock();
    test_env_->ShortSleep();
    mutex->Lock();
  }
  mutex->Unlock();

  // Write out stuff.
  mutex->Lock();
  *seg->Base() = '2';
  mutex->Unlock();

  // Wait for termination.
  test_env_->WaitForChildren();
  seg.reset(nullptr);
  DestroyDefault();
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemTestBase::TestReadWriteChild() {
  std::unique_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  std::unique_ptr<AbstractMutex> mutex(AttachDefaultMutex(seg.get()));

  // Write out '1', which the parent will wait for.
  mutex->Lock();
  *seg->Base() = '1';
  mutex->Unlock();

  // Wait for '2' from parent
  mutex->Lock();
  while (*seg->Base() != '2') {
    mutex->Unlock();
    test_env_->ShortSleep();
    mutex->Lock();
  }
  mutex->Unlock();
}

void SharedMemTestBase::TestLarge() {
  std::unique_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->CreateSegment(kTestSegment, kLarge, &handler_));
  ASSERT_TRUE(seg.get() != nullptr);

  // Make sure everything is zeroed
  for (int c = 0; c < kLarge; ++c) {
    EXPECT_EQ(0, seg->Base()[c]);
  }
  seg.reset(nullptr);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TestLargeChild));
  test_env_->WaitForChildren();

  seg.reset(shmem_runtime_->AttachToSegment(kTestSegment, kLarge, &handler_));
  for (int i = 0; i < kLarge; i += 4) {
    EXPECT_EQ(i, *IntPtr(seg.get(), i));
  }

  DestroyDefault();
}

void SharedMemTestBase::TestLargeChild() {
  std::unique_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->AttachToSegment(kTestSegment, kLarge, &handler_));
  for (int i = 0; i < kLarge; i += 4) {
    *IntPtr(seg.get(), i) = i;
  }
}

// Make sure that 2 segments don't interfere.
void SharedMemTestBase::TestDistinct() {
  std::unique_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  std::unique_ptr<AbstractSharedMemSegment> seg2(
      shmem_runtime_->CreateSegment(kOtherSegment, 4, &handler_));
  ASSERT_TRUE(seg2.get() != nullptr);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg2Child));
  test_env_->WaitForChildren();

  EXPECT_EQ('1', *seg->Base());
  EXPECT_EQ('2', *seg2->Base());

  seg.reset(nullptr);
  seg2.reset(nullptr);
  DestroyDefault();
  shmem_runtime_->DestroySegment(kOtherSegment, &handler_);
  EXPECT_EQ(0, handler_.SeriousMessages());
}

// Make sure destruction destroys things properly...
void SharedMemTestBase::TestDestroy() {
  std::unique_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != nullptr);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  test_env_->WaitForChildren();
  EXPECT_EQ('1', *seg->Base());

  seg.reset(nullptr);
  DestroyDefault();

  // Attach should fail now
  seg.reset(AttachDefault());
  EXPECT_EQ(nullptr, seg.get());

  // Newly created one should have zeroed memory
  seg.reset(CreateDefault());
  EXPECT_EQ('\0', *seg->Base());

  DestroyDefault();
}

// Make sure that re-creating a segment without a Destroy is safe and
// produces a distinct segment
void SharedMemTestBase::TestCreateTwice() {
  std::unique_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::WriteSeg1Child));
  test_env_->WaitForChildren();
  EXPECT_EQ('1', *seg->Base());

  seg.reset(CreateDefault());
  EXPECT_EQ('\0', *seg->Base());
  DestroyDefault();
}

// Make sure between two kids see the SHM as well.
void SharedMemTestBase::TestTwoKids() {
  std::unique_ptr<AbstractSharedMemSegment> seg(CreateDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  seg.reset(nullptr);

  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TwoKidsChild1));
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::TwoKidsChild2));
  test_env_->WaitForChildren();
  seg.reset(AttachDefault());
  EXPECT_EQ('2', *seg->Base());

  DestroyDefault();
  EXPECT_EQ(0, handler_.SeriousMessages());
}

void SharedMemTestBase::TwoKidsChild1() {
  std::unique_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  std::unique_ptr<AbstractMutex> mutex(AttachDefaultMutex(seg.get()));
  // Write out '1', which the other kid will wait for.
  mutex->Lock();
  *seg->Base() = '1';
  mutex->Unlock();
}

void SharedMemTestBase::TwoKidsChild2() {
  std::unique_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  std::unique_ptr<AbstractMutex> mutex(AttachDefaultMutex(seg.get()));
  // Wait for '1'
  mutex->Lock();
  while (*seg->Base() != '1') {
    mutex->Unlock();
    test_env_->ShortSleep();
    mutex->Lock();
  }
  mutex->Unlock();

  *seg->Base() = '2';
}

// Test for mutex operation. This attempts to detect lack of mutual exclusion
// by hammering on a shared location (protected by a lock) with non-atomic
// increments. This test does not guarantee that it will detect a failure
// (the schedule might just end up such that things work out), but it's
// been found to be effective in practice.
void SharedMemTestBase::TestMutex() NO_THREAD_SAFETY_ANALYSIS {
  size_t mutex_size = shmem_runtime_->SharedMutexSize();
  std::unique_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->CreateSegment(kTestSegment, mutex_size + 4, &handler_));
  ASSERT_TRUE(seg.get() != nullptr);
  ASSERT_EQ(mutex_size, seg->SharedMutexSize());

  ASSERT_TRUE(seg->InitializeSharedMutex(0, &handler_));
  seg.reset(
      shmem_runtime_->AttachToSegment(kTestSegment, mutex_size + 4, &handler_));

  std::unique_ptr<AbstractMutex> mutex(seg->AttachToSharedMutex(0));
  mutex->Lock();
  ASSERT_TRUE(CreateChild(&SharedMemTestBase::MutexChild));

  // Unblock the kid. Before that, it shouldn't have written
  EXPECT_EQ(0, *IntPtr(seg.get(), mutex_size));
  mutex->Unlock();

  mutex->Lock();
  EXPECT_TRUE(IncrementStorm(seg.get(), mutex_size));
  mutex->Unlock();

  test_env_->WaitForChildren();
  DestroyDefault();
}

void SharedMemTestBase::MutexChild() {
  size_t mutex_size = shmem_runtime_->SharedMutexSize();
  std::unique_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->AttachToSegment(kTestSegment, mutex_size + 4, &handler_));
  ASSERT_TRUE(seg.get() != nullptr);

  std::unique_ptr<AbstractMutex> mutex(seg->AttachToSharedMutex(0));
  mutex->Lock();
  if (!IncrementStorm(seg.get(), mutex_size)) {
    mutex->Unlock();
    test_env_->ChildFailed();
    return;
  }
  mutex->Unlock();
}

// Returns if successful
bool SharedMemTestBase::IncrementStorm(AbstractSharedMemSegment* seg,
                                       size_t mutex_size) {
  // We are either the first or second to do the increments.
  int init = *IntPtr(seg, mutex_size);
  if ((init != 0) && (init != kNumIncrements)) {
    return false;
  }

  for (int i = 0; i < kNumIncrements; ++i) {
    ++*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 1)) {
      return false;
    }
    ++*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 2)) {
      return false;
    }
    --*IntPtr(seg, mutex_size);
    if (*IntPtr(seg, mutex_size) != (i + init + 1)) {
      return false;
    }
  }

  return true;
}

void SharedMemTestBase::WriteSeg1Child() {
  std::unique_ptr<AbstractSharedMemSegment> seg(AttachDefault());
  ASSERT_TRUE(seg.get() != nullptr);
  *seg->Base() = '1';
}

void SharedMemTestBase::WriteSeg2Child() {
  std::unique_ptr<AbstractSharedMemSegment> seg(
      shmem_runtime_->AttachToSegment(kOtherSegment, 4, &handler_));
  ASSERT_TRUE(seg.get() != nullptr);
  *seg->Base() = '2';
}

AbstractSharedMemSegment* SharedMemTestBase::CreateDefault() {
  AbstractSharedMemSegment* result = shmem_runtime_->CreateSegment(
      kTestSegment, 4 + shmem_runtime_->SharedMutexSize(), &handler_);
  EXPECT_TRUE(result->InitializeSharedMutex(4, &handler_));
  return result;
}

AbstractSharedMemSegment* SharedMemTestBase::AttachDefault() {
  return shmem_runtime_->AttachToSegment(
      kTestSegment, 4 + shmem_runtime_->SharedMutexSize(), &handler_);
}

AbstractMutex* SharedMemTestBase::AttachDefaultMutex(
    AbstractSharedMemSegment* segment) {
  return segment->AttachToSharedMutex(4);
}

void SharedMemTestBase::DestroyDefault() {
  shmem_runtime_->DestroySegment(kTestSegment, &handler_);
}

}  // namespace net_instaweb
