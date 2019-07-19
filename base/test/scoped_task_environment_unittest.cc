// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include <atomic>
#include <memory>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/cancelable_callback.h"
#include "base/debug/debugger.h"
#include "base/run_loop.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/sequence_manager/time_domain.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/mock_log.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/tick_clock.h"
#include "base/win/com_init_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/files/file_descriptor_watcher_posix.h"
#endif  // defined(OS_POSIX)

namespace base {
namespace test {

namespace {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Return;

class ScopedTaskEnvironmentTest
    : public testing::TestWithParam<ScopedTaskEnvironment::MainThreadType> {};

void VerifyRunUntilIdleDidNotReturnAndSetFlag(
    AtomicFlag* run_until_idle_returned,
    AtomicFlag* task_ran) {
  EXPECT_FALSE(run_until_idle_returned->IsSet());
  task_ran->Set();
}

void RunUntilIdleTest(
    ScopedTaskEnvironment::MainThreadType main_thread_type,
    ScopedTaskEnvironment::ThreadPoolExecutionMode thread_pool_execution_mode) {
  AtomicFlag run_until_idle_returned;
  ScopedTaskEnvironment scoped_task_environment(main_thread_type,
                                                thread_pool_execution_mode);

  AtomicFlag first_main_thread_task_ran;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                          Unretained(&run_until_idle_returned),
                          Unretained(&first_main_thread_task_ran)));

  AtomicFlag first_thread_pool_task_ran;
  PostTask(FROM_HERE, BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                               Unretained(&run_until_idle_returned),
                               Unretained(&first_thread_pool_task_ran)));

  AtomicFlag second_thread_pool_task_ran;
  AtomicFlag second_main_thread_task_ran;
  PostTaskAndReply(FROM_HERE,
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_thread_pool_task_ran)),
                   BindOnce(&VerifyRunUntilIdleDidNotReturnAndSetFlag,
                            Unretained(&run_until_idle_returned),
                            Unretained(&second_main_thread_task_ran)));

  scoped_task_environment.RunUntilIdle();
  run_until_idle_returned.Set();

  EXPECT_TRUE(first_main_thread_task_ran.IsSet());
  EXPECT_TRUE(first_thread_pool_task_ran.IsSet());
  EXPECT_TRUE(second_thread_pool_task_ran.IsSet());
  EXPECT_TRUE(second_main_thread_task_ran.IsSet());
}

}  // namespace

TEST_P(ScopedTaskEnvironmentTest, QueuedRunUntilIdle) {
  RunUntilIdleTest(GetParam(),
                   ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);
}

TEST_P(ScopedTaskEnvironmentTest, AsyncRunUntilIdle) {
  RunUntilIdleTest(GetParam(),
                   ScopedTaskEnvironment::ThreadPoolExecutionMode::ASYNC);
}

// Verify that tasks posted to an ThreadPoolExecutionMode::QUEUED
// ScopedTaskEnvironment do not run outside of RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, QueuedTasksDoNotRunOutsideOfRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  AtomicFlag run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* run_until_idle_called) {
                            EXPECT_TRUE(run_until_idle_called->IsSet());
                          },
                          Unretained(&run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();

  AtomicFlag other_run_until_idle_called;
  PostTask(FROM_HERE, BindOnce(
                          [](AtomicFlag* other_run_until_idle_called) {
                            EXPECT_TRUE(other_run_until_idle_called->IsSet());
                          },
                          Unretained(&other_run_until_idle_called)));
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  other_run_until_idle_called.Set();
  scoped_task_environment.RunUntilIdle();
}

