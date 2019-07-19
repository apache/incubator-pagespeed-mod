// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/deferred_sequenced_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class DeferredSequencedTaskRunnerTest : public testing::Test {
 public:
  class ExecuteTaskOnDestructor : public RefCounted<ExecuteTaskOnDestructor> {
   public:
    ExecuteTaskOnDestructor(
        DeferredSequencedTaskRunnerTest* executor,
        int task_id)
        : executor_(executor),
          task_id_(task_id) {
    }
  private:
   friend class RefCounted<ExecuteTaskOnDestructor>;
   virtual ~ExecuteTaskOnDestructor() { executor_->ExecuteTask(task_id_); }
   DeferredSequencedTaskRunnerTest* executor_;
   int task_id_;
  };

  void ExecuteTask(int task_id) {
    AutoLock lock(lock_);
    executed_task_ids_.push_back(task_id);
  }

  void PostExecuteTask(int task_id) {
    runner_->PostTask(FROM_HERE,
                      BindOnce(&DeferredSequencedTaskRunnerTest::ExecuteTask,
                               Unretained(this), task_id));
  }

  void StartRunner() {
    runner_->Start();
  }

  void DoNothing(ExecuteTaskOnDestructor* object) {
  }

 protected:
  DeferredSequencedTaskRunnerTest()
      : runner_(
            new DeferredSequencedTaskRunner(ThreadTaskRunnerHandle::Get())) {}

  test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<DeferredSequencedTaskRunner> runner_;
  mutable Lock lock_;
  std::vector<int> executed_task_ids_;
};

TEST_F(DeferredSequencedTaskRunnerTest, Stopped) {
  PostExecuteTask(1);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre());
}

TEST_F(DeferredSequencedTaskRunnerTest, Start) {
  StartRunner();
  PostExecuteTask(1);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre(1));
}

TEST_F(DeferredSequencedTaskRunnerTest, StartWithMultipleElements) {
  StartRunner();
  for (int i = 1; i < 5; ++i)
    PostExecuteTask(i);

  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre(1, 2, 3, 4));
}

TEST_F(DeferredSequencedTaskRunnerTest, DeferredStart) {
  PostExecuteTask(1);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre());

  StartRunner();
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre(1));

  PostExecuteTask(2);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre(1, 2));
}

TEST_F(DeferredSequencedTaskRunnerTest, DeferredStartWithMultipleElements) {
  for (int i = 1; i < 5; ++i)
    PostExecuteTask(i);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre());

  StartRunner();
  for (int i = 5; i < 9; ++i)
    PostExecuteTask(i);
  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_, testing::ElementsAre(1, 2, 3, 4, 5, 6, 7, 8));
}

TEST_F(DeferredSequencedTaskRunnerTest, DeferredStartWithMultipleThreads) {
  {
    Thread thread1("DeferredSequencedTaskRunnerTestThread1");
    Thread thread2("DeferredSequencedTaskRunnerTestThread2");
    thread1.Start();
    thread2.Start();
    for (int i = 0; i < 5; ++i) {
      thread1.task_runner()->PostTask(
          FROM_HERE, BindOnce(&DeferredSequencedTaskRunnerTest::PostExecuteTask,
                              Unretained(this), 2 * i));
      thread2.task_runner()->PostTask(
          FROM_HERE, BindOnce(&DeferredSequencedTaskRunnerTest::PostExecuteTask,
                              Unretained(this), 2 * i + 1));
      if (i == 2) {
        thread1.task_runner()->PostTask(
            FROM_HERE, BindOnce(&DeferredSequencedTaskRunnerTest::StartRunner,
                                Unretained(this)));
      }
    }
  }

  RunLoop().RunUntilIdle();
  EXPECT_THAT(executed_task_ids_,
      testing::WhenSorted(testing::ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)));
}

TEST_F(DeferredSequencedTaskRunnerTest, ObjectDestructionOrder) {
  {
    Thread thread("DeferredSequencedTaskRunnerTestThread");
    thread.Start();
    runner_ = new DeferredSequencedTaskRunner(thread.task_runner());
    for (int i = 0; i < 5; ++i) {
      {
        // Use a block to ensure that no reference to |short_lived_object|
        // is kept on the main thread after it is posted to |runner_|.
        scoped_refptr<ExecuteTaskOnDestructor> short_lived_object =
            new ExecuteTaskOnDestructor(this, 2 * i);
        runner_->PostTask(
            FROM_HERE,
            BindOnce(&DeferredSequencedTaskRunnerTest::DoNothing,
                     Unretained(this), RetainedRef(short_lived_object)));
      }
      // |short_lived_object| with id |2 * i| should be destroyed before the
      // task |2 * i + 1| is executed.
      PostExecuteTask(2 * i + 1);
    }
    StartRunner();
  }

  // All |short_lived_object| with id |2 * i| are destroyed before the task
  // |2 * i + 1| is executed.
  EXPECT_THAT(executed_task_ids_,
              testing::ElementsAre(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
}

void GetRunsTasksInCurrentSequence(bool* result,
                                   scoped_refptr<SequencedTaskRunner> runner,
                                   OnceClosure quit) {
  *result = runner->RunsTasksInCurrentSequence();
  std::move(quit).Run();
}

TEST_F(DeferredSequencedTaskRunnerTest, RunsTasksInCurrentSequence) {
  scoped_refptr<DeferredSequencedTaskRunner> runner =
      MakeRefCounted<DeferredSequencedTaskRunner>();
  EXPECT_TRUE(runner->RunsTasksInCurrentSequence());

  Thread thread1("DeferredSequencedTaskRunnerTestThread1");
  thread1.Start();
  bool runs_task_in_current_thread = true;
  base::RunLoop run_loop;
  thread1.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&GetRunsTasksInCurrentSequence, &runs_task_in_current_thread,
               runner, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(runs_task_in_current_thread);
}

TEST_F(DeferredSequencedTaskRunnerTest, StartWithTaskRunner) {
  scoped_refptr<DeferredSequencedTaskRunner> runner =
      MakeRefCounted<DeferredSequencedTaskRunner>();
  bool run_called = false;
  base::RunLoop run_loop;
  runner->PostTask(FROM_HERE,
                   BindOnce(
                       [](bool* run_called, base::OnceClosure quit_closure) {
                         *run_called = true;
                         std::move(quit_closure).Run();
                       },
                       &run_called, run_loop.QuitClosure()));
  runner->StartWithTaskRunner(ThreadTaskRunnerHandle::Get());
  run_loop.Run();
  EXPECT_TRUE(run_called);
}

}  // namespace
}  // namespace base
