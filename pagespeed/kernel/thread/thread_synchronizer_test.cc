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


// Unit-test for ThreadSynchronizer

#include "pagespeed/kernel/thread/thread_synchronizer.h"

#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/util/platform.h"
#include "pagespeed/kernel/thread/queued_worker_pool.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/worker_test_base.h"

namespace net_instaweb {

namespace {

class ThreadSynchronizerTest : public testing::Test {
 public:
  ThreadSynchronizerTest()
      : thread_system_(Platform::CreateThreadSystem()),
        synchronizer_(thread_system_.get()),
        pool_(1, "thread_synchronizer_test", thread_system_.get()),
        sequence_(pool_.NewSequence()),
        sync_point_(new WorkerTestBase::SyncPoint(thread_system_.get())) {
  }

  ~ThreadSynchronizerTest() {
    sync_point_.reset(NULL);  // make sure this is destructed first.
  }

 protected:
  void AppendChar(char c) {
    buffer_ += c;
    synchronizer_.Signal("Thread:started");
    synchronizer_.Wait("Thread:unblock");
  }

  void AppendStringOneCharAtATime(const StringPiece& str) {
    for (int i = 0, n = str.size(); i < n; ++i) {
      sequence_->Add(MakeFunction(
          this, &ThreadSynchronizerTest::AppendChar, str[i]));
    }
  }

  void TestSyncDisabled() {
    // Queue up a bunch of functions.  By default, synchronizer_
    // is disabled so they will just execute without delay.  That is,
    // the calls to Wait and Signal in AppendChar will be no-ops.
    AppendStringOneCharAtATime("135");
    sequence_->Add(new WorkerTestBase::NotifyRunFunction(sync_point_.get()));
    sync_point_->Wait();
    EXPECT_EQ("135", buffer_);
  }

  scoped_ptr<ThreadSystem> thread_system_;
  ThreadSynchronizer synchronizer_;
  QueuedWorkerPool pool_;
  QueuedWorkerPool::Sequence* sequence_;
  scoped_ptr<WorkerTestBase::SyncPoint> sync_point_;
  GoogleString buffer_;
};

TEST_F(ThreadSynchronizerTest, SyncDisabled) {
  TestSyncDisabled();
}

TEST_F(ThreadSynchronizerTest, SyncWrongPrefix) {
  synchronizer_.EnableForPrefix("WrongPrefix_");

  // Despite having enabled the synchronizer, the prefix supplied does
  // not match the prefix we use in AppendChar above.  Thus the
  // testcase will behave exactly as if there were no sync-points, as
  // in SyncDisabled.  The sync-points will be no-ops.
  TestSyncDisabled();
}

TEST_F(ThreadSynchronizerTest, SyncEnabled) {
  synchronizer_.EnableForPrefix("Thread:");
  AppendStringOneCharAtATime("135");
  sequence_->Add(new WorkerTestBase::NotifyRunFunction(sync_point_.get()));

  // Wait for the thread to initiate, then signal it so it can complete the
  // first character.
  synchronizer_.Wait("Thread:started");
  EXPECT_EQ("1", buffer_);
  buffer_ += "2";
  synchronizer_.Signal("Thread:unblock");
  synchronizer_.Wait("Thread:started");
  EXPECT_EQ("123", buffer_);
  buffer_ += "4";
  synchronizer_.Signal("Thread:unblock");
  synchronizer_.Wait("Thread:started");
  EXPECT_EQ("12345", buffer_);
  synchronizer_.Signal("Thread:unblock");
  sync_point_->Wait();
  EXPECT_EQ("12345", buffer_);
}

TEST_F(ThreadSynchronizerTest, SignalInAdvance) {
  synchronizer_.EnableForPrefix("Thread:");
  synchronizer_.Signal("Thread:unblock");
  synchronizer_.Signal("Thread:unblock");
  synchronizer_.Signal("Thread:unblock");
  AppendStringOneCharAtATime("135");
  sequence_->Add(new WorkerTestBase::NotifyRunFunction(sync_point_.get()));
  sync_point_->Wait();

  // It's an error to let the 3 pending "Thread:started" signals go unwaited
  // on exit, so "wait" for them now -- it won't actually even block.
  synchronizer_.Wait("Thread:started");
  synchronizer_.Wait("Thread:started");
  synchronizer_.Wait("Thread:started");

  EXPECT_EQ("135", buffer_);
}

TEST_F(ThreadSynchronizerTest, TimedWait) {
  synchronizer_.EnableForPrefix("Thread:");
  synchronizer_.TimedWait("Thread:NeverComing", 10 /* ms */);
  synchronizer_.Signal("Thread:NeverComing");
}

TEST_F(ThreadSynchronizerTest, AllowSloppyNeverSignaled) {
  synchronizer_.EnableForPrefix("Thread:");
  synchronizer_.TimedWait("Thread:NeverComing", 10 /* ms */);
  synchronizer_.AllowSloppyTermination("Thread:NeverComing");
}

TEST_F(ThreadSynchronizerTest, AllowSloppyNeverWaited) {
  synchronizer_.EnableForPrefix("Thread:");
  synchronizer_.Signal("Thread:NeverWaited");
  synchronizer_.AllowSloppyTermination("Thread:NeverWaited");
}

}  // namespace

}  // namespace net_instaweb
