// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_pool_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/cfi_buildflags.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_features.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/test_task_factory.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/updateable_sequenced_task_runner.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/debug/leak_annotations.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
#include "base/win/com_init_util.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

namespace {

constexpr int kMaxNumForegroundThreads = 4;

struct TraitsExecutionModePair {
  TraitsExecutionModePair(const TaskTraits& traits,
                          TaskSourceExecutionMode execution_mode)
      : traits(traits), execution_mode(execution_mode) {}

  TaskTraits traits;
  TaskSourceExecutionMode execution_mode;
};

#if DCHECK_IS_ON()
// Returns whether I/O calls are allowed on the current thread.
bool GetIOAllowed() {
  const bool previous_value = ThreadRestrictions::SetIOAllowed(true);
  ThreadRestrictions::SetIOAllowed(previous_value);
  return previous_value;
}
#endif

// Verify that the current thread priority and I/O restrictions are appropriate
// to run a Task with |traits|.
// Note: ExecutionMode is verified inside TestTaskFactory.
void VerifyTaskEnvironment(const TaskTraits& traits, test::PoolType pool_type) {
  const bool should_run_at_background_thread_priority =
      CanUseBackgroundPriorityForWorkerThread() &&
      traits.priority() == TaskPriority::BEST_EFFORT &&
      traits.thread_policy() == ThreadPolicy::PREFER_BACKGROUND;

  EXPECT_EQ(should_run_at_background_thread_priority
                ? ThreadPriority::BACKGROUND
                : ThreadPriority::NORMAL,
            PlatformThread::GetCurrentThreadPriority());

#if DCHECK_IS_ON()
  // The #if above is required because GetIOAllowed() always returns true when
  // !DCHECK_IS_ON(), even when |traits| don't allow file I/O.
  EXPECT_EQ(traits.may_block(), GetIOAllowed());
#endif

  const std::string thread_name(PlatformThread::GetName());
  const bool is_single_threaded =
      (thread_name.find("SingleThread") != std::string::npos);

#if HAS_NATIVE_THREAD_POOL()
  // Native thread groups do not provide the ability to name threads.
  if (pool_type == test::PoolType::NATIVE && !is_single_threaded &&
      !should_run_at_background_thread_priority) {
    return;
  }
#endif

  // Verify that the thread the task is running on is named as expected.
  EXPECT_THAT(thread_name, ::testing::HasSubstr("ThreadPool"));

  EXPECT_THAT(thread_name,
              ::testing::HasSubstr(should_run_at_background_thread_priority
                                       ? "Background"
                                       : "Foreground"));

  if (is_single_threaded) {
    // SingleThread workers discriminate blocking/non-blocking tasks.
    if (traits.may_block()) {
      EXPECT_THAT(thread_name, ::testing::HasSubstr("Blocking"));
    } else {
      EXPECT_THAT(thread_name,
                  ::testing::Not(::testing::HasSubstr("Blocking")));
    }
  } else {
    EXPECT_THAT(thread_name, ::testing::Not(::testing::HasSubstr("Blocking")));
  }
}

void VerifyTaskEnvironmentAndSignalEvent(const TaskTraits& traits,
                                         test::PoolType pool_type,
                                         WaitableEvent* event) {
  DCHECK(event);
  VerifyTaskEnvironment(traits, pool_type);
  event->Signal();
}

void VerifyTimeAndTaskEnvironmentAndSignalEvent(const TaskTraits& traits,
                                                test::PoolType pool_type,
                                                TimeTicks expected_time,
                                                WaitableEvent* event) {
  DCHECK(event);
  EXPECT_LE(expected_time, TimeTicks::Now());
  VerifyTaskEnvironment(traits, pool_type);
  event->Signal();
}

void VerifyOrderAndTaskEnvironmentAndSignalEvent(
    const TaskTraits& traits,
    test::PoolType pool_type,
    WaitableEvent* expected_previous_event,
    WaitableEvent* event) {
  DCHECK(event);
  if (expected_previous_event)
    EXPECT_TRUE(expected_previous_event->IsSignaled());
  VerifyTaskEnvironment(traits, pool_type);
  event->Signal();
}

scoped_refptr<TaskRunner> CreateTaskRunnerAndExecutionMode(
    ThreadPoolImpl* thread_pool,
    const TaskTraits& traits,
    TaskSourceExecutionMode execution_mode,
    SingleThreadTaskRunnerThreadMode default_single_thread_task_runner_mode =
        SingleThreadTaskRunnerThreadMode::SHARED) {
  switch (execution_mode) {
    case TaskSourceExecutionMode::kParallel:
      return thread_pool->CreateTaskRunner(traits);
    case TaskSourceExecutionMode::kSequenced:
      return thread_pool->CreateSequencedTaskRunner(traits);
    case TaskSourceExecutionMode::kSingleThread: {
      return thread_pool->CreateSingleThreadTaskRunner(
          traits, default_single_thread_task_runner_mode);
    }
    case TaskSourceExecutionMode::kJob:
      break;
  }
  ADD_FAILURE() << "Unknown ExecutionMode";
  return nullptr;
}

class ThreadPostingTasks : public SimpleThread {
 public:
  // Creates a thread that posts Tasks to |thread_pool| with |traits| and
  // |execution_mode|.
  ThreadPostingTasks(ThreadPoolImpl* thread_pool,
                     const TaskTraits& traits,
                     test::PoolType pool_type,
                     TaskSourceExecutionMode execution_mode)
      : SimpleThread("ThreadPostingTasks"),
        traits_(traits),
        pool_type_(pool_type),
        factory_(CreateTaskRunnerAndExecutionMode(thread_pool,
                                                  traits,
                                                  execution_mode),
                 execution_mode) {}

