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


#include "pagespeed/kernel/base/split_statistics.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/sharedmem/inprocess_shared_mem.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/util/platform.h"

namespace net_instaweb {

namespace {

const char kVarA[] = "a";
const char kUpDownA[] = "aA";
const char kVarB[] = "b";
const char kVarGlobal[] = "global";
const char kHist[] = "histogram";
const char kTimedVar[] = "tv";

class SplitStatisticsTest : public testing::Test {
 public:
  SplitStatisticsTest()
      : threads_(Platform::CreateThreadSystem()),
        timer_(threads_->NewMutex(), MockTimer::kApr_5_2010_ms),
        fs_(threads_.get(), &timer_),
        global_(MakeInMemory(&global_store_)),
        local_a_(MakeInMemory(&local_a_store_)),
        split_a_(new SplitStatistics(threads_.get(), local_a_, global_.get())),
        local_b_(MakeInMemory(&local_b_store_)),
        split_b_(new SplitStatistics(threads_.get(), local_b_, global_.get())) {
    // Initialize in the documented order -- global & locals before their
    // splits. Also call Init() on the shared mem ones after that.
    InitStats(global_.get());
    global_->Init(true, &message_handler_);
    InitStats(local_a_);
    local_a_->Init(true, &message_handler_);
    InitStats(split_a_.get());
    InitStats(local_b_);
    local_b_->Init(true, &message_handler_);
    InitStats(split_b_.get());
  }

  ~SplitStatisticsTest() {
    local_b_->GlobalCleanup(&message_handler_);
    split_b_.reset(NULL);
    delete local_b_store_;

    local_a_->GlobalCleanup(&message_handler_);
    split_a_.reset(NULL);
    delete local_a_store_;

    global_->GlobalCleanup(&message_handler_);
    global_.reset(NULL);
    delete global_store_;
  }

 protected:
  void InitStats(Statistics* s) {
    s->AddVariable(kVarA);
    s->AddUpDownCounter(kUpDownA);
    s->AddVariable(kVarB);
    s->AddGlobalUpDownCounter(kVarGlobal);

    Histogram* h = s->AddHistogram(kHist);
    h->SetMinValue(1);
    h->SetMaxValue(101);
    h->SetSuggestedNumBuckets(100);

    s->AddTimedVariable(kTimedVar, "some group");
  }

  SharedMemStatistics* MakeInMemory(InProcessSharedMem** mem_runtime_out) {
    *mem_runtime_out = new InProcessSharedMem(threads_.get());
    return new SharedMemStatistics(3000,
                                   100000,
                                   "",  // statistics logging file (ignored)
                                   false,  // no statistics logging.
                                   "in_mem",
                                   *mem_runtime_out,
                                   &message_handler_,
                                   &fs_,
                                   &timer_);
  }

  GoogleMessageHandler message_handler_;
  scoped_ptr<ThreadSystem> threads_;
  MockTimer timer_;
  MemFileSystem fs_;