// Verify that a task posted to an ThreadPoolExecutionMode::ASYNC
// ScopedTaskEnvironment can run without a call to RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest, AsyncTasksRunAsTheyArePosted) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  WaitableEvent task_ran;
  PostTask(FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that a task posted to an ThreadPoolExecutionMode::ASYNC
// ScopedTaskEnvironment after a call to RunUntilIdle() can run without another
// call to RunUntilIdle().
TEST_P(ScopedTaskEnvironmentTest,
       AsyncTasksRunAsTheyArePostedAfterRunUntilIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  scoped_task_environment.RunUntilIdle();

  WaitableEvent task_ran;
  PostTask(FROM_HERE, BindOnce(&WaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(ScopedTaskEnvironmentTest, DelayedTasks) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  subtle::Atomic32 counter = 0;

  constexpr base::TimeDelta kShortTaskDelay = TimeDelta::FromDays(1);
  // Should run only in MOCK_TIME environment when time is fast-forwarded.
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 4);
          },
          Unretained(&counter)),
      kShortTaskDelay);
  PostDelayedTask(FROM_HERE,
                  BindOnce(
                      [](subtle::Atomic32* counter) {
                        subtle::NoBarrier_AtomicIncrement(counter, 128);
                      },
                      Unretained(&counter)),
                  kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = TimeDelta::FromDays(7);
  // Same as first task, longer delays to exercise
  // FastForwardUntilNoTasksRemain().
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 8);
          },
          Unretained(&counter)),
      TimeDelta::FromDays(5));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 16);
          },
          Unretained(&counter)),
      kLongTaskDelay);
  PostDelayedTask(FROM_HERE,
                  BindOnce(
                      [](subtle::Atomic32* counter) {
                        subtle::NoBarrier_AtomicIncrement(counter, 256);
                      },
                      Unretained(&counter)),
                  kLongTaskDelay * 2);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          [](subtle::Atomic32* counter) {
            subtle::NoBarrier_AtomicIncrement(counter, 512);
          },
          Unretained(&counter)),
      kLongTaskDelay * 3);
  PostDelayedTask(FROM_HERE,
                  BindOnce(
                      [](subtle::Atomic32* counter) {
                        subtle::NoBarrier_AtomicIncrement(counter, 1024);
                      },
                      Unretained(&counter)),
                  kLongTaskDelay * 4);

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindOnce(
                     [](subtle::Atomic32* counter) {
                       subtle::NoBarrier_AtomicIncrement(counter, 1);
                     },
                     Unretained(&counter)));
  PostTask(FROM_HERE, BindOnce(
                          [](subtle::Atomic32* counter) {
                            subtle::NoBarrier_AtomicIncrement(counter, 2);
                          },
                          Unretained(&counter)));

  // This expectation will fail flakily if the preceding PostTask() is executed
  // asynchronously, indicating a problem with the QUEUED execution mode.
  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);

  // RunUntilIdle() should process non-delayed tasks only in all queues.
  scoped_task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, counter);

  if (GetParam() == ScopedTaskEnvironment::MainThreadType::MOCK_TIME) {
    const TimeTicks start_time = scoped_task_environment.NowTicks();

    // Delay inferior to the delay of the first posted task.
    constexpr base::TimeDelta kInferiorTaskDelay = TimeDelta::FromSeconds(1);
    static_assert(kInferiorTaskDelay < kShortTaskDelay,
                  "|kInferiorTaskDelay| should be "
                  "set to a value inferior to the first posted task's delay.");
    scoped_task_environment.FastForwardBy(kInferiorTaskDelay);
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardBy(kShortTaskDelay - kInferiorTaskDelay);
    expected_value += 4;
    expected_value += 128;
    EXPECT_EQ(expected_value, counter);

    scoped_task_environment.FastForwardUntilNoTasksRemain();
    expected_value += 8;
    expected_value += 16;
    expected_value += 256;
    expected_value += 512;
    expected_value += 1024;
    EXPECT_EQ(expected_value, counter);

    EXPECT_EQ(scoped_task_environment.NowTicks() - start_time,
              kLongTaskDelay * 4);
  }
}

// Regression test for https://crbug.com/824770.
TEST_P(ScopedTaskEnvironmentTest, SupportsSequenceLocalStorageOnMainThread) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::ASYNC);

  SequenceLocalStorageSlot<int> sls_slot;
  sls_slot.emplace(5);
  EXPECT_EQ(5, *sls_slot);
}

TEST_P(ScopedTaskEnvironmentTest, SingleThreadShouldNotInitializeThreadPool) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::ThreadingMode::MAIN_THREAD_ONLY);
  EXPECT_THAT(ThreadPoolInstance::Get(), IsNull());
}

#if defined(OS_POSIX)
TEST_F(ScopedTaskEnvironmentTest, SupportsFileDescriptorWatcherOnIOMainThread) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::IO);

  int pipe_fds_[2];
  ASSERT_EQ(0, pipe(pipe_fds_));

  RunLoop run_loop;

  // The write end of a newly created pipe is immediately writable.
  auto controller = FileDescriptorWatcher::WatchWritable(
      pipe_fds_[1], run_loop.QuitClosure());

  // This will hang if the notification doesn't occur as expected.
  run_loop.Run();
}

