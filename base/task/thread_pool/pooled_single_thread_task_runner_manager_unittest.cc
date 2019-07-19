// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/pooled_single_thread_task_runner_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/can_run_policy_test.h"
#include "base/task/thread_pool/delayed_task_manager.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>

#include "base/win/com_init_util.h"
#include "base/win/current_module.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

namespace {

class PooledSingleThreadTaskRunnerManagerTest : public testing::Test {
 protected:
  PooledSingleThreadTaskRunnerManagerTest()
      : service_thread_("ThreadPoolServiceThread") {}

  void SetUp() override {
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    single_thread_task_runner_manager_ =
        std::make_unique<PooledSingleThreadTaskRunnerManager>(
            task_tracker_.GetTrackedRef(), &delayed_task_manager_);
    StartSingleThreadTaskRunnerManagerFromSetUp();
  }

  void TearDown() override {
    if (single_thread_task_runner_manager_)
      TearDownSingleThreadTaskRunnerManager();
    service_thread_.Stop();
  }

  virtual void StartSingleThreadTaskRunnerManagerFromSetUp() {
    single_thread_task_runner_manager_->Start();
  }

  virtual void TearDownSingleThreadTaskRunnerManager() {
    single_thread_task_runner_manager_->JoinForTesting();
    single_thread_task_runner_manager_.reset();
  }

  Thread service_thread_;
  TaskTracker task_tracker_{"Test"};
  DelayedTaskManager delayed_task_manager_;
  std::unique_ptr<PooledSingleThreadTaskRunnerManager>
      single_thread_task_runner_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PooledSingleThreadTaskRunnerManagerTest);
};

void CaptureThreadRef(PlatformThreadRef* thread_ref) {
  ASSERT_TRUE(thread_ref);
  *thread_ref = PlatformThread::CurrentRef();
}

void ShouldNotRun() {
  ADD_FAILURE() << "Ran a task that shouldn't run.";
}

}  // namespace

TEST_F(PooledSingleThreadTaskRunnerManagerTest, DifferentThreadsUsed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::DEDICATED);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::DEDICATED);

  PlatformThreadRef thread_ref_1;
  task_runner_1->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_1));
  PlatformThreadRef thread_ref_2;
  task_runner_2->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_2));

  test::ShutdownTaskTracker(&task_tracker_);

  ASSERT_FALSE(thread_ref_1.is_null());
  ASSERT_FALSE(thread_ref_2.is_null());
  EXPECT_NE(thread_ref_1, thread_ref_2);
}

TEST_F(PooledSingleThreadTaskRunnerManagerTest, SameThreadUsed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED);

  PlatformThreadRef thread_ref_1;
  task_runner_1->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_1));
  PlatformThreadRef thread_ref_2;
  task_runner_2->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_2));

  test::ShutdownTaskTracker(&task_tracker_);

  ASSERT_FALSE(thread_ref_1.is_null());
  ASSERT_FALSE(thread_ref_2.is_null());
  EXPECT_EQ(thread_ref_1, thread_ref_2);
}

TEST_F(PooledSingleThreadTaskRunnerManagerTest, RunsTasksInCurrentSequence) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::DEDICATED);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::DEDICATED);

  EXPECT_FALSE(task_runner_1->RunsTasksInCurrentSequence());
  EXPECT_FALSE(task_runner_2->RunsTasksInCurrentSequence());

  task_runner_1->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<SingleThreadTaskRunner> task_runner_1,
             scoped_refptr<SingleThreadTaskRunner> task_runner_2) {
            EXPECT_TRUE(task_runner_1->RunsTasksInCurrentSequence());
            EXPECT_FALSE(task_runner_2->RunsTasksInCurrentSequence());
          },
          task_runner_1, task_runner_2));

  task_runner_2->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<SingleThreadTaskRunner> task_runner_1,
             scoped_refptr<SingleThreadTaskRunner> task_runner_2) {
            EXPECT_FALSE(task_runner_1->RunsTasksInCurrentSequence());
            EXPECT_TRUE(task_runner_2->RunsTasksInCurrentSequence());
          },
          task_runner_1, task_runner_2));

  test::ShutdownTaskTracker(&task_tracker_);
}