  void WaitForAllTasksToRun() { factory_.WaitForAllTasksToRun(); }

 private:
  void Run() override {
    EXPECT_FALSE(factory_.task_runner()->RunsTasksInCurrentSequence());

    const size_t kNumTasksPerThread = 150;
    for (size_t i = 0; i < kNumTasksPerThread; ++i) {
      factory_.PostTask(test::TestTaskFactory::PostNestedTask::NO,
                        BindOnce(&VerifyTaskEnvironment, traits_, pool_type_));
    }
  }

  const TaskTraits traits_;
  test::PoolType pool_type_;
  test::TestTaskFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPostingTasks);
};

// Returns a vector with a TraitsExecutionModePair for each valid combination of
// {ExecutionMode, TaskPriority, ThreadPolicy, MayBlock()}.
std::vector<TraitsExecutionModePair> GetTraitsExecutionModePair() {
  std::vector<TraitsExecutionModePair> params;

  constexpr TaskSourceExecutionMode execution_modes[] = {
      TaskSourceExecutionMode::kParallel, TaskSourceExecutionMode::kSequenced,
      TaskSourceExecutionMode::kSingleThread};
  constexpr ThreadPolicy thread_policies[] = {
      ThreadPolicy::PREFER_BACKGROUND, ThreadPolicy::MUST_USE_FOREGROUND};

  for (TaskSourceExecutionMode execution_mode : execution_modes) {
    for (ThreadPolicy thread_policy : thread_policies) {
      for (size_t priority_index = static_cast<size_t>(TaskPriority::LOWEST);
           priority_index <= static_cast<size_t>(TaskPriority::HIGHEST);
           ++priority_index) {
        const TaskPriority priority = static_cast<TaskPriority>(priority_index);
        params.push_back(TraitsExecutionModePair(
            {ThreadPool(), priority, thread_policy}, execution_mode));
        params.push_back(TraitsExecutionModePair(
            {ThreadPool(), priority, thread_policy, MayBlock()},
            execution_mode));
      }
    }
  }

  return params;
}

class ThreadPoolImplTestBase : public testing::Test {
 public:
  ThreadPoolImplTestBase() : thread_pool_("Test") {}

  void EnableAllTasksUserBlocking() {
    should_enable_all_tasks_user_blocking_ = true;
  }

  void set_worker_thread_observer(
      WorkerThreadObserver* worker_thread_observer) {
    worker_thread_observer_ = worker_thread_observer;
  }

  void StartThreadPool(
      int max_num_foreground_threads = kMaxNumForegroundThreads,
      TimeDelta reclaim_time = TimeDelta::FromSeconds(30)) {
    SetupFeatures();

    ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
    init_params.suggested_reclaim_time = reclaim_time;

    thread_pool_.Start(init_params, worker_thread_observer_);
  }

  void TearDown() override {
    if (did_tear_down_)
      return;

    thread_pool_.FlushForTesting();
    thread_pool_.JoinForTesting();
    did_tear_down_ = true;
  }

  virtual test::PoolType GetPoolType() const = 0;

  ThreadPoolImpl thread_pool_;

 private:
  void SetupFeatures() {
    std::vector<base::Feature> features;

    if (should_enable_all_tasks_user_blocking_)
      features.push_back(kAllTasksUserBlocking);

#if HAS_NATIVE_THREAD_POOL()
    if (GetPoolType() == test::PoolType::NATIVE)
      features.push_back(kUseNativeThreadPool);
#endif

    if (!features.empty())
      feature_list_.InitWithFeatures(features, {});
  }

  base::test::ScopedFeatureList feature_list_;
  WorkerThreadObserver* worker_thread_observer_ = nullptr;
  bool did_tear_down_ = false;
  bool should_enable_all_tasks_user_blocking_ = false;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolImplTestBase);
};

class ThreadPoolImplTest : public ThreadPoolImplTestBase,
                           public testing::WithParamInterface<test::PoolType> {
 public:
  ThreadPoolImplTest() = default;

  test::PoolType GetPoolType() const override { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolImplTest);
};

class ThreadPoolImplTestAllTraitsExecutionModes
    : public ThreadPoolImplTestBase,
      public testing::WithParamInterface<
          std::tuple<test::PoolType, TraitsExecutionModePair>> {
 public:
  ThreadPoolImplTestAllTraitsExecutionModes() = default;

  test::PoolType GetPoolType() const override {
    return std::get<0>(GetParam());
  }
  TaskTraits GetTraits() const { return std::get<1>(GetParam()).traits; }
  TaskSourceExecutionMode GetExecutionMode() const {
    return std::get<1>(GetParam()).execution_mode;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadPoolImplTestAllTraitsExecutionModes);
};

}  // namespace

// Verifies that a Task posted via PostDelayedTask with parameterized TaskTraits
// and no delay runs on a thread with the expected priority and I/O
// restrictions. The ExecutionMode parameter is ignored by this test.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, PostDelayedTaskNoDelay) {
  StartThreadPool();
  WaitableEvent task_ran;
  thread_pool_.PostDelayedTask(
      FROM_HERE, GetTraits(),
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent, GetTraits(), GetPoolType(),
               Unretained(&task_ran)),
      TimeDelta());
  task_ran.Wait();
}

// Verifies that a Task posted via PostDelayedTask with parameterized
// TaskTraits and a non-zero delay runs on a thread with the expected priority
// and I/O restrictions after the delay expires. The ExecutionMode parameter is
// ignored by this test.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, PostDelayedTaskWithDelay) {
  StartThreadPool();
  WaitableEvent task_ran;
  thread_pool_.PostDelayedTask(
      FROM_HERE, GetTraits(),
      BindOnce(&VerifyTimeAndTaskEnvironmentAndSignalEvent, GetTraits(),
               GetPoolType(), TimeTicks::Now() + TestTimeouts::tiny_timeout(),
               Unretained(&task_ran)),
      TestTimeouts::tiny_timeout());
  task_ran.Wait();
}