TEST_F(ScopedTaskEnvironmentTest,
       SupportsFileDescriptorWatcherOnIOMockTimeMainThread) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::IO_MOCK_TIME);

  int pipe_fds_[2];
  ASSERT_EQ(0, pipe(pipe_fds_));

  RunLoop run_loop;

  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        int64_t x = 1;
        auto ret = write(pipe_fds_[1], &x, sizeof(x));
        ASSERT_EQ(static_cast<size_t>(ret), sizeof(x));
      }),
      TimeDelta::FromHours(1));

  auto controller = FileDescriptorWatcher::WatchReadable(
      pipe_fds_[0], run_loop.QuitClosure());

  // This will hang if the notification doesn't occur as expected (Run() should
  // fast-forward-time when idle).
  run_loop.Run();
}
#endif  // defined(OS_POSIX)

// Verify that the TickClock returned by
// |ScopedTaskEnvironment::GetMockTickClock| gets updated when the
// FastForward(By|UntilNoTasksRemain) functions are called.
TEST_F(ScopedTaskEnvironmentTest, FastForwardAdvanceTickClock) {
  // Use a QUEUED execution-mode environment, so that no tasks are actually
  // executed until RunUntilIdle()/FastForwardBy() are invoked.
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  constexpr base::TimeDelta kShortTaskDelay = TimeDelta::FromDays(1);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::DoNothing(),
                                                 kShortTaskDelay);

  constexpr base::TimeDelta kLongTaskDelay = TimeDelta::FromDays(7);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, base::DoNothing(),
                                                 kLongTaskDelay);

  const base::TickClock* tick_clock =
      scoped_task_environment.GetMockTickClock();
  base::TimeTicks tick_clock_ref = tick_clock->NowTicks();

  // Make sure that |FastForwardBy| advances the clock.
  scoped_task_environment.FastForwardBy(kShortTaskDelay);
  EXPECT_EQ(kShortTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Make sure that |FastForwardUntilNoTasksRemain| advances the clock.
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(kLongTaskDelay, tick_clock->NowTicks() - tick_clock_ref);

  // Fast-forwarding to a time at which there's no tasks should also advance the
  // clock.
  scoped_task_environment.FastForwardBy(kLongTaskDelay);
  EXPECT_EQ(kLongTaskDelay * 2, tick_clock->NowTicks() - tick_clock_ref);
}

TEST_F(ScopedTaskEnvironmentTest, FastForwardAdvanceMockClock) {
  constexpr base::TimeDelta kDelay = TimeDelta::FromSeconds(42);
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  const Clock* clock = scoped_task_environment.GetMockClock();
  const Time start_time = clock->Now();
  scoped_task_environment.FastForwardBy(kDelay);

  EXPECT_EQ(start_time + kDelay, clock->Now());
}

TEST_F(ScopedTaskEnvironmentTest, FastForwardAdvanceTime) {
  constexpr base::TimeDelta kDelay = TimeDelta::FromSeconds(42);
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);

  const Time start_time = base::Time::Now();
  scoped_task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(start_time + kDelay, base::Time::Now());
}

TEST_F(ScopedTaskEnvironmentTest, FastForwardAdvanceTimeTicks) {
  constexpr base::TimeDelta kDelay = TimeDelta::FromSeconds(42);
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);

  const TimeTicks start_time = base::TimeTicks::Now();
  scoped_task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
}

// Verify that FastForwardBy() runs existing immediate tasks before advancing,
// then advances to the next delayed task, runs it, then advances the remainder
// of time when out of tasks.
TEST_F(ScopedTaskEnvironmentTest, FastForwardOnlyAdvancesWhenIdle) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);

  const TimeTicks start_time = base::TimeTicks::Now();

  constexpr base::TimeDelta kDelay = TimeDelta::FromSeconds(42);
  constexpr base::TimeDelta kFastForwardUntil = TimeDelta::FromSeconds(100);
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting(
                     [&]() { EXPECT_EQ(start_time, base::TimeTicks::Now()); }));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_EQ(start_time + kDelay, base::TimeTicks::Now());
      }),
      kDelay);
  scoped_task_environment.FastForwardBy(kFastForwardUntil);
  EXPECT_EQ(start_time + kFastForwardUntil, base::TimeTicks::Now());
}

// FastForwardBy(0) should be equivalent of RunUntilIdle().
TEST_F(ScopedTaskEnvironmentTest, FastForwardZero) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  std::atomic_int run_count{0};

  for (int i = 0; i < 1000; ++i) {
    ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          run_count.fetch_add(1, std::memory_order_relaxed);
        }));
    base::PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                     run_count.fetch_add(1, std::memory_order_relaxed);
                   }));
  }

  scoped_task_environment.FastForwardBy(base::TimeDelta());

  EXPECT_EQ(2000, run_count.load(std::memory_order_relaxed));
}