  scoped_ptr<SharedMemStatistics> global_;
  InProcessSharedMem* global_store_;
  SharedMemStatistics* local_a_;  // owned by split_a_
  InProcessSharedMem* local_a_store_;
  scoped_ptr<SplitStatistics> split_a_;
  SharedMemStatistics* local_b_;  // owned by split_b_
  InProcessSharedMem* local_b_store_;
  scoped_ptr<SplitStatistics> split_b_;
};

TEST_F(SplitStatisticsTest, BasicOperation) {
  Variable* aa = split_a_->GetVariable(kVarA);
  Variable* ab = split_a_->GetVariable(kVarB);
  Variable* ba = split_b_->GetVariable(kVarA);
  Variable* bb = split_b_->GetVariable(kVarB);
  ASSERT_TRUE(aa != NULL);
  ASSERT_TRUE(ab != NULL);
  ASSERT_TRUE(ba != NULL);
  ASSERT_TRUE(bb != NULL);

  aa->Add(1);
  ab->Add(2);
  ba->Add(10);
  bb->Add(15);

  // Locals, as well as splits themselves get just what was done to them.
  EXPECT_EQ(1, local_a_->GetVariable(kVarA)->Get());
  EXPECT_EQ(1, split_a_->GetVariable(kVarA)->Get());

  EXPECT_EQ(2, local_a_->GetVariable(kVarB)->Get());
  EXPECT_EQ(2, split_a_->GetVariable(kVarB)->Get());

  EXPECT_EQ(10, local_b_->GetVariable(kVarA)->Get());
  EXPECT_EQ(10, split_b_->GetVariable(kVarA)->Get());

  EXPECT_EQ(15, local_b_->GetVariable(kVarB)->Get());
  EXPECT_EQ(15, split_b_->GetVariable(kVarB)->Get());

  // Global has aggregates
  EXPECT_EQ(11, global_->GetVariable(kVarA)->Get());
  EXPECT_EQ(17, global_->GetVariable(kVarB)->Get());
}

TEST_F(SplitStatisticsTest, TestGlobal) {
  // kVarGlobal was added via AddGlobalUpDownCounter not AddVariable,
  // so split's return global counts on Get().
  UpDownCounter* split_a_global = split_a_->GetUpDownCounter(kVarGlobal);
  UpDownCounter* split_b_global = split_b_->GetUpDownCounter(kVarGlobal);
  UpDownCounter* local_a_global = local_a_->GetUpDownCounter(kVarGlobal);
  UpDownCounter* local_b_global = local_b_->GetUpDownCounter(kVarGlobal);
  UpDownCounter* global_global = global_->GetUpDownCounter(kVarGlobal);

  split_a_global->Add(5);
  split_b_global->Add(3);
  EXPECT_EQ(8, split_a_global->Get());
  EXPECT_EQ(5, local_a_global->Get());
  EXPECT_EQ(8, split_b_global->Get());
  EXPECT_EQ(3, local_b_global->Get());
  EXPECT_EQ(8, global_global->Get());
}

TEST_F(SplitStatisticsTest, GetName) {
  EXPECT_STREQ("a", split_a_->GetVariable(kVarA)->GetName());
  EXPECT_STREQ("b", split_a_->GetVariable(kVarB)->GetName());
  EXPECT_STREQ("a", split_b_->GetVariable(kVarA)->GetName());
  EXPECT_STREQ("b", split_b_->GetVariable(kVarB)->GetName());
}

TEST_F(SplitStatisticsTest, Set) {
  split_b_->GetVariable(kVarA)->Add(41);
  split_a_->GetVariable(kVarA)->Add(42);
  EXPECT_EQ(42, split_a_->GetVariable(kVarA)->Get());
  EXPECT_EQ(42, local_a_->GetVariable(kVarA)->Get());
  EXPECT_EQ(83, global_->GetVariable(kVarA)->Get());
  EXPECT_EQ(41, split_b_->GetVariable(kVarA)->Get());
  EXPECT_EQ(41, local_b_->GetVariable(kVarA)->Get());
}

TEST_F(SplitStatisticsTest, TestSetReturningPrevious) {
  UpDownCounter* var = global_->GetUpDownCounter(kUpDownA);
  EXPECT_EQ(0, var->SetReturningPreviousValue(5));
  EXPECT_EQ(5, var->SetReturningPreviousValue(-3));
  EXPECT_EQ(-3, var->SetReturningPreviousValue(10));
  EXPECT_EQ(10, var->Get());
}

TEST_F(SplitStatisticsTest, HistoOps) {
  Histogram* global_h = global_->GetHistogram(kHist);
  ASSERT_TRUE(global_h != NULL);
  Histogram* local_a_h = local_a_->GetHistogram(kHist);
  ASSERT_TRUE(local_a_h != NULL);
  Histogram* local_b_h = local_b_->GetHistogram(kHist);
  ASSERT_TRUE(local_b_h != NULL);
  Histogram* split_a_h = split_a_->GetHistogram(kHist);
  ASSERT_TRUE(split_a_h != NULL);
  Histogram* split_b_h = split_b_->GetHistogram(kHist);
  ASSERT_TRUE(split_b_h != NULL);

  // test that NumBuckets() forwards properly.
  ASSERT_EQ(local_a_h->NumBuckets(), split_a_h->NumBuckets());
  ASSERT_EQ(local_b_h->NumBuckets(), split_b_h->NumBuckets());
  // We also expect all of them to be configured the same,
  // due to our test setup.
  ASSERT_EQ(global_h->NumBuckets(), local_a_h->NumBuckets());
  ASSERT_EQ(global_h->NumBuckets(), local_b_h->NumBuckets());

  split_a_h->Add(1);
  split_a_h->Add(2);
  EXPECT_EQ(1, split_a_h->Minimum());
  EXPECT_EQ(1, local_a_h->Minimum());
  EXPECT_EQ(2, split_a_h->Maximum());
  EXPECT_EQ(2, local_a_h->Maximum());
  EXPECT_DOUBLE_EQ(1.5, split_a_h->Average());
  EXPECT_DOUBLE_EQ(1.5, local_a_h->Average());
  EXPECT_DOUBLE_EQ(2, split_a_h->Percentile(50));
  EXPECT_DOUBLE_EQ(2, local_a_h->Percentile(50));

  EXPECT_EQ(2, local_a_h->Count());
  EXPECT_EQ(2, split_a_h->Count());
  EXPECT_FALSE(local_a_h->Empty());
  EXPECT_FALSE(split_a_h->Empty());

  split_b_h->Add(3);
  split_b_h->Add(4);
  EXPECT_EQ(3, split_b_h->Minimum());
  EXPECT_EQ(3, local_b_h->Minimum());
  EXPECT_EQ(4, split_b_h->Maximum());
  EXPECT_EQ(4, local_b_h->Maximum());
  EXPECT_DOUBLE_EQ(3.5, split_b_h->Average());
  EXPECT_DOUBLE_EQ(3.5, local_b_h->Average());
  EXPECT_DOUBLE_EQ(4, split_b_h->Percentile(50));
  EXPECT_DOUBLE_EQ(4, local_b_h->Percentile(50));
  EXPECT_EQ(2, local_b_h->Count());
  EXPECT_EQ(2, split_b_h->Count());
  EXPECT_FALSE(local_b_h->Empty());
  EXPECT_FALSE(split_b_h->Empty());

  EXPECT_EQ(1, global_h->Minimum());
  EXPECT_EQ(4, global_h->Maximum());
  EXPECT_DOUBLE_EQ(2.5, global_h->Average());
  EXPECT_DOUBLE_EQ(3, global_h->Percentile(50));
  EXPECT_EQ(4, global_h->Count());
  EXPECT_FALSE(global_h->Empty());

  for (int bucket = 0; bucket < global_h->NumBuckets(); ++bucket) {
    EXPECT_DOUBLE_EQ(local_a_h->BucketStart(bucket),
                     split_a_h->BucketStart(bucket));
    EXPECT_DOUBLE_EQ(local_b_h->BucketLimit(bucket),
                     split_b_h->BucketLimit(bucket));
  }

  split_a_h->Clear();
  EXPECT_EQ(0, local_a_h->Count());
  EXPECT_EQ(0, split_a_h->Count());
  EXPECT_TRUE(local_a_h->Empty());
  EXPECT_TRUE(split_a_h->Empty());

  // Global is untouched by Clear, to permit independent clearing of
  // each vhost. 'b' is also unaffected, of course.
  EXPECT_EQ(2, local_b_h->Count());
  EXPECT_EQ(2, split_b_h->Count());
  EXPECT_EQ(4, global_h->Count());

  GoogleString local_render;
  GoogleString split_render;
  StringWriter write_local(&local_render);
  StringWriter write_split(&split_render);

  local_b_->RenderHistograms(&write_local, &message_handler_);
  split_b_->RenderHistograms(&write_split, &message_handler_);
  EXPECT_EQ(local_render, split_render);
}

TEST_F(SplitStatisticsTest, TimedVars) {
  TimedVariable* global_tv = global_->GetTimedVariable(kTimedVar);
  ASSERT_TRUE(global_tv != NULL);
  TimedVariable* local_a_tv = local_a_->GetTimedVariable(kTimedVar);
  ASSERT_TRUE(local_a_tv != NULL);
  TimedVariable* local_b_tv = local_b_->GetTimedVariable(kTimedVar);
  ASSERT_TRUE(local_b_tv != NULL);
  TimedVariable* split_a_tv = split_a_->GetTimedVariable(kTimedVar);
  ASSERT_TRUE(split_a_tv != NULL);
  TimedVariable* split_b_tv = split_b_->GetTimedVariable(kTimedVar);
  ASSERT_TRUE(split_b_tv != NULL);

  split_a_tv->IncBy(4);
  split_a_tv->IncBy(3);

  split_b_tv->IncBy(15);
  split_b_tv->IncBy(17);

  EXPECT_EQ(7, split_a_tv->Get(TimedVariable::START));
  EXPECT_EQ(7, local_a_tv->Get(TimedVariable::START));

  EXPECT_EQ(32, split_b_tv->Get(TimedVariable::START));
  EXPECT_EQ(32, local_b_tv->Get(TimedVariable::START));

  EXPECT_EQ(39, global_tv->Get(TimedVariable::START));

  split_a_tv->Clear();
  EXPECT_EQ(0, split_a_tv->Get(TimedVariable::START));
  EXPECT_EQ(0, local_a_tv->Get(TimedVariable::START));

  EXPECT_EQ(32, split_b_tv->Get(TimedVariable::START));
  EXPECT_EQ(32, local_b_tv->Get(TimedVariable::START));

  EXPECT_EQ(39, global_tv->Get(TimedVariable::START));
}

}  // namespace

}  // namespace net_instaweb
