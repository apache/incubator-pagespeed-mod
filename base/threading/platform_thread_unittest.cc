// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include "base/threading/platform_thread_internal_posix.h"
#elif defined(OS_WIN)
#include <windows.h>
#include "base/threading/platform_thread_win.h"
#endif

namespace base {

// Trivial tests that thread runs and doesn't crash on create, join, or detach -

namespace {

class TrivialThread : public PlatformThread::Delegate {
 public:
  TrivialThread() : run_event_(WaitableEvent::ResetPolicy::MANUAL,
                               WaitableEvent::InitialState::NOT_SIGNALED) {}

  void ThreadMain() override { run_event_.Signal(); }

  WaitableEvent& run_event() { return run_event_; }

 private:
  WaitableEvent run_event_;

  DISALLOW_COPY_AND_ASSIGN(TrivialThread);
};

}  // namespace

TEST(PlatformThreadTest, TrivialJoin) {
  TrivialThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.run_event().IsSignaled());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  PlatformThread::Join(handle);
  ASSERT_TRUE(thread.run_event().IsSignaled());
}

TEST(PlatformThreadTest, TrivialJoinTimesTen) {
  TrivialThread thread[10];
  PlatformThreadHandle handle[base::size(thread)];

  for (auto& n : thread)
    ASSERT_FALSE(n.run_event().IsSignaled());
  for (size_t n = 0; n < base::size(thread); n++)
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
  for (auto n : handle)
    PlatformThread::Join(n);
  for (auto& n : thread)
    ASSERT_TRUE(n.run_event().IsSignaled());
}

// The following detach tests are by nature racy. The run_event approximates the
// end and termination of the thread, but threads could persist shortly after
// the test completes.
TEST(PlatformThreadTest, TrivialDetach) {
  TrivialThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.run_event().IsSignaled());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  PlatformThread::Detach(handle);
  thread.run_event().Wait();
}

TEST(PlatformThreadTest, TrivialDetachTimesTen) {
  TrivialThread thread[10];
  PlatformThreadHandle handle[base::size(thread)];

  for (auto& n : thread)
    ASSERT_FALSE(n.run_event().IsSignaled());
  for (size_t n = 0; n < base::size(thread); n++) {
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
    PlatformThread::Detach(handle[n]);
  }
  for (auto& n : thread)
    n.run_event().Wait();
}

// Tests of basic thread functions ---------------------------------------------

namespace {

class FunctionTestThread : public PlatformThread::Delegate {
 public:
  FunctionTestThread()
      : thread_id_(kInvalidThreadId),
        termination_ready_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED),
        terminate_thread_(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED),
        done_(false) {}
  ~FunctionTestThread() override {
    EXPECT_TRUE(terminate_thread_.IsSignaled())
        << "Need to mark thread for termination and join the underlying thread "
        << "before destroying a FunctionTestThread as it owns the "
        << "WaitableEvent blocking the underlying thread's main.";
  }

  // Grabs |thread_id_|, runs an optional test on that thread, signals
  // |termination_ready_|, and then waits for |terminate_thread_| to be
  // signaled before exiting.
  void ThreadMain() override {
    thread_id_ = PlatformThread::CurrentId();
    EXPECT_NE(thread_id_, kInvalidThreadId);

    // Make sure that the thread ID is the same across calls.
    EXPECT_EQ(thread_id_, PlatformThread::CurrentId());

    // Run extra tests.
    RunTest();

    termination_ready_.Signal();
    terminate_thread_.Wait();

    done_ = true;
  }

  PlatformThreadId thread_id() const {
    EXPECT_TRUE(termination_ready_.IsSignaled()) << "Thread ID still unknown";
    return thread_id_;
  }

  bool IsRunning() const {
    return termination_ready_.IsSignaled() && !done_;
  }

  // Blocks until this thread is started and ready to be terminated.
  void WaitForTerminationReady() { termination_ready_.Wait(); }