TEST_F(ScopedTaskEnvironmentTest, NestedFastForwardBy) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  constexpr TimeDelta kDelayPerTask = TimeDelta::FromMilliseconds(1);
  const TimeTicks start_time = scoped_task_environment.NowTicks();

  int max_nesting_level = 0;

  RepeatingClosure post_fast_forwarding_task;
  post_fast_forwarding_task = BindLambdaForTesting([&]() {
    if (max_nesting_level < 5) {
      ++max_nesting_level;
      ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_fast_forwarding_task, kDelayPerTask);
      scoped_task_environment.FastForwardBy(kDelayPerTask);
    }
  });
  post_fast_forwarding_task.Run();

  EXPECT_EQ(max_nesting_level, 5);
  EXPECT_EQ(scoped_task_environment.NowTicks(), start_time + kDelayPerTask * 5);
}

TEST_F(ScopedTaskEnvironmentTest, NestedRunInFastForwardBy) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  constexpr TimeDelta kDelayPerTask = TimeDelta::FromMilliseconds(1);
  const TimeTicks start_time = scoped_task_environment.NowTicks();

  std::vector<RunLoop*> run_loops;

  RepeatingClosure post_and_runloop_task;
  post_and_runloop_task = BindLambdaForTesting([&]() {
    // Run 4 nested run loops on top of the initial FastForwardBy().
    if (run_loops.size() < 4U) {
      ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_and_runloop_task, kDelayPerTask);

      RunLoop run_loop(RunLoop::Type::kNestableTasksAllowed);
      run_loops.push_back(&run_loop);
      run_loop.Run();
    } else {
      for (RunLoop* run_loop : run_loops) {
        run_loop->Quit();
      }
    }
  });

  // Initial task is FastForwardBy().
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, post_and_runloop_task, kDelayPerTask);
  scoped_task_environment.FastForwardBy(kDelayPerTask);

  EXPECT_EQ(run_loops.size(), 4U);
  EXPECT_EQ(scoped_task_environment.NowTicks(), start_time + kDelayPerTask * 5);
}

TEST_F(ScopedTaskEnvironmentTest,
       CrossThreadImmediateTaskPostingDoesntAffectMockTime) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);

  int count = 0;

  // Post tasks delayd between 0 and 999 seconds.
  for (int i = 0; i < 1000; ++i) {
    const TimeDelta delay = TimeDelta::FromSeconds(i);
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        BindOnce(
            [](TimeTicks expected_run_time, int* count) {
              EXPECT_EQ(expected_run_time, TimeTicks::Now());
              ++*count;
            },
            TimeTicks::Now() + delay, &count),
        delay);
  }

  // Having a bunch of tasks running in parallel and replying to the main thread
  // shouldn't affect the rest of this test. Wait for the first task to run
  // before proceeding with the test to increase the likelihood of exercising
  // races.
  base::WaitableEvent first_reply_is_incoming;
  for (int i = 0; i < 1000; ++i) {
    base::PostTaskAndReply(
        FROM_HERE,
        BindOnce(&WaitableEvent::Signal, Unretained(&first_reply_is_incoming)),
        DoNothing());
  }
  first_reply_is_incoming.Wait();

  scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(1000));

  // If this test flakes it's because there's an error with MockTimeDomain.
  EXPECT_EQ(count, 1000);

  // Flush any remaining asynchronous tasks with Unretained() state.
  scoped_task_environment.RunUntilIdle();
}