TEST_F(PooledSingleThreadTaskRunnerManagerTest,
       SharedWithBaseSyncPrimitivesDCHECKs) {
  testing::GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DCHECK_DEATH({
    single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
        {ThreadPool(), WithBaseSyncPrimitives()},
        SingleThreadTaskRunnerThreadMode::SHARED);
  });
}

// Regression test for https://crbug.com/829786
TEST_F(PooledSingleThreadTaskRunnerManagerTest,
       ContinueOnShutdownDoesNotBlockBlockShutdown) {
  WaitableEvent task_has_started;
  WaitableEvent task_can_continue;

  // Post a CONTINUE_ON_SHUTDOWN task that waits on
  // |task_can_continue| to a shared SingleThreadTaskRunner.
  single_thread_task_runner_manager_
      ->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE, base::BindOnce(
                                [](WaitableEvent* task_has_started,
                                   WaitableEvent* task_can_continue) {
                                  task_has_started->Signal();
                                  ScopedAllowBaseSyncPrimitivesForTesting
                                      allow_base_sync_primitives;
                                  task_can_continue->Wait();
                                },
                                Unretained(&task_has_started),
                                Unretained(&task_can_continue)));

  task_has_started.Wait();

  // Post a BLOCK_SHUTDOWN task to a shared SingleThreadTaskRunner.
  single_thread_task_runner_manager_
      ->CreateSingleThreadTaskRunner(
          {ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED)
      ->PostTask(FROM_HERE, DoNothing());

  // Shutdown should not hang even though the first task hasn't finished.
  test::ShutdownTaskTracker(&task_tracker_);

  // Let the first task finish.
  task_can_continue.Signal();

  // Tear down from the test body to prevent accesses to |task_can_continue|
  // after it goes out of scope.
  TearDownSingleThreadTaskRunnerManager();
}