  // Marks this thread for termination (callers must then join this thread to be
  // guaranteed of termination).
  void MarkForTermination() { terminate_thread_.Signal(); }

 private:
  // Runs an optional test on the newly created thread.
  virtual void RunTest() {}

  PlatformThreadId thread_id_;

  mutable WaitableEvent termination_ready_;
  WaitableEvent terminate_thread_;
  bool done_;

  DISALLOW_COPY_AND_ASSIGN(FunctionTestThread);
};

}  // namespace

TEST(PlatformThreadTest, Function) {
  PlatformThreadId main_thread_id = PlatformThread::CurrentId();

  FunctionTestThread thread;
  PlatformThreadHandle handle;

  ASSERT_FALSE(thread.IsRunning());
  ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
  thread.WaitForTerminationReady();
  ASSERT_TRUE(thread.IsRunning());
  EXPECT_NE(thread.thread_id(), main_thread_id);

  thread.MarkForTermination();
  PlatformThread::Join(handle);
  ASSERT_FALSE(thread.IsRunning());

  // Make sure that the thread ID is the same across calls.
  EXPECT_EQ(main_thread_id, PlatformThread::CurrentId());
}

TEST(PlatformThreadTest, FunctionTimesTen) {
  PlatformThreadId main_thread_id = PlatformThread::CurrentId();

  FunctionTestThread thread[10];
  PlatformThreadHandle handle[base::size(thread)];

  for (const auto& n : thread)
    ASSERT_FALSE(n.IsRunning());

  for (size_t n = 0; n < base::size(thread); n++)
    ASSERT_TRUE(PlatformThread::Create(0, &thread[n], &handle[n]));
  for (auto& n : thread)
    n.WaitForTerminationReady();

  for (size_t n = 0; n < base::size(thread); n++) {
    ASSERT_TRUE(thread[n].IsRunning());
    EXPECT_NE(thread[n].thread_id(), main_thread_id);

    // Make sure no two threads get the same ID.
    for (size_t i = 0; i < n; ++i) {
      EXPECT_NE(thread[i].thread_id(), thread[n].thread_id());
    }
  }

  for (auto& n : thread)
    n.MarkForTermination();
  for (auto n : handle)
    PlatformThread::Join(n);
  for (const auto& n : thread)
    ASSERT_FALSE(n.IsRunning());

  // Make sure that the thread ID is the same across calls.
  EXPECT_EQ(main_thread_id, PlatformThread::CurrentId());
}

namespace {

class ThreadPriorityTestThread : public FunctionTestThread {
 public:
  explicit ThreadPriorityTestThread(ThreadPriority from, ThreadPriority to)
      : from_(from), to_(to) {}
  ~ThreadPriorityTestThread() override = default;

 private:
  void RunTest() override {
    EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(),
              ThreadPriority::NORMAL);
    PlatformThread::SetCurrentThreadPriority(from_);
    EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(), from_);
    PlatformThread::SetCurrentThreadPriority(to_);

    if (static_cast<int>(to_) <= static_cast<int>(from_) ||
        PlatformThread::CanIncreaseThreadPriority(to_)) {
      EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(), to_);
    } else {
      EXPECT_NE(PlatformThread::GetCurrentThreadPriority(), to_);
    }
  }

  const ThreadPriority from_;
  const ThreadPriority to_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPriorityTestThread);
};

void TestSetCurrentThreadPriority() {
  constexpr ThreadPriority kAllThreadPriorities[] = {
      ThreadPriority::REALTIME_AUDIO, ThreadPriority::DISPLAY,
      ThreadPriority::NORMAL, ThreadPriority::BACKGROUND};

  for (auto from : kAllThreadPriorities) {
    if (static_cast<int>(from) <= static_cast<int>(ThreadPriority::NORMAL) ||
        PlatformThread::CanIncreaseThreadPriority(from)) {
      for (auto to : kAllThreadPriorities) {
        ThreadPriorityTestThread thread(from, to);
        PlatformThreadHandle handle;

        ASSERT_FALSE(thread.IsRunning());
        ASSERT_TRUE(PlatformThread::Create(0, &thread, &handle));
        thread.WaitForTerminationReady();
        ASSERT_TRUE(thread.IsRunning());

        thread.MarkForTermination();
        PlatformThread::Join(handle);
        ASSERT_FALSE(thread.IsRunning());
      }
    }
  }
}

}  // namespace

