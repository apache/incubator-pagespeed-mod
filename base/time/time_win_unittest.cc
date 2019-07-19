// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <mmsystem.h>
#include <process.h>
#include <stdint.h>

#include <cmath>
#include <limits>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// For TimeDelta::ConstexprInitialization
constexpr int kExpectedDeltaInMilliseconds = 10;
constexpr TimeDelta kConstexprTimeDelta =
    TimeDelta::FromMilliseconds(kExpectedDeltaInMilliseconds);

class MockTimeTicks : public TimeTicks {
 public:
  static DWORD Ticker() {
    return static_cast<int>(InterlockedIncrement(&ticker_));
  }

  static void InstallTicker() {
    old_tick_function_ = SetMockTickFunction(&Ticker);
    ticker_ = -5;
  }

  static void UninstallTicker() { SetMockTickFunction(old_tick_function_); }

 private:
  static volatile LONG ticker_;
  static TickFunctionType old_tick_function_;
};

volatile LONG MockTimeTicks::ticker_;
MockTimeTicks::TickFunctionType MockTimeTicks::old_tick_function_;

HANDLE g_rollover_test_start;

unsigned __stdcall RolloverTestThreadMain(void* param) {
  int64_t counter = reinterpret_cast<int64_t>(param);
  DWORD rv = WaitForSingleObject(g_rollover_test_start, INFINITE);
  EXPECT_EQ(rv, WAIT_OBJECT_0);

  TimeTicks last = TimeTicks::Now();
  for (int index = 0; index < counter; index++) {
    TimeTicks now = TimeTicks::Now();
    int64_t milliseconds = (now - last).InMilliseconds();
    // This is a tight loop; we could have looped faster than our
    // measurements, so the time might be 0 millis.
    EXPECT_GE(milliseconds, 0);
    EXPECT_LT(milliseconds, 250);
    last = now;
  }
  return 0;
}

#if defined(_M_ARM64) && defined(__clang__)
#define ReadCycleCounter() _ReadStatusReg(ARM64_PMCCNTR_EL0)
#else
#define ReadCycleCounter() __rdtsc()
#endif

// Measure the performance of the CPU cycle counter so that we can compare it to
// the overhead of QueryPerformanceCounter. A hard-coded frequency is used
// because we don't care about the accuracy of the results, we just need to do
// the work. The amount of work is not exactly the same as in TimeTicks::Now
// (some steps are skipped) but that doesn't seem to materially affect the
// results.
TimeTicks GetTSC() {
  // Using a fake cycle counter frequency for test purposes.
  return TimeTicks() +
         TimeDelta::FromMicroseconds(ReadCycleCounter() *
                                     Time::kMicrosecondsPerSecond / 10000000);
}

}  // namespace

// This test spawns many threads, and can occasionally fail due to resource
// exhaustion in the presence of ASan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_WinRollover DISABLED_WinRollover
#else
#define MAYBE_WinRollover WinRollover
#endif
TEST(TimeTicks, MAYBE_WinRollover) {
  // The internal counter rolls over at ~49days.  We'll use a mock
  // timer to test this case.
  // Basic test algorithm:
  //   1) Set clock to rollover - N
  //   2) Create N threads
  //   3) Start the threads
  //   4) Each thread loops through TimeTicks() N times
  //   5) Each thread verifies integrity of result.

  const int kThreads = 8;
  // Use int64_t so we can cast into a void* without a compiler warning.
  const int64_t kChecks = 10;

  // It takes a lot of iterations to reproduce the bug!
  // (See bug 1081395)
  for (int loop = 0; loop < 4096; loop++) {
    // Setup
    MockTimeTicks::InstallTicker();
    g_rollover_test_start = CreateEvent(0, TRUE, FALSE, 0);
    HANDLE threads[kThreads];

    for (int index = 0; index < kThreads; index++) {
      void* argument = reinterpret_cast<void*>(kChecks);
      unsigned thread_id;
      threads[index] = reinterpret_cast<HANDLE>(_beginthreadex(
          NULL, 0, RolloverTestThreadMain, argument, 0, &thread_id));
      EXPECT_NE((HANDLE)NULL, threads[index]);
    }

    // Start!
    SetEvent(g_rollover_test_start);

    // Wait for threads to finish
    for (int index = 0; index < kThreads; index++) {
      DWORD rv = WaitForSingleObject(threads[index], INFINITE);
      EXPECT_EQ(rv, WAIT_OBJECT_0);
      // Since using _beginthreadex() (as opposed to _beginthread),
      // an explicit CloseHandle() is supposed to be called.
      CloseHandle(threads[index]);
    }

    CloseHandle(g_rollover_test_start);

    // Teardown
    MockTimeTicks::UninstallTicker();
  }
}