namespace {

class PooledSingleThreadTaskRunnerManagerCommonTest
    : public PooledSingleThreadTaskRunnerManagerTest,
      public ::testing::WithParamInterface<SingleThreadTaskRunnerThreadMode> {
 public:
  PooledSingleThreadTaskRunnerManagerCommonTest() = default;

  scoped_refptr<SingleThreadTaskRunner> CreateTaskRunner(TaskTraits traits = {
                                                             ThreadPool()}) {
    return single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
        traits, GetParam());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PooledSingleThreadTaskRunnerManagerCommonTest);
};

}  // namespace

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, PrioritySetCorrectly) {
  const struct {
    TaskTraits traits;
    ThreadPriority expected_thread_priority;
  } test_cases[] = {
      {{ThreadPool(), TaskPriority::BEST_EFFORT},
       CanUseBackgroundPriorityForWorkerThread() ? ThreadPriority::BACKGROUND
                                                 : ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::PREFER_BACKGROUND},
       CanUseBackgroundPriorityForWorkerThread() ? ThreadPriority::BACKGROUND
                                                 : ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::MUST_USE_FOREGROUND},
       ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_VISIBLE}, ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::PREFER_BACKGROUND},
       ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::MUST_USE_FOREGROUND},
       ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_BLOCKING}, ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::PREFER_BACKGROUND},
       ThreadPriority::NORMAL},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::MUST_USE_FOREGROUND},
       ThreadPriority::NORMAL}};

  // Why are events used here instead of the task tracker?
  // Shutting down can cause priorities to get raised. This means we have to use
  // events to determine when a task is run.
  for (auto& test_case : test_cases) {
    WaitableEvent event;
    CreateTaskRunner(test_case.traits)
        ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                     EXPECT_EQ(test_case.expected_thread_priority,
                               PlatformThread::GetCurrentThreadPriority());
                     event.Signal();
                   }));
    event.Wait();
  }
}

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, ThreadNamesSet) {
  const std::string maybe_shared(
      GetParam() == SingleThreadTaskRunnerThreadMode::DEDICATED ? ""
                                                                : "Shared");
  const std::string background =
      "^ThreadPoolSingleThread" + maybe_shared + "Background\\d+$";
  const std::string foreground =
      "^ThreadPoolSingleThread" + maybe_shared + "Foreground\\d+$";
  const std::string background_blocking =
      "^ThreadPoolSingleThread" + maybe_shared + "BackgroundBlocking\\d+$";
  const std::string foreground_blocking =
      "^ThreadPoolSingleThread" + maybe_shared + "ForegroundBlocking\\d+$";

  const struct {
    TaskTraits traits;
    std::string expected_thread_name;
  } test_cases[] = {
      // Non-MayBlock()
      {{ThreadPool(), TaskPriority::BEST_EFFORT},
       CanUseBackgroundPriorityForWorkerThread() ? background : foreground},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::PREFER_BACKGROUND},
       CanUseBackgroundPriorityForWorkerThread() ? background : foreground},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::MUST_USE_FOREGROUND},
       foreground},
      {{ThreadPool(), TaskPriority::USER_VISIBLE}, foreground},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::PREFER_BACKGROUND},
       foreground},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::MUST_USE_FOREGROUND},
       foreground},
      {{ThreadPool(), TaskPriority::USER_BLOCKING}, foreground},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::PREFER_BACKGROUND},
       foreground},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::MUST_USE_FOREGROUND},
       foreground},

      // MayBlock()
      {{ThreadPool(), TaskPriority::BEST_EFFORT, MayBlock()},
       CanUseBackgroundPriorityForWorkerThread() ? background_blocking
                                                 : foreground_blocking},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::PREFER_BACKGROUND, MayBlock()},
       CanUseBackgroundPriorityForWorkerThread() ? background_blocking
                                                 : foreground_blocking},
      {{ThreadPool(), TaskPriority::BEST_EFFORT,
        ThreadPolicy::MUST_USE_FOREGROUND, MayBlock()},
       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_VISIBLE, MayBlock()},
       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::PREFER_BACKGROUND, MayBlock()},
       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_VISIBLE,
        ThreadPolicy::MUST_USE_FOREGROUND, MayBlock()},

       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_BLOCKING, MayBlock()},
       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::PREFER_BACKGROUND, MayBlock()},
       foreground_blocking},
      {{ThreadPool(), TaskPriority::USER_BLOCKING,
        ThreadPolicy::MUST_USE_FOREGROUND, MayBlock()},
       foreground_blocking}};

  for (auto& test_case : test_cases) {
    WaitableEvent event;
    CreateTaskRunner(test_case.traits)
        ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                     EXPECT_THAT(PlatformThread::GetName(),
                                 ::testing::MatchesRegex(
                                     test_case.expected_thread_name));
                     event.Signal();
                   }));
    event.Wait();
  }
}

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, PostTaskAfterShutdown) {
  auto task_runner = CreateTaskRunner();
  test::ShutdownTaskTracker(&task_tracker_);
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&ShouldNotRun)));
}

// Verify that a Task runs shortly after its delay expires.
TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, PostDelayedTask) {
  TimeTicks start_time = TimeTicks::Now();

  WaitableEvent task_ran(WaitableEvent::ResetPolicy::AUTOMATIC,
                         WaitableEvent::InitialState::NOT_SIGNALED);
  auto task_runner = CreateTaskRunner();

  // Wait until the task runner is up and running to make sure the test below is
  // solely timing the delayed task, not bringing up a physical thread.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
  ASSERT_TRUE(!task_ran.IsSignaled());

  // Post a task with a short delay.
  EXPECT_TRUE(task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)),
      TestTimeouts::tiny_timeout()));

  // Wait until the task runs.
  task_ran.Wait();

  // Expect the task to run after its delay expires, but no more than 250 ms
  // after that.
  const TimeDelta actual_delay = TimeTicks::Now() - start_time;
  EXPECT_GE(actual_delay, TestTimeouts::tiny_timeout());
  EXPECT_LT(actual_delay,
            TimeDelta::FromMilliseconds(250) + TestTimeouts::tiny_timeout());
}