// Verifies that Tasks posted via a TaskRunner with parameterized TaskTraits and
// ExecutionMode run on a thread with the expected priority and I/O restrictions
// and respect the characteristics of their ExecutionMode.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, PostTasksViaTaskRunner) {
  StartThreadPool();
  test::TestTaskFactory factory(
      CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                       GetExecutionMode()),
      GetExecutionMode());
  EXPECT_FALSE(factory.task_runner()->RunsTasksInCurrentSequence());

  const size_t kNumTasksPerTest = 150;
  for (size_t i = 0; i < kNumTasksPerTest; ++i) {
    factory.PostTask(
        test::TestTaskFactory::PostNestedTask::NO,
        BindOnce(&VerifyTaskEnvironment, GetTraits(), GetPoolType()));
  }

  factory.WaitForAllTasksToRun();
}

// Verifies that a task posted via PostDelayedTask without a delay doesn't run
// before Start() is called.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes,
       PostDelayedTaskNoDelayBeforeStart) {
  WaitableEvent task_running;
  thread_pool_.PostDelayedTask(
      FROM_HERE, GetTraits(),
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent, GetTraits(), GetPoolType(),
               Unretained(&task_running)),
      TimeDelta());

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartThreadPool();
  task_running.Wait();
}

// Verifies that a task posted via PostDelayedTask with a delay doesn't run
// before Start() is called.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes,
       PostDelayedTaskWithDelayBeforeStart) {
  WaitableEvent task_running;
  thread_pool_.PostDelayedTask(
      FROM_HERE, GetTraits(),
      BindOnce(&VerifyTimeAndTaskEnvironmentAndSignalEvent, GetTraits(),
               GetPoolType(), TimeTicks::Now() + TestTimeouts::tiny_timeout(),
               Unretained(&task_running)),
      TestTimeouts::tiny_timeout());

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartThreadPool();
  task_running.Wait();
}

// Verifies that a task posted via a TaskRunner doesn't run before Start() is
// called.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes,
       PostTaskViaTaskRunnerBeforeStart) {
  WaitableEvent task_running;
  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode())
      ->PostTask(FROM_HERE,
                 BindOnce(&VerifyTaskEnvironmentAndSignalEvent, GetTraits(),
                          GetPoolType(), Unretained(&task_running)));

  // Wait a little bit to make sure that the task doesn't run before Start().
  // Note: This test won't catch a case where the task runs just after the check
  // and before Start(). However, we expect the test to be flaky if the tested
  // code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_running.IsSignaled());

  StartThreadPool();

  // This should not hang if the task runs after Start().
  task_running.Wait();
}

// Verify that all tasks posted to a TaskRunner after Start() run in a
// USER_BLOCKING environment when the AllTasksUserBlocking variation param of
// the BrowserScheduler experiment is true.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes,
       AllTasksAreUserBlockingTaskRunner) {
  TaskTraits user_blocking_traits = GetTraits();
  user_blocking_traits.UpdatePriority(TaskPriority::USER_BLOCKING);

  EnableAllTasksUserBlocking();
  StartThreadPool();

  WaitableEvent task_running;
  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode())
      ->PostTask(FROM_HERE, BindOnce(&VerifyTaskEnvironmentAndSignalEvent,
                                     user_blocking_traits, GetPoolType(),
                                     Unretained(&task_running)));
  task_running.Wait();
}

// Verify that all tasks posted via PostDelayedTask() after Start() run in a
// USER_BLOCKING environment when the AllTasksUserBlocking variation param of
// the BrowserScheduler experiment is true.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, AllTasksAreUserBlocking) {
  TaskTraits user_blocking_traits = GetTraits();
  user_blocking_traits.UpdatePriority(TaskPriority::USER_BLOCKING);

  EnableAllTasksUserBlocking();
  StartThreadPool();

  WaitableEvent task_running;
  // Ignore |params.execution_mode| in this test.
  thread_pool_.PostDelayedTask(
      FROM_HERE, GetTraits(),
      BindOnce(&VerifyTaskEnvironmentAndSignalEvent, user_blocking_traits,
               GetPoolType(), Unretained(&task_running)),
      TimeDelta());
  task_running.Wait();
}

// Verifies that FlushAsyncForTesting() calls back correctly for all trait and
// execution mode pairs.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, FlushAsyncForTestingSimple) {
  StartThreadPool();

  WaitableEvent unblock_task;
  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode(),
                                   SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE, BindOnce(&test::WaitWithoutBlockingObserver,
                                     Unretained(&unblock_task)));

  WaitableEvent flush_event;
  thread_pool_.FlushAsyncForTesting(
      BindOnce(&WaitableEvent::Signal, Unretained(&flush_event)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(flush_event.IsSignaled());

  unblock_task.Signal();

  flush_event.Wait();
}

// Verifies that tasks only run when allowed by SetHasFence().
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, SetHasFence) {
  StartThreadPool();

  AtomicFlag can_run;
  WaitableEvent did_run;
  thread_pool_.SetHasFence(true);

  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode())
      ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                   EXPECT_TRUE(can_run.IsSet());
                   did_run.Signal();
                 }));

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  can_run.Set();
  thread_pool_.SetHasFence(false);
  did_run.Wait();
}