TEST(TimeTicks, SubMillisecondTimers) {
  // IsHighResolution() is false on some systems.  Since the product still works
  // even if it's false, it makes this entire test questionable.
  if (!TimeTicks::IsHighResolution())
    return;

  const int kRetries = 1000;
  bool saw_submillisecond_timer = false;

  // Run kRetries attempts to see a sub-millisecond timer.
  for (int index = 0; index < kRetries; index++) {
    TimeTicks last_time = TimeTicks::Now();
    TimeDelta delta;
    // Spin until the clock has detected a change.
    do {
      delta = TimeTicks::Now() - last_time;
    } while (delta.InMicroseconds() == 0);
    if (delta.InMicroseconds() < 1000) {
      saw_submillisecond_timer = true;
      break;
    }
  }
  EXPECT_TRUE(saw_submillisecond_timer);
}

TEST(TimeTicks, TimeGetTimeCaps) {
  // Test some basic assumptions that we expect about how timeGetDevCaps works.

  TIMECAPS caps;
  MMRESULT status = timeGetDevCaps(&caps, sizeof(caps));
  ASSERT_EQ(static_cast<MMRESULT>(MMSYSERR_NOERROR), status);

  EXPECT_GE(static_cast<int>(caps.wPeriodMin), 1);
  EXPECT_GT(static_cast<int>(caps.wPeriodMax), 1);
  EXPECT_GE(static_cast<int>(caps.wPeriodMin), 1);
  EXPECT_GT(static_cast<int>(caps.wPeriodMax), 1);
  printf("timeGetTime range is %d to %dms\n", caps.wPeriodMin, caps.wPeriodMax);
}

TEST(TimeTicks, QueryPerformanceFrequency) {
  // Test some basic assumptions that we expect about QPC.

  LARGE_INTEGER frequency;
  BOOL rv = QueryPerformanceFrequency(&frequency);
  EXPECT_EQ(TRUE, rv);
  EXPECT_GT(frequency.QuadPart, 1000000);  // Expect at least 1MHz
  printf("QueryPerformanceFrequency is %5.2fMHz\n",
         frequency.QuadPart / 1000000.0);
}

TEST(TimeTicks, TimerPerformance) {
  // Verify that various timer mechanisms can always complete quickly.
  // Note:  This is a somewhat arbitrary test.
  const int kLoops = 500000;

  typedef TimeTicks (*TestFunc)();
  struct TestCase {
    TestFunc func;
    const char* description;
  };
  // Cheating a bit here:  assumes sizeof(TimeTicks) == sizeof(Time)
  // in order to create a single test case list.
  static_assert(sizeof(TimeTicks) == sizeof(Time),
                "TimeTicks and Time must be the same size");
  std::vector<TestCase> cases;
  cases.push_back({reinterpret_cast<TestFunc>(&Time::Now), "Time::Now"});
  cases.push_back({&TimeTicks::Now, "TimeTicks::Now"});
  cases.push_back({&GetTSC, "CPUCycleCounter"});

  if (ThreadTicks::IsSupported()) {
    ThreadTicks::WaitUntilInitialized();
    cases.push_back(
        {reinterpret_cast<TestFunc>(&ThreadTicks::Now), "ThreadTicks::Now"});
  }

  // Warm up the CPU to its full clock rate so that we get accurate timing
  // information.
  DWORD start_tick = GetTickCount();
  const DWORD kWarmupMs = 50;
  for (;;) {
    DWORD elapsed = GetTickCount() - start_tick;
    if (elapsed > kWarmupMs)
      break;
  }

  for (const auto& test_case : cases) {
    TimeTicks start = TimeTicks::Now();
    for (int index = 0; index < kLoops; index++)
      test_case.func();
    TimeTicks stop = TimeTicks::Now();
    // Turning off the check for acceptible delays.  Without this check,
    // the test really doesn't do much other than measure.  But the
    // measurements are still useful for testing timers on various platforms.
    // The reason to remove the check is because the tests run on many
    // buildbots, some of which are VMs.  These machines can run horribly
    // slow, and there is really no value for checking against a max timer.
    // const int kMaxTime = 35;  // Maximum acceptible milliseconds for test.
    // EXPECT_LT((stop - start).InMilliseconds(), kMaxTime);
    printf("%s: %1.2fus per call\n", test_case.description,
           (stop - start).InMillisecondsF() * 1000 / kLoops);
  }
}

