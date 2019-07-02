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


#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"

namespace net_instaweb {

class SharedCircularBuffer;
class ThreadSystem;

// This TestBase is added to pthread_shared_mem_test
class SharedCircularBufferTestBase : public testing::Test {
 protected:
  typedef void (SharedCircularBufferTestBase::*TestMethod)();

  explicit SharedCircularBufferTestBase(SharedMemTestEnv* test_env);

  bool CreateChild(TestMethod method);

  // Test basic initialization/writing/cleanup.
  void TestCreate();
  // Test writing from child process.
  void TestAdd();
  // Test cleanup from child process.
  void TestClear();
  // Test the shared memory circular buffer.
  void TestCircular();

 private:
  // Helper functions.
  void TestCreateChild();
  void TestAddChild();
  void TestClearChild();
  // Write to SharedCircularBuffer in a child process.
  void TestChildWrite();
  // Check content of SharedCircularBuffer in a child process.
  void TestChildBuff();

  // Initialize SharedMemoryCircularBuffer from child process.
  SharedCircularBuffer* ChildInit();
  // Initialize SharedMemoryCircularBuffer from root process.
  SharedCircularBuffer* ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;
  NullMessageHandler null_handler_;
  // Message to write in Child process.
  // We can't pass in argument in callback functions in this TestBase,
  // stick value to member variable instead.
  StringPiece message_;
  // Expected content of SharedCircularBuffer.
  // Used to check buffer content in a child process.
  StringPiece expected_result_;

  DISALLOW_COPY_AND_ASSIGN(SharedCircularBufferTestBase);
};

template<typename ConcreteTestEnv>
class SharedCircularBufferTestTemplate : public SharedCircularBufferTestBase {
 public:
  SharedCircularBufferTestTemplate()
      : SharedCircularBufferTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedCircularBufferTestTemplate);

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestCreate) {
  SharedCircularBufferTestBase::TestCreate();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestAdd) {
  SharedCircularBufferTestBase::TestAdd();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestClear) {
  SharedCircularBufferTestBase::TestClear();
}

TYPED_TEST_P(SharedCircularBufferTestTemplate, TestCircular) {
  SharedCircularBufferTestBase::TestCircular();
}

REGISTER_TYPED_TEST_CASE_P(SharedCircularBufferTestTemplate, TestCreate,
                           TestAdd, TestClear, TestCircular);

}  // namespace net_instaweb
#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_CIRCULAR_BUFFER_TEST_BASE_H_