TEST_F(ScopedTaskEnvironmentTest, MultiThreadedMockTime) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  constexpr TimeDelta kOneMs = TimeDelta::FromMilliseconds(1);
  const TimeTicks start_time = scoped_task_environment.NowTicks();
  const TimeTicks end_time = start_time + TimeDelta::FromMilliseconds(1'000);

  // Last TimeTicks::Now() seen from either contexts.
  TimeTicks last_main_thread_ticks = start_time;
  TimeTicks last_thread_pool_ticks = start_time;

  RepeatingClosure post_main_thread_delayed_task;
  post_main_thread_delayed_task = BindLambdaForTesting([&]() {
    // Expect that time only moves forward.
    EXPECT_GE(scoped_task_environment.NowTicks(), last_main_thread_ticks);

    // Post four tasks to exercise the system some more but only if this is the
    // first task at its runtime (otherwise we end up with 4^10'000 tasks by
    // the end!).
    if (last_main_thread_ticks < scoped_task_environment.NowTicks() &&
        scoped_task_environment.NowTicks() < end_time) {
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_main_thread_delayed_task, kOneMs);
    }

    last_main_thread_ticks = scoped_task_environment.NowTicks();
  });

  RepeatingClosure post_thread_pool_delayed_task;
  post_thread_pool_delayed_task = BindLambdaForTesting([&]() {
    // Expect that time only moves forward.
    EXPECT_GE(scoped_task_environment.NowTicks(), last_thread_pool_ticks);

    // Post four tasks to exercise the system some more but only if this is the
    // first task at its runtime (otherwise we end up with 4^10'000 tasks by
    // the end!).
    if (last_thread_pool_ticks < scoped_task_environment.NowTicks() &&
        scoped_task_environment.NowTicks() < end_time) {
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);
      SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, post_thread_pool_delayed_task, kOneMs);

      EXPECT_LT(scoped_task_environment.NowTicks(), end_time);
    }

    last_thread_pool_ticks = scoped_task_environment.NowTicks();
  });

  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, post_main_thread_delayed_task, kOneMs);
  CreateSequencedTaskRunnerWithTraits({ThreadPool()})
      ->PostDelayedTask(FROM_HERE, post_thread_pool_delayed_task, kOneMs);

  scoped_task_environment.FastForwardUntilNoTasksRemain();

  EXPECT_EQ(last_main_thread_ticks, end_time);
  EXPECT_EQ(last_thread_pool_ticks, end_time);
  EXPECT_EQ(scoped_task_environment.NowTicks(), end_time);
}

// This test ensures the implementation of FastForwardBy() doesn't fast-forward
// beyond the cap it reaches idle with pending delayed tasks further ahead on
// the main thread.
TEST_F(ScopedTaskEnvironmentTest, MultiThreadedFastForwardBy) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME);

  const TimeTicks start_time = scoped_task_environment.NowTicks();

  // The 1s delayed task in the pool should run but not the 5s delayed task on
  // the main thread and fast-forward by should be capped at +2s.
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, MakeExpectedNotRunClosure(FROM_HERE),
      TimeDelta::FromSeconds(5));
  PostDelayedTask(FROM_HERE, {ThreadPool()}, MakeExpectedRunClosure(FROM_HERE),
                  TimeDelta::FromSeconds(1));
  scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(2));

  EXPECT_EQ(scoped_task_environment.NowTicks(),
            start_time + TimeDelta::FromSeconds(2));
}

// Verify that ThreadPoolExecutionMode::QUEUED doesn't prevent running tasks and
// advancing time on the main thread.
TEST_F(ScopedTaskEnvironmentTest,
       MultiThreadedMockTimeAndThreadPoolQueuedMode) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
      ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  int count = 0;
  const TimeTicks start_time = scoped_task_environment.NowTicks();

  RunLoop run_loop;

  // Neither of these should run automatically per
  // ThreadPoolExecutionMode::QUEUED.
  PostTask(FROM_HERE, {ThreadPool()},
           BindLambdaForTesting([&]() { count += 128; }));
  PostDelayedTask(FROM_HERE, {ThreadPool()},
                  BindLambdaForTesting([&]() { count += 256; }),
                  TimeDelta::FromSeconds(5));

  // Time should auto-advance to +500s in RunLoop::Run() without having to run
  // the above forcefully QUEUED tasks.
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { count += 1; }));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE,
                                                 BindLambdaForTesting([&]() {
                                                   count += 2;
                                                   run_loop.Quit();
                                                 }),
                                                 TimeDelta::FromSeconds(500));

  int expected_value = 0;
  EXPECT_EQ(expected_value, count);
  run_loop.Run();
  expected_value += 1;
  expected_value += 2;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(scoped_task_environment.NowTicks() - start_time,
            TimeDelta::FromSeconds(500));

  // Fast-forward through all remaining tasks, this should unblock QUEUED tasks
  // in the thread pool but shouldn't need to advance time to process them.
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  expected_value += 128;
  expected_value += 256;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(scoped_task_environment.NowTicks() - start_time,
            TimeDelta::FromSeconds(500));

  // Test advancing time to a QUEUED task in the future.
  PostDelayedTask(FROM_HERE, {ThreadPool()},
                  BindLambdaForTesting([&]() { count += 512; }),
                  TimeDelta::FromSeconds(5));
  scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(7));
  expected_value += 512;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(scoped_task_environment.NowTicks() - start_time,
            TimeDelta::FromSeconds(507));

  // Confirm that QUEUED mode is still active after the above fast forwarding
  // (only the main thread task should run from RunLoop).
  PostTask(FROM_HERE, {ThreadPool()},
           BindLambdaForTesting([&]() { count += 1024; }));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() { count += 2048; }));
  PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
  RunLoop().RunUntilIdle();
  expected_value += 2048;
  EXPECT_EQ(expected_value, count);
  EXPECT_EQ(scoped_task_environment.NowTicks() - start_time,
            TimeDelta::FromSeconds(507));

  // Run the remaining task to avoid use-after-free on |count| from
  // ~ScopedTaskEnvironment().
  scoped_task_environment.RunUntilIdle();
  expected_value += 1024;
  EXPECT_EQ(expected_value, count);
}

