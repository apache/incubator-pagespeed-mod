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

// Unit-test for QueuedWorker

#include "pagespeed/kernel/thread/queued_worker.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/thread/worker_test_base.h"

namespace net_instaweb {
namespace {

class QueuedWorkerTest: public WorkerTestBase {
 public:
  QueuedWorkerTest()
      : worker_(new QueuedWorker("queued_worker_test", thread_runtime_.get())) {
  }

 protected:
  scoped_ptr<QueuedWorker> worker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QueuedWorkerTest);
};

// A closure that enqueues a new version of itself 'count' times, and
// finally schedules the running of the sync-point.
class ChainedTask : public Function {
 public:
  ChainedTask(int* count, QueuedWorker* worker, WorkerTestBase::SyncPoint* sync)
      : count_(count),
        worker_(worker),
        sync_(sync) {
  }

  virtual void Run() {
    --*count_;
    if (*count_ > 0) {
      worker_->RunInWorkThread(new ChainedTask(count_, worker_, sync_));
    } else {
      worker_->RunInWorkThread(new WorkerTestBase::NotifyRunFunction(sync_));
    }
  }

 private:
  int* count_;
  QueuedWorker* worker_;
  WorkerTestBase::SyncPoint* sync_;

  DISALLOW_COPY_AND_ASSIGN(ChainedTask);
};

TEST_F(QueuedWorkerTest, BasicOperation) {
  // All the jobs we queued should be run in order
  const int kBound = 42;
  int count = 0;
  SyncPoint sync(thread_runtime_.get());

  worker_->Start();
  for (int i = 0; i < kBound; ++i) {
    worker_->RunInWorkThread(new CountFunction(&count));
  }

  worker_->RunInWorkThread(new NotifyRunFunction(&sync));
  sync.Wait();
  EXPECT_EQ(kBound, count);
}

TEST_F(QueuedWorkerTest, ChainedTasks) {
  // The ChainedTask closure ensures that there is always a task
  // queued until we've executed all 11 tasks in the chain, at which
  // point the 'notify' function fires and we can complete the test.
  int count = 11;
  SyncPoint sync(thread_runtime_.get());
  worker_->Start();
  worker_->RunInWorkThread(new ChainedTask(&count, worker_.get(), &sync));
  sync.Wait();
  EXPECT_EQ(0, count);
}

TEST_F(QueuedWorkerTest, TestShutDown) {
  // Make sure that shutdown cancels jobs put in after it --- that
  // the job gets deleted (making clean.Wait() return), and doesn't
  // run (which would LOG(FATAL)).
  SyncPoint clean(thread_runtime_.get());
  worker_->Start();
  worker_->ShutDown();
  worker_->RunInWorkThread(new DeleteNotifyFunction(&clean));
  clean.Wait();
}

TEST_F(QueuedWorkerTest, TestIsBusy) {
  worker_->Start();
  EXPECT_FALSE(worker_->IsBusy());

  SyncPoint start_sync(thread_runtime_.get());
  worker_->RunInWorkThread(new WaitRunFunction(&start_sync));
  EXPECT_TRUE(worker_->IsBusy());
  start_sync.Notify();
  worker_->ShutDown();
  EXPECT_FALSE(worker_->IsBusy());
}

}  // namespace
}  // namespace net_instaweb