// Verifies that a call to SetHasFence(true) before Start() is honored.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, SetHasFenceBeforeStart) {
  thread_pool_.SetHasFence(true);
  StartThreadPool();

  AtomicFlag can_run;
  WaitableEvent did_run;

  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode())
      ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                   EXPECT_TRUE(can_run.IsSet());
                   did_run.Signal();
                 }));

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  can_run.Set();
  thread_pool_.SetHasFence(false);
  did_run.Wait();
}

// Verifies that BEST_EFFORT tasks only run when allowed by
// SetHasBestEffortFence().
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, SetHasBestEffortFence) {
  StartThreadPool();

  AtomicFlag can_run;
  WaitableEvent did_run;
  thread_pool_.SetHasBestEffortFence(true);

  CreateTaskRunnerAndExecutionMode(&thread_pool_, GetTraits(),
                                   GetExecutionMode())
      ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                   if (GetTraits().priority() == TaskPriority::BEST_EFFORT)
                     EXPECT_TRUE(can_run.IsSet());
                   did_run.Signal();
                 }));

  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  can_run.Set();
  thread_pool_.SetHasBestEffortFence(false);
  did_run.Wait();
}

// Spawns threads that simultaneously post Tasks to TaskRunners with various
// TaskTraits and ExecutionModes. Verifies that each Task runs on a thread with
// the expected priority and I/O restrictions and respects the characteristics
// of its ExecutionMode.
TEST_P(ThreadPoolImplTest, MultipleTraitsExecutionModePair) {
  StartThreadPool();
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (const auto& test_params : GetTraitsExecutionModePair()) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &thread_pool_, test_params.traits, GetPoolType(),
        test_params.execution_mode));
    threads_posting_tasks.back()->Start();
  }

  for (const auto& thread : threads_posting_tasks) {
    thread->WaitForAllTasksToRun();
    thread->Join();
  }
}

TEST_P(ThreadPoolImplTest,
       GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated) {
  StartThreadPool();

#if HAS_NATIVE_THREAD_POOL()
  if (GetPoolType() == test::PoolType::NATIVE)
    return;
#endif

  // GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated() does not support
  // TaskPriority::BEST_EFFORT.
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DCHECK_DEATH({
    thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
        {ThreadPool(), TaskPriority::BEST_EFFORT});
  });
  EXPECT_DCHECK_DEATH({
    thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
        {ThreadPool(), MayBlock(), TaskPriority::BEST_EFFORT});
  });

  EXPECT_EQ(4, thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                   {ThreadPool(), TaskPriority::USER_VISIBLE}));
  EXPECT_EQ(4, thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                   {ThreadPool(), MayBlock(), TaskPriority::USER_VISIBLE}));
  EXPECT_EQ(4, thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                   {ThreadPool(), TaskPriority::USER_BLOCKING}));
  EXPECT_EQ(4, thread_pool_.GetMaxConcurrentNonBlockedTasksWithTraitsDeprecated(
                   {ThreadPool(), MayBlock(), TaskPriority::USER_BLOCKING}));
}

// Verify that the RunsTasksInCurrentSequence() method of a SequencedTaskRunner
// returns false when called from a task that isn't part of the sequence.
TEST_P(ThreadPoolImplTest, SequencedRunsTasksInCurrentSequence) {
  StartThreadPool();
  auto single_thread_task_runner = thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool()}, SingleThreadTaskRunnerThreadMode::SHARED);
  auto sequenced_task_runner =
      thread_pool_.CreateSequencedTaskRunner({ThreadPool()});

  WaitableEvent task_ran;
  single_thread_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<TaskRunner> sequenced_task_runner,
             WaitableEvent* task_ran) {
            EXPECT_FALSE(sequenced_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          sequenced_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that the RunsTasksInCurrentSequence() method of a
// SingleThreadTaskRunner returns false when called from a task that isn't part
// of the sequence.
TEST_P(ThreadPoolImplTest, SingleThreadRunsTasksInCurrentSequence) {
  StartThreadPool();
  auto sequenced_task_runner =
      thread_pool_.CreateSequencedTaskRunner({ThreadPool()});
  auto single_thread_task_runner = thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool()}, SingleThreadTaskRunnerThreadMode::SHARED);

  WaitableEvent task_ran;
  sequenced_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<TaskRunner> single_thread_task_runner,
             WaitableEvent* task_ran) {
            EXPECT_FALSE(
                single_thread_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          single_thread_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

#if defined(OS_WIN)
TEST_P(ThreadPoolImplTest, COMSTATaskRunnersRunWithCOMSTA) {
  StartThreadPool();
  auto com_sta_task_runner = thread_pool_.CreateCOMSTATaskRunner(
      {ThreadPool()}, SingleThreadTaskRunnerThreadMode::SHARED);

  WaitableEvent task_ran;
  com_sta_task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](WaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::STA);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}
#endif  // defined(OS_WIN)

TEST_P(ThreadPoolImplTest, DelayedTasksNotRunAfterShutdown) {
  StartThreadPool();
  // As with delayed tasks in general, this is racy. If the task does happen to
  // run after Shutdown within the timeout, it will fail this test.
  //
  // The timeout should be set sufficiently long enough to ensure that the
  // delayed task did not run. 2x is generally good enough.
  //
  // A non-racy way to do this would be to post two sequenced tasks:
  // 1) Regular Post Task: A WaitableEvent.Wait
  // 2) Delayed Task: ADD_FAILURE()
  // and signalling the WaitableEvent after Shutdown() on a different thread
  // since Shutdown() will block. However, the cost of managing this extra
  // thread was deemed to be too great for the unlikely race.
  thread_pool_.PostDelayedTask(FROM_HERE, {ThreadPool()},
                               BindOnce([]() { ADD_FAILURE(); }),
                               TestTimeouts::tiny_timeout());
  thread_pool_.Shutdown();
  PlatformThread::Sleep(TestTimeouts::tiny_timeout() * 2);
}

#if defined(OS_POSIX)

TEST_P(ThreadPoolImplTest, FileDescriptorWatcherNoOpsAfterShutdown) {
  StartThreadPool();

  int pipes[2];
  ASSERT_EQ(0, pipe(pipes));

  scoped_refptr<TaskRunner> blocking_task_runner =
      thread_pool_.CreateSequencedTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN});
  blocking_task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](int read_fd) {
            std::unique_ptr<FileDescriptorWatcher::Controller> controller =
                FileDescriptorWatcher::WatchReadable(
                    read_fd, BindRepeating([]() { NOTREACHED(); }));

            // This test is for components that intentionally leak their
            // watchers at shutdown. We can't clean |controller| up because its
            // destructor will assert that it's being called from the correct
            // sequence. After the thread pool is shutdown, it is not
            // possible to run tasks on this sequence.
            //
            // Note: Do not inline the controller.release() call into the
            //       ANNOTATE_LEAKING_OBJECT_PTR as the annotation is removed
            //       by the preprocessor in non-LEAK_SANITIZER builds,
            //       effectively breaking this test.
            ANNOTATE_LEAKING_OBJECT_PTR(controller.get());
            controller.release();
          },
          pipes[0]));

  thread_pool_.Shutdown();

  constexpr char kByte = '!';
  ASSERT_TRUE(WriteFileDescriptor(pipes[1], &kByte, sizeof(kByte)));

  // Give a chance for the file watcher to fire before closing the handles.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  EXPECT_EQ(0, IGNORE_EINTR(close(pipes[0])));
  EXPECT_EQ(0, IGNORE_EINTR(close(pipes[1])));
}
#endif  // defined(OS_POSIX)