#if defined(OS_WIN)
// Regression test to ensure that ScopedTaskEnvironment enables the MTA in the
// thread pool (so that the test environment matches that of the browser process
// and com_init_util.h's assertions are happy in unit tests).
TEST_F(ScopedTaskEnvironmentTest, ThreadPoolPoolAllowsMTA) {
  ScopedTaskEnvironment scoped_task_environment;
  PostTask(FROM_HERE,
           BindOnce(&win::AssertComApartmentType, win::ComApartmentType::MTA));
  scoped_task_environment.RunUntilIdle();
}
#endif  // defined(OS_WIN)

TEST_F(ScopedTaskEnvironmentTest, SetsDefaultRunTimeout) {
  const RunLoop::ScopedRunTimeoutForTest* old_run_timeout =
      RunLoop::ScopedRunTimeoutForTest::Current();

  {
    ScopedTaskEnvironment scoped_task_environment;

    // ScopedTaskEnvironment should set a default Run() timeout that fails the
    // calling test (before test_launcher_timeout()).

    const RunLoop::ScopedRunTimeoutForTest* run_timeout =
        RunLoop::ScopedRunTimeoutForTest::Current();
    EXPECT_NE(run_timeout, old_run_timeout);
    EXPECT_TRUE(run_timeout);
    if (!debug::BeingDebugged()) {
      EXPECT_LT(run_timeout->timeout(), TestTimeouts::test_launcher_timeout());
    }
    EXPECT_NONFATAL_FAILURE({ run_timeout->on_timeout().Run(); },
                            "RunLoop::Run() timed out");
  }

  EXPECT_EQ(RunLoop::ScopedRunTimeoutForTest::Current(), old_run_timeout);
}

namespace {}

TEST_F(ScopedTaskEnvironmentTest, DescribePendingMainThreadTasks) {
  ScopedTaskEnvironment scoped_task_environment;
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, DoNothing());

  test::MockLog mock_log;
  mock_log.StartCapturingLogs();

  EXPECT_CALL(mock_log, Log(::logging::LOG_INFO, _, _, _,
                            HasSubstr("scoped_task_environment_unittest.cc")))
      .WillOnce(Return(true));
  scoped_task_environment.DescribePendingMainThreadTasks();

  scoped_task_environment.RunUntilIdle();

  EXPECT_CALL(mock_log,
              Log(::logging::LOG_INFO, _, _, _,
                  Not(HasSubstr("scoped_task_environment_unittest.cc"))))
      .WillOnce(Return(true));
  scoped_task_environment.DescribePendingMainThreadTasks();
}

INSTANTIATE_TEST_SUITE_P(
    MainThreadDefault,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::DEFAULT));
INSTANTIATE_TEST_SUITE_P(
    MainThreadMockTime,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::MOCK_TIME));
INSTANTIATE_TEST_SUITE_P(
    MainThreadUIMockTime,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME));
INSTANTIATE_TEST_SUITE_P(
    MainThreadUI,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::UI));
INSTANTIATE_TEST_SUITE_P(
    MainThreadIO,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::IO));
INSTANTIATE_TEST_SUITE_P(
    MainThreadIOMockTime,
    ScopedTaskEnvironmentTest,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::IO_MOCK_TIME));

class ScopedTaskEnvironmentMockedTime
    : public testing::TestWithParam<ScopedTaskEnvironment::MainThreadType> {};