#if !defined(ARCH_CPU_ARM64)
// This test is disabled on Windows ARM64 systems because TSCTicksPerSecond is
// only used in Chromium for QueryThreadCycleTime, and QueryThreadCycleTime
// doesn't use a constant-rate timer on ARM64.
TEST(TimeTicks, TSCTicksPerSecond) {
  if (ThreadTicks::IsSupported()) {
    ThreadTicks::WaitUntilInitialized();

    // Read the CPU frequency from the registry.
    base::win::RegKey processor_key(
        HKEY_LOCAL_MACHINE,
        STRING16_LITERAL("Hardware\\Description\\System\\CentralProcessor\\0"),
        KEY_QUERY_VALUE);
    ASSERT_TRUE(processor_key.Valid());
    DWORD processor_mhz_from_registry;
    ASSERT_EQ(ERROR_SUCCESS,
              processor_key.ReadValueDW(STRING16_LITERAL("~MHz"),
                                        &processor_mhz_from_registry));

    // Expect the measured TSC frequency to be similar to the processor
    // frequency from the registry (0.5% error).
    double tsc_mhz_measured = ThreadTicks::TSCTicksPerSecond() / 1e6;
    EXPECT_NEAR(tsc_mhz_measured, processor_mhz_from_registry,
                0.005 * processor_mhz_from_registry);
  }
}
#endif

TEST(TimeTicks, FromQPCValue) {
  if (!TimeTicks::IsHighResolution())
    return;

  LARGE_INTEGER frequency;
  ASSERT_TRUE(QueryPerformanceFrequency(&frequency));
  const int64_t ticks_per_second = frequency.QuadPart;
  ASSERT_GT(ticks_per_second, 0);

  // Generate the tick values to convert, advancing the tick count by varying
  // amounts.  These values will ensure that both the fast and overflow-safe
  // conversion logic in FromQPCValue() is tested, and across the entire range
  // of possible QPC tick values.
  std::vector<int64_t> test_cases;
  test_cases.push_back(0);
  const int kNumAdvancements = 100;
  int64_t ticks = 0;
  int64_t ticks_increment = 10;
  for (int i = 0; i < kNumAdvancements; ++i) {
    test_cases.push_back(ticks);
    ticks += ticks_increment;
    ticks_increment = ticks_increment * 6 / 5;
  }
  test_cases.push_back(Time::kQPCOverflowThreshold - 1);
  test_cases.push_back(Time::kQPCOverflowThreshold);
  test_cases.push_back(Time::kQPCOverflowThreshold + 1);
  ticks = Time::kQPCOverflowThreshold + 10;
  ticks_increment = 10;
  for (int i = 0; i < kNumAdvancements; ++i) {
    test_cases.push_back(ticks);
    ticks += ticks_increment;
    ticks_increment = ticks_increment * 6 / 5;
  }
  test_cases.push_back(std::numeric_limits<int64_t>::max());

  // Test that the conversions using FromQPCValue() match those computed here
  // using simple floating-point arithmetic.  The floating-point math provides
  // enough precision for all reasonable values to confirm that the
  // implementation is correct to the microsecond, and for "very large" values
  // it confirms that the answer is very close to correct.
  for (int64_t ticks : test_cases) {
    const double expected_microseconds_since_origin =
        (static_cast<double>(ticks) * Time::kMicrosecondsPerSecond) /
        ticks_per_second;
    const TimeTicks converted_value = TimeTicks::FromQPCValue(ticks);
    const double converted_microseconds_since_origin =
        static_cast<double>((converted_value - TimeTicks()).InMicroseconds());
    // When we test with very large numbers we end up in a range where adjacent
    // double values are far apart - 512.0 apart in one test failure. In that
    // situation it makes no sense for our epsilon to be 1.0 - it should be
    // the difference between adjacent doubles.
    double epsilon = nextafter(expected_microseconds_since_origin, INFINITY) -
                     expected_microseconds_since_origin;
    // Epsilon must be at least 1.0 because converted_microseconds_since_origin
    // comes from an integral value, and expected_microseconds_since_origin is
    // a double that is expected to be up to 0.999 larger. In addition, due to
    // multiple roundings in the double calculation the actual error can be
    // slightly larger than 1.0, even when the converted value is perfect. This
    // epsilon value was chosen because it is slightly larger than the error
    // seen in a test failure caused by the double rounding.
    const double min_epsilon = 1.002;
    if (epsilon < min_epsilon)
      epsilon = min_epsilon;
    EXPECT_NEAR(expected_microseconds_since_origin,
                converted_microseconds_since_origin, epsilon)
        << "ticks=" << ticks << ", to be converted via logic path: "
        << (ticks < Time::kQPCOverflowThreshold ? "FAST" : "SAFE");
  }
}