// Verify that tasks posted on the same sequence access the same values on
// SequenceLocalStorage, and tasks on different sequences see different values.
TEST_P(ThreadPoolImplTest, SequenceLocalStorage) {
  StartThreadPool();

  SequenceLocalStorageSlot<int> slot;
  auto sequenced_task_runner1 =
      thread_pool_.CreateSequencedTaskRunner({ThreadPool()});
  auto sequenced_task_runner2 =
      thread_pool_.CreateSequencedTaskRunner({ThreadPool()});

  sequenced_task_runner1->PostTask(
      FROM_HERE,
      BindOnce([](SequenceLocalStorageSlot<int>* slot) { slot->emplace(11); },
               &slot));

  sequenced_task_runner1->PostTask(
      FROM_HERE, BindOnce(
                     [](SequenceLocalStorageSlot<int>* slot) {
                       EXPECT_EQ(slot->GetOrCreateValue(), 11);
                     },
                     &slot));

  sequenced_task_runner2->PostTask(
      FROM_HERE, BindOnce(
                     [](SequenceLocalStorageSlot<int>* slot) {
                       EXPECT_NE(slot->GetOrCreateValue(), 11);
                     },
                     &slot));

  thread_pool_.FlushForTesting();
}

TEST_P(ThreadPoolImplTest, FlushAsyncNoTasks) {
  StartThreadPool();
  bool called_back = false;
  thread_pool_.FlushAsyncForTesting(
      BindOnce([](bool* called_back) { *called_back = true; },
               Unretained(&called_back)));
  EXPECT_TRUE(called_back);
}

namespace {

// Verifies that all strings passed as argument are found on the current stack.
// Ignores failures if this configuration doesn't have symbols.
void VerifyHasStringsOnStack(const std::string& pool_str,
                             const std::string& shutdown_behavior_str) {
  const std::string stack = debug::StackTrace().ToString();
  SCOPED_TRACE(stack);
  const bool stack_has_symbols =
      stack.find("WorkerThread") != std::string::npos;
  if (!stack_has_symbols)
    return;

  EXPECT_THAT(stack, ::testing::HasSubstr(pool_str));
  EXPECT_THAT(stack, ::testing::HasSubstr(shutdown_behavior_str));
}

}  // namespace

#if defined(OS_POSIX)
// Many POSIX bots flakily crash on |debug::StackTrace().ToString()|,
// https://crbug.com/840429.
#define MAYBE_IdentifiableStacks DISABLED_IdentifiableStacks
#elif defined(OS_WIN) && \
    (defined(ADDRESS_SANITIZER) || BUILDFLAG(CFI_CAST_CHECK))
// Hangs on WinASan and WinCFI (grabbing StackTrace() too slow?),
// https://crbug.com/845010#c7.
#define MAYBE_IdentifiableStacks DISABLED_IdentifiableStacks
#else
#define MAYBE_IdentifiableStacks IdentifiableStacks
#endif

