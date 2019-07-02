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


#ifndef PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_TEST_BASE_H_
#define PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_TEST_BASE_H_

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/sharedmem/shared_mem_test_base.h"

namespace net_instaweb {

class StatisticsLogger;

class SharedMemStatisticsTestBase : public testing::Test {
 protected:
  typedef void (SharedMemStatisticsTestBase::*TestMethod)();

  static const int64 kLogIntervalMs;
  static const int64 kMaxLogfileSizeKb;

  SharedMemStatisticsTestBase();
  explicit SharedMemStatisticsTestBase(SharedMemTestEnv* test_env);

  virtual void SetUp();
  virtual void TearDown();
  bool CreateChild(TestMethod method);

  void TestCreate();
  void TestSet();
  void TestClear();
  void TestAdd();
  void TestSetReturningPrevious();
  void TestHistogram();
  void TestHistogramRender();
  void TestHistogramNoExtraClear();
  void TestHistogramExtremeBuckets();
  void TestTimedVariableEmulation();
  void TestConsoleStatisticsLogger();

  StatisticsLogger* console_logger() const {
    return stats_->console_logger_.get();
  }

  scoped_ptr<ThreadSystem> thread_system_;
  MockMessageHandler handler_;
  scoped_ptr<MemFileSystem> file_system_;
  scoped_ptr<SharedMemStatistics> stats_;  // (the parent process version)

 private:
  void TestCreateChild();
  void TestSetChild();
  void TestClearChild();
  void TestHistogramNoExtraClearChild();

  // Adds 10x +1 to variable 1, and 10x +2 to variable 2.
  void TestAddChild();
  bool AddVars(SharedMemStatistics* stats);
  bool AddHistograms(SharedMemStatistics* stats);
  // Helper function for TestHistogramRender().
  // Check if string html contains the pattern.
  bool Contains(const StringPiece& html, const StringPiece& pattern);

  SharedMemStatistics* ChildInit();
  void ParentInit();

  scoped_ptr<SharedMemTestEnv> test_env_;
  scoped_ptr<AbstractSharedMem> shmem_runtime_;
  scoped_ptr<MockTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemStatisticsTestBase);
};

template<typename ConcreteTestEnv>
class SharedMemStatisticsTestTemplate : public SharedMemStatisticsTestBase {
 public:
  SharedMemStatisticsTestTemplate()
      : SharedMemStatisticsTestBase(new ConcreteTestEnv) {
  }
};

TYPED_TEST_CASE_P(SharedMemStatisticsTestTemplate);

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestCreate) {
  SharedMemStatisticsTestBase::TestCreate();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestSet) {
  SharedMemStatisticsTestBase::TestSet();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestClear) {
  SharedMemStatisticsTestBase::TestClear();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestAdd) {
  SharedMemStatisticsTestBase::TestAdd();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestSetReturningPrevious) {
  SharedMemStatisticsTestBase::TestSetReturningPrevious();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogram) {
  SharedMemStatisticsTestBase::TestHistogram();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogramRender) {
  SharedMemStatisticsTestBase::TestHistogramRender();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogramExtremeBuckets) {
  SharedMemStatisticsTestBase::TestHistogramExtremeBuckets();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestHistogramNoExtraClear) {
  SharedMemStatisticsTestBase::TestHistogramNoExtraClear();
}

TYPED_TEST_P(SharedMemStatisticsTestTemplate, TestTimedVariableEmulation) {
  SharedMemStatisticsTestBase::TestTimedVariableEmulation();
}

REGISTER_TYPED_TEST_CASE_P(SharedMemStatisticsTestTemplate, TestCreate,
                           TestSet, TestClear, TestAdd,
                           TestSetReturningPrevious,
                           TestHistogram, TestHistogramRender,
                           TestHistogramNoExtraClear,
                           TestHistogramExtremeBuckets,
                           TestTimedVariableEmulation);

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_SHARED_MEM_STATISTICS_TEST_BASE_H_
