// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/object_watcher.h"

#include <process.h>
#include <windows.h>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

class QuitDelegate : public ObjectWatcher::Delegate {
 public:
  void OnObjectSignaled(HANDLE object) override {
    RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};

class DecrementCountDelegate : public ObjectWatcher::Delegate {
 public:
  explicit DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  void OnObjectSignaled(HANDLE object) override { --(*counter_); }

 private:
  int* counter_;
};

void RunTest_BasicSignal(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

  ObjectWatcher watcher;
  EXPECT_FALSE(watcher.IsWatching());

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_EQ(event, watcher.GetWatchedObject());

  SetEvent(event);

  RunLoop().Run();

  EXPECT_FALSE(watcher.IsWatching());
  CloseHandle(event);
}

void RunTest_BasicCancel(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

  ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  watcher.StopWatching();

  CloseHandle(event);
}

void RunTest_CancelAfterSet(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

  ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  SetEvent(event);

  // Let the background thread do its business
  Sleep(30);

  watcher.StopWatching();

  RunLoop().RunUntilIdle();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);

  CloseHandle(event);
}

void RunTest_SignalBeforeWatch(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

  ObjectWatcher watcher;

  // A manual-reset event that is signaled before we begin watching.
  HANDLE event = CreateEvent(NULL, TRUE, TRUE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatchingOnce(event, &delegate);
  EXPECT_TRUE(ok);

  RunLoop().Run();

  EXPECT_FALSE(watcher.IsWatching());
  CloseHandle(event);
}

void RunTest_OutlivesTaskEnvironment(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  // Simulate a task environment that dies before an ObjectWatcher.  This
  // ordinarily doesn't happen when people use the Thread class, but it can
  // happen when people use the Singleton pattern or atexit.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);  // not signaled
  {
    ObjectWatcher watcher;
    {
      test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

      QuitDelegate delegate;
      watcher.StartWatchingOnce(event, &delegate);
    }
  }
  CloseHandle(event);
}

class QuitAfterMultipleDelegate : public ObjectWatcher::Delegate {
 public:
  QuitAfterMultipleDelegate(HANDLE event, int iterations)
      : event_(event), iterations_(iterations) {}
  void OnObjectSignaled(HANDLE object) override {
    if (--iterations_) {
      SetEvent(event_);
    } else {
      RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }

 private:
  HANDLE event_;
  int iterations_;
};

void RunTest_ExecuteMultipleTimes(
    test::ScopedTaskEnvironment::MainThreadType main_thread_type) {
  test::ScopedTaskEnvironment scoped_task_environment(main_thread_type);

  ObjectWatcher watcher;
  EXPECT_FALSE(watcher.IsWatching());

  // An auto-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, FALSE, FALSE, NULL);

  QuitAfterMultipleDelegate delegate(event, 2);
  bool ok = watcher.StartWatchingMultipleTimes(event, &delegate);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_EQ(event, watcher.GetWatchedObject());

  SetEvent(event);

  RunLoop().Run();

  EXPECT_TRUE(watcher.IsWatching());
  EXPECT_TRUE(watcher.StopWatching());
  CloseHandle(event);
}

}  // namespace

//-----------------------------------------------------------------------------

TEST(ObjectWatcherTest, BasicSignal) {
  RunTest_BasicSignal(test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_BasicSignal(test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_BasicSignal(test::ScopedTaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, BasicCancel) {
  RunTest_BasicCancel(test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_BasicCancel(test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_BasicCancel(test::ScopedTaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, CancelAfterSet) {
  RunTest_CancelAfterSet(test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_CancelAfterSet(test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_CancelAfterSet(test::ScopedTaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, SignalBeforeWatch) {
  RunTest_SignalBeforeWatch(
      test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_SignalBeforeWatch(test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_SignalBeforeWatch(test::ScopedTaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, OutlivesTaskEnvironment) {
  RunTest_OutlivesTaskEnvironment(
      test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_OutlivesTaskEnvironment(
      test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_OutlivesTaskEnvironment(
      test::ScopedTaskEnvironment::MainThreadType::UI);
}

TEST(ObjectWatcherTest, ExecuteMultipleTimes) {
  RunTest_ExecuteMultipleTimes(
      test::ScopedTaskEnvironment::MainThreadType::DEFAULT);
  RunTest_ExecuteMultipleTimes(test::ScopedTaskEnvironment::MainThreadType::IO);
  RunTest_ExecuteMultipleTimes(test::ScopedTaskEnvironment::MainThreadType::UI);
}

}  // namespace win
}  // namespace base