// Integration test that verifies that workers have a frame on their stacks
// which easily identifies the type of worker and shutdown behavior (useful to
// diagnose issues from logs without memory dumps).
TEST_P(ThreadPoolImplTest, MAYBE_IdentifiableStacks) {
  StartThreadPool();

  // Shutdown behaviors and expected stack frames.
  constexpr std::pair<TaskShutdownBehavior, const char*> shutdown_behaviors[] =
      {{TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, "RunContinueOnShutdown"},
       {TaskShutdownBehavior::SKIP_ON_SHUTDOWN, "RunSkipOnShutdown"},
       {TaskShutdownBehavior::BLOCK_SHUTDOWN, "RunBlockShutdown"}};

  for (const auto& shutdown_behavior : shutdown_behaviors) {
    const TaskTraits traits = {ThreadPool(), shutdown_behavior.first};
    const TaskTraits best_effort_traits = {
        ThreadPool(), shutdown_behavior.first, TaskPriority::BEST_EFFORT};

    thread_pool_.CreateSequencedTaskRunner(traits)->PostTask(
        FROM_HERE, BindOnce(&VerifyHasStringsOnStack, "RunPooledWorker",
                            shutdown_behavior.second));
    thread_pool_.CreateSequencedTaskRunner(best_effort_traits)
        ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringsOnStack,
                                       "RunBackgroundPooledWorker",
                                       shutdown_behavior.second));

    thread_pool_
        .CreateSingleThreadTaskRunner(traits,
                                      SingleThreadTaskRunnerThreadMode::SHARED)
        ->PostTask(FROM_HERE,
                   BindOnce(&VerifyHasStringsOnStack, "RunSharedWorker",
                            shutdown_behavior.second));
    thread_pool_
        .CreateSingleThreadTaskRunner(best_effort_traits,
                                      SingleThreadTaskRunnerThreadMode::SHARED)
        ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringsOnStack,
                                       "RunBackgroundSharedWorker",
                                       shutdown_behavior.second));

    thread_pool_
        .CreateSingleThreadTaskRunner(
            traits, SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(FROM_HERE,
                   BindOnce(&VerifyHasStringsOnStack, "RunDedicatedWorker",
                            shutdown_behavior.second));
    thread_pool_
        .CreateSingleThreadTaskRunner(
            best_effort_traits, SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringsOnStack,
                                       "RunBackgroundDedicatedWorker",
                                       shutdown_behavior.second));

#if defined(OS_WIN)
    thread_pool_
        .CreateCOMSTATaskRunner(traits,
                                SingleThreadTaskRunnerThreadMode::SHARED)
        ->PostTask(FROM_HERE,
                   BindOnce(&VerifyHasStringsOnStack, "RunSharedCOMWorker",
                            shutdown_behavior.second));
    thread_pool_
        .CreateCOMSTATaskRunner(best_effort_traits,
                                SingleThreadTaskRunnerThreadMode::SHARED)
        ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringsOnStack,
                                       "RunBackgroundSharedCOMWorker",
                                       shutdown_behavior.second));

    thread_pool_
        .CreateCOMSTATaskRunner(traits,
                                SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(FROM_HERE,
                   BindOnce(&VerifyHasStringsOnStack, "RunDedicatedCOMWorker",
                            shutdown_behavior.second));
    thread_pool_
        .CreateCOMSTATaskRunner(best_effort_traits,
                                SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(FROM_HERE, BindOnce(&VerifyHasStringsOnStack,
                                       "RunBackgroundDedicatedCOMWorker",
                                       shutdown_behavior.second));
#endif  // defined(OS_WIN)
  }

  thread_pool_.FlushForTesting();
}

TEST_P(ThreadPoolImplTest, WorkerThreadObserver) {
#if HAS_NATIVE_THREAD_POOL()
  // WorkerThreads are not created (and hence not observed) when using the
  // native thread pools. We still start the ThreadPool in this case since
  // JoinForTesting is always called on TearDown, and DCHECKs that all thread
  // groups are started.
  if (GetPoolType() == test::PoolType::NATIVE) {
    StartThreadPool();
    return;
  }
#endif

  testing::StrictMock<test::MockWorkerThreadObserver> observer;
  set_worker_thread_observer(&observer);

  // A worker should be created for each thread group. After that, 4 threads
  // should be created for each SingleThreadTaskRunnerThreadMode (8 on Windows).
  const int kExpectedNumPoolWorkers =
      CanUseBackgroundPriorityForWorkerThread() ? 2 : 1;
  const int kExpectedNumSharedSingleThreadedWorkers =
      CanUseBackgroundPriorityForWorkerThread() ? 4 : 2;
  const int kExpectedNumDedicatedSingleThreadedWorkers = 4;

  const int kExpectedNumCOMSharedSingleThreadedWorkers =
#if defined(OS_WIN)
      kExpectedNumSharedSingleThreadedWorkers;
#else
      0;
#endif
  const int kExpectedNumCOMDedicatedSingleThreadedWorkers =
#if defined(OS_WIN)
      kExpectedNumDedicatedSingleThreadedWorkers;
#else
      0;
#endif

  EXPECT_CALL(observer, OnWorkerThreadMainEntry())
      .Times(kExpectedNumPoolWorkers + kExpectedNumSharedSingleThreadedWorkers +
             kExpectedNumDedicatedSingleThreadedWorkers +
             kExpectedNumCOMSharedSingleThreadedWorkers +
             kExpectedNumCOMDedicatedSingleThreadedWorkers);

  // Infinite detach time to prevent workers from invoking
  // OnWorkerThreadMainExit() earlier than expected.
  StartThreadPool(kMaxNumForegroundThreads, TimeDelta::Max());

  std::vector<scoped_refptr<SingleThreadTaskRunner>> task_runners;

  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::BEST_EFFORT},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::USER_BLOCKING},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));

  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::BEST_EFFORT},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::USER_BLOCKING},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateSingleThreadTaskRunner(
      {ThreadPool(), TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));

#if defined(OS_WIN)
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::BEST_EFFORT}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::USER_BLOCKING}, SingleThreadTaskRunnerThreadMode::SHARED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::SHARED));

  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::BEST_EFFORT},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::BEST_EFFORT, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::USER_BLOCKING},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
  task_runners.push_back(thread_pool_.CreateCOMSTATaskRunner(
      {TaskPriority::USER_BLOCKING, MayBlock()},
      SingleThreadTaskRunnerThreadMode::DEDICATED));