// Verify that posting tasks after the single-thread manager is destroyed fails
// but doesn't crash.
TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, PostTaskAfterDestroy) {
  auto task_runner = CreateTaskRunner();
  EXPECT_TRUE(task_runner->PostTask(FROM_HERE, DoNothing()));
  test::ShutdownTaskTracker(&task_tracker_);
  TearDownSingleThreadTaskRunnerManager();
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&ShouldNotRun)));
}

// Verify that tasks only run when allowed by the CanRunPolicy.
TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, CanRunPolicyBasic) {
  test::TestCanRunPolicyBasic(
      single_thread_task_runner_manager_.get(),
      [this](TaskPriority priority) {
        return CreateTaskRunner({ThreadPool(), priority});
      },
      &task_tracker_);
}

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest,
       CanRunPolicyUpdatedBeforeRun) {
  test::TestCanRunPolicyChangedBeforeRun(
      single_thread_task_runner_manager_.get(),
      [this](TaskPriority priority) {
        return CreateTaskRunner({ThreadPool(), priority});
      },
      &task_tracker_);
}

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, CanRunPolicyLoad) {
  test::TestCanRunPolicyLoad(
      single_thread_task_runner_manager_.get(),
      [this](TaskPriority priority) {
        return CreateTaskRunner({ThreadPool(), priority});
      },
      &task_tracker_);
}

INSTANTIATE_TEST_SUITE_P(
    SharedAndDedicated,
    PooledSingleThreadTaskRunnerManagerCommonTest,
    ::testing::Values(SingleThreadTaskRunnerThreadMode::SHARED,
                      SingleThreadTaskRunnerThreadMode::DEDICATED));

namespace {

class CallJoinFromDifferentThread : public SimpleThread {
 public:
  CallJoinFromDifferentThread(
      PooledSingleThreadTaskRunnerManager* manager_to_join)
      : SimpleThread("PooledSingleThreadTaskRunnerManagerJoinThread"),
        manager_to_join_(manager_to_join) {}

  ~CallJoinFromDifferentThread() override = default;

  void Run() override {
    run_started_event_.Signal();
    manager_to_join_->JoinForTesting();
  }

  void WaitForRunToStart() { run_started_event_.Wait(); }

 private:
  PooledSingleThreadTaskRunnerManager* const manager_to_join_;
  WaitableEvent run_started_event_;

  DISALLOW_COPY_AND_ASSIGN(CallJoinFromDifferentThread);
};

class PooledSingleThreadTaskRunnerManagerJoinTest
    : public PooledSingleThreadTaskRunnerManagerTest {
 public:
  PooledSingleThreadTaskRunnerManagerJoinTest() = default;
  ~PooledSingleThreadTaskRunnerManagerJoinTest() override = default;

 protected:
  void TearDownSingleThreadTaskRunnerManager() override {
    // The tests themselves are responsible for calling JoinForTesting().
    single_thread_task_runner_manager_.reset();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PooledSingleThreadTaskRunnerManagerJoinTest);
};

}  // namespace

TEST_F(PooledSingleThreadTaskRunnerManagerJoinTest, ConcurrentJoin) {
  // Exercises the codepath where the workers are unavailable for unregistration
  // because of a Join call.
  WaitableEvent task_running;
  WaitableEvent task_blocking;

  {
    auto task_runner =
        single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
            {ThreadPool(), WithBaseSyncPrimitives()},
            SingleThreadTaskRunnerThreadMode::DEDICATED);
    EXPECT_TRUE(task_runner->PostTask(
        FROM_HERE,
        BindOnce(&WaitableEvent::Signal, Unretained(&task_running))));
    EXPECT_TRUE(task_runner->PostTask(
        FROM_HERE, BindOnce(&WaitableEvent::Wait, Unretained(&task_blocking))));
  }

  task_running.Wait();
  CallJoinFromDifferentThread join_from_different_thread(
      single_thread_task_runner_manager_.get());
  join_from_different_thread.Start();
  join_from_different_thread.WaitForRunToStart();
  task_blocking.Signal();
  join_from_different_thread.Join();
}