TEST_P(ScopedTaskEnvironmentMockedTime, Basic) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  int counter = 0;

  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 1; }, Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 32; }, Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 256; }, Unretained(&counter)),
      TimeDelta::FromSeconds(3));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 64; }, Unretained(&counter)),
      TimeDelta::FromSeconds(1));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 1024; }, Unretained(&counter)),
      TimeDelta::FromMinutes(20));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      BindOnce([](int* counter) { *counter += 4096; }, Unretained(&counter)),
      TimeDelta::FromDays(20));

  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);
  scoped_task_environment.RunUntilIdle();
  expected_value += 1;
  expected_value += 32;
  EXPECT_EQ(expected_value, counter);

  scoped_task_environment.RunUntilIdle();
  EXPECT_EQ(expected_value, counter);

  scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(1));
  expected_value += 64;
  EXPECT_EQ(expected_value, counter);

  scoped_task_environment.FastForwardBy(TimeDelta::FromSeconds(5));
  expected_value += 256;
  EXPECT_EQ(expected_value, counter);

  scoped_task_environment.FastForwardUntilNoTasksRemain();
  expected_value += 1024;
  expected_value += 4096;
  EXPECT_EQ(expected_value, counter);
}

TEST_P(ScopedTaskEnvironmentMockedTime, RunLoopDriveable) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  int counter = 0;
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 1; },
                                Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce([](int* counter) { *counter += 32; },
                                Unretained(&counter)));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 256; },
                     Unretained(&counter)),
      TimeDelta::FromSeconds(3));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 64; },
                     Unretained(&counter)),
      TimeDelta::FromSeconds(1));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 1024; },
                     Unretained(&counter)),
      TimeDelta::FromMinutes(20));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](int* counter) { *counter += 4096; },
                     Unretained(&counter)),
      TimeDelta::FromDays(20));

  int expected_value = 0;
  EXPECT_EQ(expected_value, counter);
  RunLoop().RunUntilIdle();
  expected_value += 1;
  expected_value += 32;
  EXPECT_EQ(expected_value, counter);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(expected_value, counter);

  {
    RunLoop run_loop;
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TimeDelta::FromSeconds(1));
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 8192; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(1));

    // The QuitClosure() should be ordered between the 64 and the 8192
    // increments and should preempt the latter.
    run_loop.Run();
    expected_value += 64;
    EXPECT_EQ(expected_value, counter);

    // Running until idle should process the 8192 increment whose delay has
    // expired in the previous Run().
    RunLoop().RunUntilIdle();
    expected_value += 8192;
    EXPECT_EQ(expected_value, counter);
  }

  {
    RunLoop run_loop;
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(), TimeDelta::FromSeconds(5));
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce([](int* counter) { *counter += 16384; },
                       Unretained(&counter)),
        TimeDelta::FromSeconds(5));

    // The QuitWhenIdleClosure() shouldn't preempt equally delayed tasks and as
    // such the 16384 increment should be processed before quitting.
    run_loop.Run();
    expected_value += 256;
    expected_value += 16384;
    EXPECT_EQ(expected_value, counter);
  }

  // Process the remaining tasks (note: do not mimic this elsewhere,
  // TestMockTimeTaskRunner::FastForwardUntilNoTasksRemain() is a better API to
  // do this, this is just done here for the purpose of extensively testing the
  // RunLoop approach).

  // Disable Run() timeout here, otherwise we'll fast-forward to it before we
  // reach the quit task.
  RunLoop::ScopedDisableRunTimeoutForTest disable_timeout;

  RunLoop run_loop;
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitWhenIdleClosure(), TimeDelta::FromDays(50));

  run_loop.Run();
  expected_value += 1024;
  expected_value += 4096;
  EXPECT_EQ(expected_value, counter);
}

