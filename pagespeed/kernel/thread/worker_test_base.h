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

// This contains things that are common between unit tests for Worker and its
// subclasses, such as runtime creation and various closures.

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/thread_system.h"

#ifndef PAGESPEED_KERNEL_THREAD_WORKER_TEST_BASE_H_
#define PAGESPEED_KERNEL_THREAD_WORKER_TEST_BASE_H_

namespace net_instaweb {

class WorkerTestBase : public ::testing::Test {
 public:
  class CountFunction;
  class SyncPoint;
  class NotifyRunFunction;
  class WaitRunFunction;
  class FailureFunction;

  WorkerTestBase();
  ~WorkerTestBase();

 protected:
  scoped_ptr<ThreadSystem> thread_runtime_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerTestBase);
};

// A closure that increments a variable on running.
class WorkerTestBase::CountFunction : public Function {
 public:
  explicit CountFunction(int* variable) : variable_(variable) {}

  virtual void Run() {
    ++*variable_;
  }

  virtual void Cancel() {
    *variable_ -= 100;
  }

 private:
  int* variable_;
  DISALLOW_COPY_AND_ASSIGN(CountFunction);
};

// A way for one thread to wait for another.
class WorkerTestBase::SyncPoint {
 public:
  explicit SyncPoint(ThreadSystem* thread_system);

  void Wait();
  void Notify();

 private:
  bool done_;
  scoped_ptr<ThreadSystem::CondvarCapableMutex> mutex_;
  scoped_ptr<ThreadSystem::Condvar> notify_;
  DISALLOW_COPY_AND_ASSIGN(SyncPoint);
};

// Notifies of itself having run on a given SyncPoint.
class WorkerTestBase::NotifyRunFunction : public Function {
 public:
  explicit NotifyRunFunction(SyncPoint* sync);
  virtual void Run();

 private:
  SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(NotifyRunFunction);
};

// Waits on a given SyncPoint before completing Run()
class WorkerTestBase::WaitRunFunction : public Function {
 public:
  explicit WaitRunFunction(SyncPoint* sync);
  virtual void Run();

 private:
  SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(WaitRunFunction);
};

// Function that signals on destruction and check fails when run.
class DeleteNotifyFunction : public Function {
 public:
  explicit DeleteNotifyFunction(WorkerTestBase::SyncPoint* sync)
      : sync_(sync) {}
  virtual ~DeleteNotifyFunction() {
    sync_->Notify();
  }

  virtual void Run() {
    LOG(FATAL) << "DeleteNotifyFunction ran.";
  }

 private:
  WorkerTestBase::SyncPoint* sync_;
  DISALLOW_COPY_AND_ASSIGN(DeleteNotifyFunction);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_WORKER_TEST_BASE_H_