#endif

  for (auto& task_runner : task_runners)
    task_runner->PostTask(FROM_HERE, DoNothing());

  // Release single-threaded workers. This should cause dedicated workers to
  // invoke OnWorkerThreadMainExit().
  observer.AllowCallsOnMainExit(kExpectedNumDedicatedSingleThreadedWorkers +
                                kExpectedNumCOMDedicatedSingleThreadedWorkers);
  task_runners.clear();
  observer.WaitCallsOnMainExit();

  // Join all remaining workers. This should cause shared single-threaded
  // workers and thread pool workers to invoke OnWorkerThreadMainExit().
  observer.AllowCallsOnMainExit(kExpectedNumPoolWorkers +
                                kExpectedNumSharedSingleThreadedWorkers +
                                kExpectedNumCOMSharedSingleThreadedWorkers);
  TearDown();
  observer.WaitCallsOnMainExit();
}

namespace {

class MustBeDestroyed {
 public:
  MustBeDestroyed(bool* was_destroyed) : was_destroyed_(was_destroyed) {}
  ~MustBeDestroyed() { *was_destroyed_ = true; }

 private:
  bool* const was_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(MustBeDestroyed);
};

}  // namespace

// Regression test for https://crbug.com/945087.
TEST_P(ThreadPoolImplTestAllTraitsExecutionModes, NoLeakWhenPostingNestedTask) {
  StartThreadPool();

  SequenceLocalStorageSlot<std::unique_ptr<MustBeDestroyed>> sls;

  bool was_destroyed = false;
  auto must_be_destroyed = std::make_unique<MustBeDestroyed>(&was_destroyed);

  auto task_runner = CreateTaskRunnerAndExecutionMode(
      &thread_pool_, GetTraits(), GetExecutionMode());

  task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                          sls.emplace(std::move(must_be_destroyed));
                          task_runner->PostTask(FROM_HERE, DoNothing());
                        }));

  TearDown();

  // The TaskRunner should be deleted along with the Sequence and its
  // SequenceLocalStorage when dropping this reference.
  task_runner = nullptr;

  EXPECT_TRUE(was_destroyed);
}

namespace {

struct TaskRunnerAndEvents {
  TaskRunnerAndEvents(scoped_refptr<UpdateableSequencedTaskRunner> task_runner,
                      const TaskPriority updated_priority,
                      WaitableEvent* expected_previous_event)
      : task_runner(std::move(task_runner)),
        updated_priority(updated_priority),
        expected_previous_event(expected_previous_event) {}

  // The UpdateableSequencedTaskRunner.
  scoped_refptr<UpdateableSequencedTaskRunner> task_runner;

  // The priority to use in UpdatePriority().
  const TaskPriority updated_priority;

  // Signaled when a task blocking |task_runner| is scheduled.
  WaitableEvent scheduled;

  // Signaled to release the task blocking |task_runner|.
  WaitableEvent blocked;

  // Signaled in the task that runs following the priority update.
  WaitableEvent task_ran;

  // An event that should be signaled before the task following the priority
  // update runs.
  WaitableEvent* expected_previous_event;
};

// Create a series of sample task runners that will post tasks at various
// initial priorities, then update priority.
std::vector<std::unique_ptr<TaskRunnerAndEvents>> CreateTaskRunnersAndEvents(
    ThreadPoolImpl* thread_pool,
    ThreadPolicy thread_policy) {
  std::vector<std::unique_ptr<TaskRunnerAndEvents>> task_runners_and_events;

  // -----
  // Task runner that will start as USER_VISIBLE and update to USER_BLOCKING.
  // Its task is expected to run first.
  task_runners_and_events.push_back(std::make_unique<TaskRunnerAndEvents>(
      thread_pool->CreateUpdateableSequencedTaskRunner(TaskTraits(
          {ThreadPool(), TaskPriority::USER_VISIBLE, thread_policy})),
      TaskPriority::USER_BLOCKING, nullptr));

  // -----
  // Task runner that will start as BEST_EFFORT and update to USER_VISIBLE.
  // Its task is expected to run after the USER_BLOCKING task runner's task.
  task_runners_and_events.push_back(std::make_unique<TaskRunnerAndEvents>(
      thread_pool->CreateUpdateableSequencedTaskRunner(
          TaskTraits({ThreadPool(), TaskPriority::BEST_EFFORT, thread_policy})),
      TaskPriority::USER_VISIBLE, &task_runners_and_events.back()->task_ran));

  // -----
  // Task runner that will start as USER_BLOCKING and update to BEST_EFFORT. Its
  // task is expected to run asynchronously with the other two task runners'
  // tasks if background thread groups exist, or after the USER_VISIBLE task
  // runner's task if not.
  //
  // If the task following the priority update is expected to run in the
  // foreground group, it should be after the task posted to the TaskRunner
  // whose priority is updated to USER_VISIBLE.
  WaitableEvent* expected_previous_event =
      CanUseBackgroundPriorityForWorkerThread()
          ? nullptr
          : &task_runners_and_events.back()->task_ran;

  task_runners_and_events.push_back(std::make_unique<TaskRunnerAndEvents>(
      thread_pool->CreateUpdateableSequencedTaskRunner(TaskTraits(
          {ThreadPool(), TaskPriority::USER_BLOCKING, thread_policy})),
      TaskPriority::BEST_EFFORT, expected_previous_event));

  return task_runners_and_events;
}

// Update the priority of a sequence when it is not scheduled.
void TestUpdatePrioritySequenceNotScheduled(ThreadPoolImplTest* test,
                                            ThreadPolicy thread_policy) {
  // This test verifies that tasks run in priority order. With more than 1
  // thread per pool, it is possible that tasks don't run in order even if
  // threads got tasks from the PriorityQueue in order. Therefore, enforce a
  // maximum of 1 thread per pool.
  constexpr int kLocalMaxNumForegroundThreads = 1;

  test->StartThreadPool(kLocalMaxNumForegroundThreads);
  auto task_runners_and_events =
      CreateTaskRunnersAndEvents(&test->thread_pool_, thread_policy);

  // Prevent tasks from running.
  test->thread_pool_.SetHasFence(true);

  // Post tasks to multiple task runners while they are at initial priority.
  // They won't run immediately because of the call to SetHasFence(true) above.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->task_runner->PostTask(
        FROM_HERE,
        BindOnce(
            &VerifyOrderAndTaskEnvironmentAndSignalEvent,
            TaskTraits{ThreadPool(), task_runner_and_events->updated_priority,
                       thread_policy},
            test->GetPoolType(),
            // Native pools ignore the maximum number of threads per pool
            // and therefore don't guarantee that tasks run in priority
            // order (see comment at beginning of test).
            Unretained(
#if HAS_NATIVE_THREAD_POOL()
                test->GetPoolType() == test::PoolType::NATIVE
                    ? nullptr
                    :
#endif
                    task_runner_and_events->expected_previous_event),
            Unretained(&task_runner_and_events->task_ran)));
  }

  // Update the priorities of the task runners that posted the tasks.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->task_runner->UpdatePriority(
        task_runner_and_events->updated_priority);
  }

  // Allow tasks to run.
  test->thread_pool_.SetHasFence(false);

  for (auto& task_runner_and_events : task_runners_and_events)
    test::WaitWithoutBlockingObserver(&task_runner_and_events->task_ran);
}