TEST_P(ScopedTaskEnvironmentMockedTime, CancelPendingTask) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  CancelableOnceClosure task1(BindOnce([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task1.callback(),
                                                 TimeDelta::FromSeconds(1));
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());
  EXPECT_EQ(1u, scoped_task_environment.GetPendingMainThreadTaskCount());
  EXPECT_EQ(TimeDelta::FromSeconds(1),
            scoped_task_environment.NextMainThreadPendingTaskDelay());
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());
  task1.Cancel();
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());
  EXPECT_EQ(TimeDelta::Max(),
            scoped_task_environment.NextMainThreadPendingTaskDelay());

  CancelableClosure task2(BindRepeating([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task2.callback(),
                                                 TimeDelta::FromSeconds(1));
  task2.Cancel();
  EXPECT_EQ(0u, scoped_task_environment.GetPendingMainThreadTaskCount());

  CancelableClosure task3(BindRepeating([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task3.callback(),
                                                 TimeDelta::FromSeconds(1));
  task3.Cancel();
  EXPECT_EQ(TimeDelta::Max(),
            scoped_task_environment.NextMainThreadPendingTaskDelay());

  CancelableClosure task4(BindRepeating([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task4.callback(),
                                                 TimeDelta::FromSeconds(1));
  task4.Cancel();
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());
}

TEST_P(ScopedTaskEnvironmentMockedTime, CancelPendingImmediateTask) {
  ScopedTaskEnvironment scoped_task_environment(GetParam());
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());

  CancelableOnceClosure task1(BindOnce([]() {}));
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task1.callback());
  EXPECT_FALSE(scoped_task_environment.MainThreadIsIdle());

  task1.Cancel();
  EXPECT_TRUE(scoped_task_environment.MainThreadIsIdle());
}

TEST_P(ScopedTaskEnvironmentMockedTime, NoFastForwardToCancelledTask) {
  ScopedTaskEnvironment scoped_task_environment(
      GetParam(), ScopedTaskEnvironment::ThreadPoolExecutionMode::QUEUED);

  TimeTicks start_time = scoped_task_environment.NowTicks();
  CancelableClosure task(BindRepeating([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task.callback(),
                                                 TimeDelta::FromSeconds(1));
  EXPECT_EQ(TimeDelta::FromSeconds(1),
            scoped_task_environment.NextMainThreadPendingTaskDelay());
  task.Cancel();
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(start_time, scoped_task_environment.NowTicks());
}

TEST_P(ScopedTaskEnvironmentMockedTime, NextTaskIsDelayed) {
  ScopedTaskEnvironment scoped_task_environment(GetParam());

  EXPECT_FALSE(scoped_task_environment.NextTaskIsDelayed());
  CancelableClosure task(BindRepeating([]() {}));
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, task.callback(),
                                                 TimeDelta::FromSeconds(1));
  EXPECT_TRUE(scoped_task_environment.NextTaskIsDelayed());
  task.Cancel();
  EXPECT_FALSE(scoped_task_environment.NextTaskIsDelayed());

  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, BindOnce([]() {}),
                                                 TimeDelta::FromSeconds(2));
  EXPECT_TRUE(scoped_task_environment.NextTaskIsDelayed());
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(scoped_task_environment.NextTaskIsDelayed());

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, BindOnce([]() {}));
  EXPECT_FALSE(scoped_task_environment.NextTaskIsDelayed());
}

TEST_P(ScopedTaskEnvironmentMockedTime,
       NextMainThreadPendingTaskDelayWithImmediateTask) {
  ScopedTaskEnvironment scoped_task_environment(GetParam());

  EXPECT_EQ(TimeDelta::Max(),
            scoped_task_environment.NextMainThreadPendingTaskDelay());
  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, BindOnce([]() {}));
  EXPECT_EQ(TimeDelta(),
            scoped_task_environment.NextMainThreadPendingTaskDelay());
}

INSTANTIATE_TEST_SUITE_P(
    MainThreadMockTime,
    ScopedTaskEnvironmentMockedTime,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::MOCK_TIME));
INSTANTIATE_TEST_SUITE_P(
    MainThreadUIMockTime,
    ScopedTaskEnvironmentMockedTime,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::UI_MOCK_TIME));
INSTANTIATE_TEST_SUITE_P(
    MainThreadIOMockTime,
    ScopedTaskEnvironmentMockedTime,
    ::testing::Values(ScopedTaskEnvironment::MainThreadType::IO_MOCK_TIME));

TEST_F(ScopedTaskEnvironmentTest, TimeSourceMockTime) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME);

  const TimeTicks start_time = scoped_task_environment.NowTicks();

  constexpr TimeDelta kDelay = TimeDelta::FromSeconds(10);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, DoNothing(),
                                                 kDelay);
  scoped_task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(scoped_task_environment.NowTicks(), start_time + kDelay);
}

TEST_F(ScopedTaskEnvironmentTest, TimeSourceMockTimeAndNow) {
  ScopedTaskEnvironment scoped_task_environment(
      ScopedTaskEnvironment::TimeSource::MOCK_TIME_AND_NOW);

  const TimeTicks start_time = scoped_task_environment.NowTicks();
  EXPECT_EQ(TimeTicks::Now(), start_time);

  constexpr TimeDelta kDelay = TimeDelta::FromSeconds(10);
  scoped_task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(TimeTicks::Now(), start_time + kDelay);
}

}  // namespace test
}  // namespace base