// Test changing a created thread's priority.
#if defined(OS_FUCHSIA)
// TODO(crbug.com/851759): Thread priorities are not implemented in Fuchsia.
#define MAYBE_SetCurrentThreadPriority DISABLED_SetCurrentThreadPriority
#else
#define MAYBE_SetCurrentThreadPriority SetCurrentThreadPriority
#endif
TEST(PlatformThreadTest, MAYBE_SetCurrentThreadPriority) {
  TestSetCurrentThreadPriority();
}

#if defined(OS_WIN)
// Test changing a created thread's priority, with the
// kWindowsThreadModeBackground feature enabled.
// Flaky: https://crbug.com/931706
TEST(PlatformThreadTest,
     DISABLED_SetCurrentThreadPriorityWithThreadModeBackground) {
  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWindowsThreadModeBackground);
  TestSetCurrentThreadPriority();
}

// Test changing a created thread's priority, with the
// kWindowsThreadModeBackground feature enabled, in an IDLE_PRIORITY_CLASS
// process (regression test for https://crbug.com/901483).
// Flaky: https://crbug.com/931706
TEST(PlatformThreadTest,
     DISABLED_SetCurrentThreadPriorityWithThreadModeBackgroundIdleProcess) {
  ::SetPriorityClass(Process::Current().Handle(), IDLE_PRIORITY_CLASS);

  test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kWindowsThreadModeBackground);
  TestSetCurrentThreadPriority();

  ::SetPriorityClass(Process::Current().Handle(), NORMAL_PRIORITY_CLASS);
}
#endif  // defined(OS_WIN)

// Ideally PlatformThread::CanIncreaseThreadPriority() would be true on all
// platforms for all priorities. This not being the case. This test documents
// and hardcodes what we know. Please inform scheduler-dev@chromium.org if this
// proprerty changes for a given platform.
TEST(PlatformThreadTest, CanIncreaseThreadPriority) {
#if defined(OS_LINUX)
  // On Ubuntu, RLIMIT_NICE and RLIMIT_RTPRIO are 0 by default, so we won't be
  // able to increase priority to any level.
  constexpr bool kCanIncreasePriority = false;
#elif defined(OS_FUCHSIA)
  // Fuchsia doesn't support thread priorities.
  constexpr bool kCanIncreasePriority = false;
#else
  constexpr bool kCanIncreasePriority = true;
#endif

  EXPECT_EQ(
      PlatformThread::CanIncreaseThreadPriority(ThreadPriority::BACKGROUND),
      kCanIncreasePriority);
  EXPECT_EQ(PlatformThread::CanIncreaseThreadPriority(ThreadPriority::NORMAL),
            kCanIncreasePriority);
  EXPECT_EQ(PlatformThread::CanIncreaseThreadPriority(ThreadPriority::DISPLAY),
            kCanIncreasePriority);
  EXPECT_EQ(
      PlatformThread::CanIncreaseThreadPriority(ThreadPriority::REALTIME_AUDIO),
      kCanIncreasePriority);
}

// This tests internal PlatformThread APIs used under some POSIX platforms,
// with the exception of Mac OS X, iOS and Fuchsia.
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS) && \
    !defined(OS_FUCHSIA)