// Update the priority of a sequence when it is scheduled, i.e. not currently
// in a priority queue.
void TestUpdatePrioritySequenceScheduled(ThreadPoolImplTest* test,
                                         ThreadPolicy thread_policy) {
  test->StartThreadPool();
  auto task_runners_and_events =
      CreateTaskRunnersAndEvents(&test->thread_pool_, thread_policy);

  // Post blocking tasks to all task runners to prevent tasks from being
  // scheduled later in the test.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->task_runner->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
          task_runner_and_events->scheduled.Signal();
          test::WaitWithoutBlockingObserver(&task_runner_and_events->blocked);
        }));

    ScopedAllowBaseSyncPrimitivesForTesting allow_base_sync_primitives;
    test::WaitWithoutBlockingObserver(&task_runner_and_events->scheduled);
  }

  // Update the priorities of the task runners while they are scheduled and
  // blocked.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->task_runner->UpdatePriority(
        task_runner_and_events->updated_priority);
  }

  // Post an additional task to each task runner.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->task_runner->PostTask(
        FROM_HERE,
        BindOnce(
            &VerifyOrderAndTaskEnvironmentAndSignalEvent,
            TaskTraits{ThreadPool(), task_runner_and_events->updated_priority,
                       thread_policy},
            test->GetPoolType(),
            Unretained(task_runner_and_events->expected_previous_event),
            Unretained(&task_runner_and_events->task_ran)));
  }

  // Unblock the task blocking each task runner, allowing the additional posted
  // tasks to run. Each posted task will verify that it has been posted with
  // updated priority when it runs.
  for (auto& task_runner_and_events : task_runners_and_events) {
    task_runner_and_events->blocked.Signal();
    test::WaitWithoutBlockingObserver(&task_runner_and_events->task_ran);
  }
}

}  // namespace

TEST_P(ThreadPoolImplTest,
       UpdatePrioritySequenceNotScheduled_PreferBackground) {
  TestUpdatePrioritySequenceNotScheduled(this, ThreadPolicy::PREFER_BACKGROUND);
}

TEST_P(ThreadPoolImplTest,
       UpdatePrioritySequenceNotScheduled_MustUseForeground) {
  TestUpdatePrioritySequenceNotScheduled(this,
                                         ThreadPolicy::MUST_USE_FOREGROUND);
}

TEST_P(ThreadPoolImplTest, UpdatePrioritySequenceScheduled_PreferBackground) {
  TestUpdatePrioritySequenceScheduled(this, ThreadPolicy::PREFER_BACKGROUND);
}

TEST_P(ThreadPoolImplTest, UpdatePrioritySequenceScheduled_MustUseForeground) {
  TestUpdatePrioritySequenceScheduled(this, ThreadPolicy::MUST_USE_FOREGROUND);
}

// Verify that a ThreadPolicy has to be specified in TaskTraits to increase
// TaskPriority from BEST_EFFORT.
TEST_P(ThreadPoolImplTest, UpdatePriorityFromBestEffortNoThreadPolicy) {
  StartThreadPool();
  {
    auto task_runner = thread_pool_.CreateUpdateableSequencedTaskRunner(
        {ThreadPool(), TaskPriority::BEST_EFFORT});
    EXPECT_DCHECK_DEATH(
        { task_runner->UpdatePriority(TaskPriority::USER_VISIBLE); });
  }
  {
    auto task_runner = thread_pool_.CreateUpdateableSequencedTaskRunner(
        {ThreadPool(), TaskPriority::BEST_EFFORT});
    EXPECT_DCHECK_DEATH(
        { task_runner->UpdatePriority(TaskPriority::USER_BLOCKING); });
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         ThreadPoolImplTest,
                         ::testing::Values(test::PoolType::GENERIC
#if HAS_NATIVE_THREAD_POOL()
                                           ,
                                           test::PoolType::NATIVE
#endif
                                           ));

INSTANTIATE_TEST_SUITE_P(
    ,
    ThreadPoolImplTestAllTraitsExecutionModes,
    ::testing::Combine(::testing::Values(test::PoolType::GENERIC
#if HAS_NATIVE_THREAD_POOL()
                                         ,
                                         test::PoolType::NATIVE
#endif
                                         ),
                       ::testing::ValuesIn(GetTraitsExecutionModePair())));

}  // namespace internal
}  // namespace base