TEST_F(PooledSingleThreadTaskRunnerManagerJoinTest,
       ConcurrentJoinExtraSkippedTask) {
  // Tests to make sure that tasks are properly cleaned up at Join, allowing
  // SingleThreadTaskRunners to unregister themselves.
  WaitableEvent task_running;
  WaitableEvent task_blocking;

  {
    auto task_runner =
        single_thread_task_runner_manager_->CreateSingleThreadTaskRunner(
            {ThreadPool(), WithBaseSyncPrimitives()},
            SingleThreadTaskRunnerThreadMode::DEDICATED);
    EXPECT_TRUE(task_runner->PostTask(
        FROM_HERE,
        BindOnce(&WaitableEvent::Signal, Unretained(&task_running))));
    EXPECT_TRUE(task_runner->PostTask(
        FROM_HERE, BindOnce(&WaitableEvent::Wait, Unretained(&task_blocking))));
    EXPECT_TRUE(task_runner->PostTask(FROM_HERE, DoNothing()));
  }

  task_running.Wait();
  CallJoinFromDifferentThread join_from_different_thread(
      single_thread_task_runner_manager_.get());
  join_from_different_thread.Start();
  join_from_different_thread.WaitForRunToStart();
  task_blocking.Signal();
  join_from_different_thread.Join();
}

#if defined(OS_WIN)

TEST_P(PooledSingleThreadTaskRunnerManagerCommonTest, COMSTAInitialized) {
  scoped_refptr<SingleThreadTaskRunner> com_task_runner =
      single_thread_task_runner_manager_->CreateCOMSTATaskRunner(
          {TaskShutdownBehavior::BLOCK_SHUTDOWN}, GetParam());

  com_task_runner->PostTask(FROM_HERE, BindOnce(&win::AssertComApartmentType,
                                                win::ComApartmentType::STA));

  test::ShutdownTaskTracker(&task_tracker_);
}

TEST_F(PooledSingleThreadTaskRunnerManagerTest, COMSTASameThreadUsed) {
  scoped_refptr<SingleThreadTaskRunner> task_runner_1 =
      single_thread_task_runner_manager_->CreateCOMSTATaskRunner(
          {TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED);
  scoped_refptr<SingleThreadTaskRunner> task_runner_2 =
      single_thread_task_runner_manager_->CreateCOMSTATaskRunner(
          {TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::SHARED);

  PlatformThreadRef thread_ref_1;
  task_runner_1->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_1));
  PlatformThreadRef thread_ref_2;
  task_runner_2->PostTask(FROM_HERE,
                          BindOnce(&CaptureThreadRef, &thread_ref_2));

  test::ShutdownTaskTracker(&task_tracker_);

  ASSERT_FALSE(thread_ref_1.is_null());
  ASSERT_FALSE(thread_ref_2.is_null());
  EXPECT_EQ(thread_ref_1, thread_ref_2);
}