TEST(PlatformThreadTest, GetNiceValueToThreadPriority) {
  using internal::NiceValueToThreadPriority;
  using internal::kThreadPriorityToNiceValueMap;

  EXPECT_EQ(ThreadPriority::BACKGROUND,
            kThreadPriorityToNiceValueMap[0].priority);
  EXPECT_EQ(ThreadPriority::NORMAL,
            kThreadPriorityToNiceValueMap[1].priority);
  EXPECT_EQ(ThreadPriority::DISPLAY,
            kThreadPriorityToNiceValueMap[2].priority);
  EXPECT_EQ(ThreadPriority::REALTIME_AUDIO,
            kThreadPriorityToNiceValueMap[3].priority);

  static const int kBackgroundNiceValue =
      kThreadPriorityToNiceValueMap[0].nice_value;
  static const int kNormalNiceValue =
      kThreadPriorityToNiceValueMap[1].nice_value;
  static const int kDisplayNiceValue =
      kThreadPriorityToNiceValueMap[2].nice_value;
  static const int kRealtimeAudioNiceValue =
      kThreadPriorityToNiceValueMap[3].nice_value;

  // The tests below assume the nice values specified in the map are within
  // the range below (both ends exclusive).
  static const int kHighestNiceValue = 19;
  static const int kLowestNiceValue = -20;

  EXPECT_GT(kHighestNiceValue, kBackgroundNiceValue);
  EXPECT_GT(kBackgroundNiceValue, kNormalNiceValue);
  EXPECT_GT(kNormalNiceValue, kDisplayNiceValue);
  EXPECT_GT(kDisplayNiceValue, kRealtimeAudioNiceValue);
  EXPECT_GT(kRealtimeAudioNiceValue, kLowestNiceValue);

  EXPECT_EQ(ThreadPriority::BACKGROUND,
            NiceValueToThreadPriority(kHighestNiceValue));
  EXPECT_EQ(ThreadPriority::BACKGROUND,
            NiceValueToThreadPriority(kBackgroundNiceValue + 1));
  EXPECT_EQ(ThreadPriority::BACKGROUND,
            NiceValueToThreadPriority(kBackgroundNiceValue));
  EXPECT_EQ(ThreadPriority::BACKGROUND,
            NiceValueToThreadPriority(kNormalNiceValue + 1));
  EXPECT_EQ(ThreadPriority::NORMAL,
            NiceValueToThreadPriority(kNormalNiceValue));
  EXPECT_EQ(ThreadPriority::NORMAL,
            NiceValueToThreadPriority(kDisplayNiceValue + 1));
  EXPECT_EQ(ThreadPriority::DISPLAY,
            NiceValueToThreadPriority(kDisplayNiceValue));
  EXPECT_EQ(ThreadPriority::DISPLAY,
            NiceValueToThreadPriority(kRealtimeAudioNiceValue + 1));
  EXPECT_EQ(ThreadPriority::REALTIME_AUDIO,
            NiceValueToThreadPriority(kRealtimeAudioNiceValue));
  EXPECT_EQ(ThreadPriority::REALTIME_AUDIO,
            NiceValueToThreadPriority(kLowestNiceValue));
}
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS) &&
        // !defined(OS_FUCHSIA)

TEST(PlatformThreadTest, SetHugeThreadName) {
  // Construct an excessively long thread name.
  std::string long_name(1024, 'a');

  // SetName has no return code, so just verify that implementations
  // don't [D]CHECK().
  PlatformThread::SetName(long_name);
}

TEST(PlatformThreadTest, GetDefaultThreadStackSize) {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();
#if defined(OS_WIN) || defined(OS_IOS) || defined(OS_FUCHSIA) || \
    (defined(OS_LINUX) && !defined(THREAD_SANITIZER)) ||         \
    (defined(OS_ANDROID) && !defined(ADDRESS_SANITIZER))
  EXPECT_EQ(0u, stack_size);
#else
  EXPECT_GT(stack_size, 0u);
  EXPECT_LT(stack_size, 20u * (1 << 20));
#endif
}

}  // namespace base
