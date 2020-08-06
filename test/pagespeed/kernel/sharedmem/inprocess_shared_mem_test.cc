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

// This tests the operation of the various SHM modules under
// the inprocess not-really-shared implementation.

#include "pagespeed/kernel/sharedmem/inprocess_shared_mem.h"

#include <unistd.h>

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/util/platform.h"
#include "test/pagespeed/kernel/base/gtest.h"
#include "test/pagespeed/kernel/sharedmem/shared_circular_buffer_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_dynamic_string_map_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_mem_cache_data_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_mem_cache_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_mem_lock_manager_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_mem_statistics_test_base.h"
#include "test/pagespeed/kernel/sharedmem/shared_mem_test_base.h"

namespace net_instaweb {

namespace {

class InProcessSharedMemEnv : public SharedMemTestEnv {
 public:
  InProcessSharedMemEnv() : thread_system_(Platform::CreateThreadSystem()) {}

  AbstractSharedMem* CreateSharedMemRuntime() override {
    return new InProcessSharedMem(thread_system_.get());
  }

  void ShortSleep() override { usleep(1000); }

  bool CreateChild(Function* callback) override {
    RunFunctionThread* thread =
        new RunFunctionThread(thread_system_.get(), callback);

    bool ok = thread->Start();
    if (!ok) {
      ADD_FAILURE() << "Problem starting child thread";
      return false;
    }
    child_threads_.push_back(thread);
    return true;
  }

  void WaitForChildren() override {
    for (size_t i = 0; i < child_threads_.size(); ++i) {
      child_threads_[i]->Join();
    }
    STLDeleteElements(&child_threads_);
  }

  void ChildFailed() override {
    // Unfortunately we don't have a clean way of signaling this.
    LOG(FATAL) << "Test failure in child thread";
  }

 protected:
  // Helper Thread subclass that just runs a function.
  class RunFunctionThread : public ThreadSystem::Thread {
   public:
    RunFunctionThread(ThreadSystem* runtime, Function* fn)
        : Thread(runtime, "thread_run", ThreadSystem::kJoinable), fn_(fn) {}

    void Run() override {
      fn_->CallRun();
      fn_ = nullptr;
    }

   private:
    Function* fn_;
    DISALLOW_COPY_AND_ASSIGN(RunFunctionThread);
  };

  std::unique_ptr<ThreadSystem> thread_system_;
  std::vector<ThreadSystem::Thread*> child_threads_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedCircularBufferTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedDynamicStringMapTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedMemCacheTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedMemCacheDataTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedMemLockManagerTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedMemStatisticsTestTemplate,
                               InProcessSharedMemEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(InprocessShm, SharedMemTestTemplate,
                               InProcessSharedMemEnv);

}  // namespace

}  // namespace net_instaweb
