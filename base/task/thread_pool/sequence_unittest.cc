// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/sequence.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

Task CreateTask(MockTask* mock_task) {
  return Task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(mock_task)),
              TimeDelta());
}

void ExpectMockTask(MockTask* mock_task, Task* task) {
  EXPECT_CALL(*mock_task, Run());
  std::move(task->task).Run();
  testing::Mock::VerifyAndClear(mock_task);
}

}  // namespace

TEST(ThreadPoolSequenceTest, PushTakeRemove) {
  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;
  testing::StrictMock<MockTask> mock_task_c;
  testing::StrictMock<MockTask> mock_task_d;
  testing::StrictMock<MockTask> mock_task_e;

  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool(), TaskPriority::BEST_EFFORT), nullptr,
      TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Push task A in the sequence. PushTask() should return true since it's the
  // first task->
  EXPECT_TRUE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_a));

  // Push task B, C and D in the sequence. PushTask() should return false since
  // there is already a task in a sequence.
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_b));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_c));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_d));

  // Take the task in front of the sequence. It should be task A.
  auto run_intent = sequence->WillRunTask();
  Optional<Task> task = sequence_transaction.TakeTask(&run_intent);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task B should now be in front.
  EXPECT_TRUE(sequence_transaction.DidProcessTask(std::move(run_intent)));

  EXPECT_FALSE(sequence_transaction.WillPushTask());
  run_intent = sequence->WillRunTask();
  task = sequence_transaction.TakeTask(&run_intent);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task C should now be in front.
  EXPECT_TRUE(sequence_transaction.DidProcessTask(std::move(run_intent)));

  EXPECT_FALSE(sequence_transaction.WillPushTask());
  run_intent = sequence->WillRunTask();
  task = sequence_transaction.TakeTask(&run_intent);
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot.
  EXPECT_TRUE(sequence_transaction.DidProcessTask(std::move(run_intent)));

  // Push task E in the sequence.
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_e));

  // Task D should be in front.
  run_intent = sequence->WillRunTask();
  task = sequence_transaction.TakeTask(&run_intent);
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task E should now be in front.
  EXPECT_TRUE(sequence_transaction.DidProcessTask(std::move(run_intent)));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  run_intent = sequence->WillRunTask();
  task = sequence_transaction.TakeTask(&run_intent);
  ExpectMockTask(&mock_task_e, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. The sequence should now be empty.
  EXPECT_FALSE(sequence_transaction.DidProcessTask(std::move(run_intent)));
  EXPECT_TRUE(sequence_transaction.WillPushTask());
}

// Verifies the sort key of a BEST_EFFORT sequence that contains one task.
TEST(ThreadPoolSequenceTest, GetSortKeyBestEffort) {
  // Create a BEST_EFFORT sequence with a task.
  Task best_effort_task(FROM_HERE, DoNothing(), TimeDelta());
  scoped_refptr<Sequence> best_effort_sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool(), TaskPriority::BEST_EFFORT), nullptr,
      TaskSourceExecutionMode::kParallel);
  Sequence::Transaction best_effort_sequence_transaction(
      best_effort_sequence->BeginTransaction());
  best_effort_sequence_transaction.PushTask(std::move(best_effort_task));

  // Get the sort key.
  const SequenceSortKey best_effort_sort_key =
      best_effort_sequence_transaction.GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto run_intent = best_effort_sequence->WillRunTask();
  auto take_best_effort_task =
      best_effort_sequence_transaction.TakeTask(&run_intent);

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::BEST_EFFORT, best_effort_sort_key.priority());
  EXPECT_EQ(take_best_effort_task->queue_time,
            best_effort_sort_key.next_task_sequenced_time());

  // DidProcessTask for correctness.
  best_effort_sequence_transaction.DidProcessTask(std::move(run_intent));
}

// Same as ThreadPoolSequenceTest.GetSortKeyBestEffort, but with a
// USER_VISIBLE sequence.
TEST(ThreadPoolSequenceTest, GetSortKeyForeground) {
  // Create a USER_VISIBLE sequence with a task.
  Task foreground_task(FROM_HERE, DoNothing(), TimeDelta());
  scoped_refptr<Sequence> foreground_sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool(), TaskPriority::USER_VISIBLE), nullptr,
      TaskSourceExecutionMode::kParallel);
  Sequence::Transaction foreground_sequence_transaction(
      foreground_sequence->BeginTransaction());
  foreground_sequence_transaction.PushTask(std::move(foreground_task));

  // Get the sort key.
  const SequenceSortKey foreground_sort_key =
      foreground_sequence_transaction.GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto run_intent = foreground_sequence->WillRunTask();
  auto take_foreground_task =
      foreground_sequence_transaction.TakeTask(&run_intent);

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::USER_VISIBLE, foreground_sort_key.priority());
  EXPECT_EQ(take_foreground_task->queue_time,
            foreground_sort_key.next_task_sequenced_time());

  // DidProcessTask for correctness.
  foreground_sequence_transaction.DidProcessTask(std::move(run_intent));
}

// Verify that a DCHECK fires if DidProcessTask() is called on a sequence which
// didn't return a Task.
TEST(ThreadPoolSequenceTest, DidProcessTaskWithoutTakeTask) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool()), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
  sequence_transaction.PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));

  EXPECT_DCHECK_DEATH({
    auto run_intent = sequence->WillRunTask();
    sequence_transaction.DidProcessTask(std::move(run_intent));
  });
}

// Verify that a DCHECK fires if TakeTask() is called on a sequence whose front
// slot is empty.
TEST(ThreadPoolSequenceTest, TakeEmptyFrontSlot) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool()), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
  sequence_transaction.PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));

  {
    auto run_intent = sequence->WillRunTask();
    EXPECT_TRUE(sequence_transaction.TakeTask(&run_intent));
    run_intent.ReleaseForTesting();
  }
  EXPECT_DCHECK_DEATH({
    auto run_intent = sequence->WillRunTask();
    auto task = sequence_transaction.TakeTask(&run_intent);
  });
}

// Verify that a DCHECK fires if TakeTask() is called on an empty sequence.
TEST(ThreadPoolSequenceTest, TakeEmptySequence) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(ThreadPool()), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
  auto run_intent = sequence->WillRunTask();
  EXPECT_DCHECK_DEATH(
      { auto task = sequence_transaction.TakeTask(&run_intent); });
  run_intent.ReleaseForTesting();
}

}  // namespace internal
}  // namespace base
