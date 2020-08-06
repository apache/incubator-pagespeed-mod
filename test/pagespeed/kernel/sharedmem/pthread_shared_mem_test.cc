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

#include "pagespeed/kernel/thread/pthread_shared_mem.h"

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <vector>

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/function.h"
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

// We test operation of pthread shared memory with both thread & process
// use, which is what PthreadSharedMemThreadEnv and PthreadSharedMemProcEnv
// provide.

class PthreadSharedMemEnvBase : public SharedMemTestEnv {
 public:
  AbstractSharedMem* CreateSharedMemRuntime() override {
    return new PthreadSharedMem();
  }

  void ShortSleep() override { usleep(1000); }
};

class PthreadSharedMemThreadEnv : public PthreadSharedMemEnvBase {
 public:
  bool CreateChild(Function* callback) override {
    pthread_t thread;
    if (pthread_create(&thread, nullptr, InvokeCallback, callback) != 0) {
      return false;
    }
    child_threads_.push_back(thread);
    return true;
  }

  void WaitForChildren() override {
    for (size_t i = 0; i < child_threads_.size(); ++i) {
      void* result = this;  // non-NULL -> failure.
      EXPECT_EQ(0, pthread_join(child_threads_[i], &result));
      EXPECT_EQ(nullptr, result) << "Child reported failure";
    }
    child_threads_.clear();
  }

  void ChildFailed() override {
    // In case of failure, we exit the thread with a non-NULL status.
    // We leak the callback object in that case, but this only gets called
    // for test failures anyway.
    pthread_exit(this);
  }

 private:
  static void* InvokeCallback(void* raw_callback_ptr) {
    Function* callback = static_cast<Function*>(raw_callback_ptr);
    callback->CallRun();
    return nullptr;  // Used to denote success
  }

  std::vector<pthread_t> child_threads_;
};

class PthreadSharedMemProcEnv : public PthreadSharedMemEnvBase {
 public:
  bool CreateChild(Function* callback) override {
    pid_t ret = fork();
    if (ret == -1) {
      // Failure
      callback->CallCancel();
      return false;
    } else if (ret == 0) {
      // Child.
      callback->CallRun();
      std::exit(0);
    } else {
      // Parent.
      child_processes_.push_back(ret);
      callback->CallCancel();
      return true;
    }
  }

  void WaitForChildren() override {
    for (size_t i = 0; i < child_processes_.size(); ++i) {
      int status;
      EXPECT_EQ(child_processes_[i], waitpid(child_processes_[i], &status, 0));
      EXPECT_TRUE(WIFEXITED(status)) << "Child did not exit cleanly";
      EXPECT_EQ(0, WEXITSTATUS(status)) << "Child reported failure";
    }
    child_processes_.clear();
  }

  void ChildFailed() override { exit(-1); }

 private:
  std::vector<pid_t> child_processes_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedCircularBufferTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedDynamicStringMapTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedMemCacheTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedMemCacheDataTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedMemLockManagerTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedMemStatisticsTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadProc, SharedMemTestTemplate,
                               PthreadSharedMemProcEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedCircularBufferTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread,
                               SharedDynamicStringMapTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedMemCacheTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedMemCacheDataTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedMemLockManagerTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedMemStatisticsTestTemplate,
                               PthreadSharedMemThreadEnv);
INSTANTIATE_TYPED_TEST_SUITE_P(PthreadThread, SharedMemTestTemplate,
                               PthreadSharedMemThreadEnv);

}  // namespace

}  // namespace net_instaweb
