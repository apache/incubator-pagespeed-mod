// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

struct CheckOnDestroy {
  ~CheckOnDestroy() { CHECK(false); }
};

TEST(NoDestructorTest, SkipsDestructors) {
  NoDestructor<CheckOnDestroy> destructor_should_not_run;
}

struct CopyOnly {
  CopyOnly() = default;

  CopyOnly(const CopyOnly&) = default;
  CopyOnly& operator=(const CopyOnly&) = default;

  CopyOnly(CopyOnly&&) = delete;
  CopyOnly& operator=(CopyOnly&&) = delete;
};

struct MoveOnly {
  MoveOnly() = default;

  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
};

struct ForwardingTestStruct {
  ForwardingTestStruct(const CopyOnly&, MoveOnly&&) {}
};

TEST(NoDestructorTest, ForwardsArguments) {
  CopyOnly copy_only;
  MoveOnly move_only;

  static NoDestructor<ForwardingTestStruct> test_forwarding(
      copy_only, std::move(move_only));
}

TEST(NoDestructorTest, Accessors) {
  static NoDestructor<std::string> awesome("awesome");

  EXPECT_EQ("awesome", *awesome);
  EXPECT_EQ(0, awesome->compare("awesome"));
  EXPECT_EQ(0, awesome.get()->compare("awesome"));
}

// Passing initializer list to a NoDestructor like in this test
// is ambiguous in GCC.
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84849
#if !defined(COMPILER_GCC) && !defined(__clang__)
TEST(NoDestructorTest, InitializerList) {
  static NoDestructor<std::vector<std::string>> vector({"a", "b", "c"});
}
#endif
}  // namespace

namespace {

// A class whose constructor busy-loops until it is told to complete
// construction.
class BlockingConstructor {
 public:
  BlockingConstructor() {
    EXPECT_FALSE(WasConstructorCalled());
    subtle::NoBarrier_Store(&constructor_called_, 1);
    EXPECT_TRUE(WasConstructorCalled());
    while (!subtle::NoBarrier_Load(&complete_construction_))
      PlatformThread::YieldCurrentThread();
    done_construction_ = true;
  }

  ~BlockingConstructor() = delete;

  // Returns true if BlockingConstructor() was entered.
  static bool WasConstructorCalled() {
    return subtle::NoBarrier_Load(&constructor_called_);
  }

  // Instructs BlockingConstructor() that it may now unblock its construction.
  static void CompleteConstructionNow() {
    subtle::NoBarrier_Store(&complete_construction_, 1);
  }

  bool done_construction() { return done_construction_; }

 private:
  // Use Atomic32 instead of AtomicFlag for them to be trivially initialized.
  static subtle::Atomic32 constructor_called_;
  static subtle::Atomic32 complete_construction_;

  bool done_construction_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlockingConstructor);
};

// static
subtle::Atomic32 BlockingConstructor::constructor_called_ = 0;
// static
subtle::Atomic32 BlockingConstructor::complete_construction_ = 0;

// A SimpleThread running at |thread_priority| which invokes |before_get|
// (optional) and then invokes thread-safe
// scoped-static-initializationconstruction on its NoDestructor instance.
class BlockingConstructorThread : public SimpleThread {
 public:
  BlockingConstructorThread(ThreadPriority thread_priority,
                            OnceClosure before_get)
      : SimpleThread("BlockingConstructorThread", Options(thread_priority)),
        before_get_(std::move(before_get)) {}

  void Run() override {
    if (before_get_)
      std::move(before_get_).Run();

    static NoDestructor<BlockingConstructor> instance;
    EXPECT_TRUE(instance->done_construction());
  }

 private:
  OnceClosure before_get_;

  DISALLOW_COPY_AND_ASSIGN(BlockingConstructorThread);
};

}  // namespace

// Tests that if the thread assigned to construct the local-static
// initialization of the NoDestructor runs at background priority : the
// foreground threads will yield to it enough for it to eventually complete
// construction. While local-static thread-safe initialization isn't specific to
// NoDestructor, it is tested here as NoDestructor is set to replace
// LazyInstance and this is an important regression test for it
// (https://crbug.com/797129).
TEST(NoDestructorTest, PriorityInversionAtStaticInitializationResolves) {
  TimeTicks test_begin = TimeTicks::Now();

  // Construct BlockingConstructor from a background thread.
  BlockingConstructorThread background_getter(ThreadPriority::BACKGROUND,
                                              OnceClosure());
  background_getter.Start();

  while (!BlockingConstructor::WasConstructorCalled())
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));

  // Spin 4 foreground thread per core contending to get the already under
  // construction NoDestructor. When they are all running and poking at it :
  // allow the background thread to complete its work.
  const int kNumForegroundThreads = 4 * SysInfo::NumberOfProcessors();
  std::vector<std::unique_ptr<SimpleThread>> foreground_threads;
  RepeatingClosure foreground_thread_ready_callback =
      BarrierClosure(kNumForegroundThreads,
                     BindOnce(&BlockingConstructor::CompleteConstructionNow));
  for (int i = 0; i < kNumForegroundThreads; ++i) {
    foreground_threads.push_back(std::make_unique<BlockingConstructorThread>(
        ThreadPriority::NORMAL, foreground_thread_ready_callback));
    foreground_threads.back()->Start();
  }

  // This test will hang if the foreground threads become stuck in
  // NoDestructor's construction per the background thread never being scheduled
  // to complete construction.
  for (auto& foreground_thread : foreground_threads)
    foreground_thread->Join();
  background_getter.Join();

  // Fail if this test takes more than 5 seconds (it takes 5-10 seconds on a
  // Z840 without r527445 but is expected to be fast (~30ms) with the fix).
  EXPECT_LT(TimeTicks::Now() - test_begin, TimeDelta::FromSeconds(5));
}

}  // namespace base