TEST(TimeDelta, ConstexprInitialization) {
  // Make sure that TimeDelta works around crbug.com/635974
  EXPECT_EQ(kExpectedDeltaInMilliseconds, kConstexprTimeDelta.InMilliseconds());
}

TEST(TimeDelta, FromFileTime) {
  FILETIME ft;
  ft.dwLowDateTime = 1001;
  ft.dwHighDateTime = 0;

  // 100100 ns ~= 100 us.
  EXPECT_EQ(TimeDelta::FromMicroseconds(100), TimeDelta::FromFileTime(ft));

  ft.dwLowDateTime = 0;
  ft.dwHighDateTime = 1;

  // 2^32 * 100 ns ~= 2^32 * 10 us.
  EXPECT_EQ(TimeDelta::FromMicroseconds((1ull << 32) / 10),
            TimeDelta::FromFileTime(ft));
}

TEST(HighResolutionTimer, GetUsage) {
  EXPECT_EQ(0.0, Time::GetHighResolutionTimerUsage());

  Time::ResetHighResolutionTimerUsage();

  // 0% usage since the timer isn't activated regardless of how much time has
  // elapsed.
  EXPECT_EQ(0.0, Time::GetHighResolutionTimerUsage());
  Sleep(10);
  EXPECT_EQ(0.0, Time::GetHighResolutionTimerUsage());

  Time::ActivateHighResolutionTimer(true);
  Time::ResetHighResolutionTimerUsage();

  Sleep(20);
  // 100% usage since the timer has been activated entire time.
  EXPECT_EQ(100.0, Time::GetHighResolutionTimerUsage());

  Time::ActivateHighResolutionTimer(false);
  Sleep(20);
  double usage1 = Time::GetHighResolutionTimerUsage();
  // usage1 should be about 50%.
  EXPECT_LT(usage1, 100.0);
  EXPECT_GT(usage1, 0.0);

  Time::ActivateHighResolutionTimer(true);
  Sleep(10);
  Time::ActivateHighResolutionTimer(false);
  double usage2 = Time::GetHighResolutionTimerUsage();
  // usage2 should be about 60%.
  EXPECT_LT(usage2, 100.0);
  EXPECT_GT(usage2, usage1);

  Time::ResetHighResolutionTimerUsage();
  EXPECT_EQ(0.0, Time::GetHighResolutionTimerUsage());
}

}  // namespace base