namespace {

const wchar_t* const kTestWindowClassName =
    L"PooledSingleThreadTaskRunnerManagerTestWinMessageWindow";

class PooledSingleThreadTaskRunnerManagerTestWin
    : public PooledSingleThreadTaskRunnerManagerTest {
 public:
  PooledSingleThreadTaskRunnerManagerTestWin() = default;

  void SetUp() override {
    PooledSingleThreadTaskRunnerManagerTest::SetUp();
    register_class_succeeded_ = RegisterTestWindowClass();
    ASSERT_TRUE(register_class_succeeded_);
  }

  void TearDown() override {
    if (register_class_succeeded_)
      ::UnregisterClass(kTestWindowClassName, CURRENT_MODULE());

    PooledSingleThreadTaskRunnerManagerTest::TearDown();
  }

  HWND CreateTestWindow() {
    return CreateWindow(kTestWindowClassName, kTestWindowClassName, 0, 0, 0, 0,
                        0, HWND_MESSAGE, nullptr, CURRENT_MODULE(), nullptr);
  }

 private:
  bool RegisterTestWindowClass() {
    WNDCLASSEX window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &::DefWindowProc;
    window_class.hInstance = CURRENT_MODULE();
    window_class.lpszClassName = kTestWindowClassName;
    return !!::RegisterClassEx(&window_class);
  }

  bool register_class_succeeded_ = false;

  DISALLOW_COPY_AND_ASSIGN(PooledSingleThreadTaskRunnerManagerTestWin);
};

}  // namespace

TEST_F(PooledSingleThreadTaskRunnerManagerTestWin, PumpsMessages) {
  scoped_refptr<SingleThreadTaskRunner> com_task_runner =
      single_thread_task_runner_manager_->CreateCOMSTATaskRunner(
          {TaskShutdownBehavior::BLOCK_SHUTDOWN},
          SingleThreadTaskRunnerThreadMode::DEDICATED);
  HWND hwnd = nullptr;
  // HWNDs process messages on the thread that created them, so we have to
  // create them within the context of the task runner to properly simulate a
  // COM callback.
  com_task_runner->PostTask(
      FROM_HERE,
      BindOnce([](PooledSingleThreadTaskRunnerManagerTestWin* test_harness,
                  HWND* hwnd) { *hwnd = test_harness->CreateTestWindow(); },
               Unretained(this), &hwnd));

  task_tracker_.FlushForTesting();

  ASSERT_NE(hwnd, nullptr);
  // If the message pump isn't running, we will hang here. This simulates how
  // COM would receive a callback with its own message HWND.
  SendMessage(hwnd, WM_USER, 0, 0);

  com_task_runner->PostTask(
      FROM_HERE, BindOnce([](HWND hwnd) { ::DestroyWindow(hwnd); }, hwnd));

  test::ShutdownTaskTracker(&task_tracker_);
}

#endif  // defined(OS_WIN)

namespace {

class PooledSingleThreadTaskRunnerManagerStartTest
    : public PooledSingleThreadTaskRunnerManagerTest {
 public:
  PooledSingleThreadTaskRunnerManagerStartTest() = default;

 private:
  void StartSingleThreadTaskRunnerManagerFromSetUp() override {
    // Start() is called in the test body rather than in SetUp().
  }

  DISALLOW_COPY_AND_ASSIGN(PooledSingleThreadTaskRunnerManagerStartTest);
};

}  // namespace

// Verify that a task posted before Start() doesn't run until Start() is called.
TEST_F(PooledSingleThreadTaskRunnerManagerStartTest, PostTaskBeforeStart) {
  AtomicFlag manager_started;
  WaitableEvent task_finished;
  single_thread_task_runner_manager_
      ->CreateSingleThreadTaskRunner(
          {ThreadPool()}, SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(
          FROM_HERE,
          BindOnce(
              [](WaitableEvent* task_finished, AtomicFlag* manager_started) {
                // The task should not run before Start().
                EXPECT_TRUE(manager_started->IsSet());
                task_finished->Signal();
              },
              Unretained(&task_finished), Unretained(&manager_started)));

  // Wait a little bit to make sure that the task doesn't run before start.
  // Note: This test won't catch a case where the task runs between setting
  // |manager_started| and calling Start(). However, we expect the test to be
  // flaky if the tested code allows that to happen.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  manager_started.Set();
  single_thread_task_runner_manager_->Start();

  // Wait for the task to complete to keep |manager_started| alive.
  task_finished.Wait();
}

}  // namespace internal
}  // namespace base
