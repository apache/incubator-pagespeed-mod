// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Ne;
using testing::StrictMock;

namespace base {
namespace internal {
namespace {

const size_t kNumSequencesPerTest = 150;

class WorkerThreadDefaultDelegate : public WorkerThread::Delegate {
 public:
  WorkerThreadDefaultDelegate() = default;

  // WorkerThread::Delegate:
  WorkerThread::ThreadLabel GetThreadLabel() const override {
    return WorkerThread::ThreadLabel::DEDICATED;
  }
  void OnMainEntry(const WorkerThread* worker) override {}
  RunIntentWithRegisteredTaskSource GetWork(WorkerThread* worker) override {
    return nullptr;
  }
  void DidProcessTask(RegisteredTaskSource task_source) override {
    ADD_FAILURE() << "Unexpected call to DidRunTask()";
  }
  TimeDelta GetSleepTimeout() override { return TimeDelta::Max(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerThreadDefaultDelegate);
};

// The test parameter is the number of Tasks per Sequence returned by GetWork().
class ThreadPoolWorkerTest : public testing::TestWithParam<int> {
 protected:
  ThreadPoolWorkerTest() : num_get_work_cv_(lock_.CreateConditionVariable()) {}

  void SetUp() override {
    worker_ = MakeRefCounted<WorkerThread>(
        ThreadPriority::NORMAL,
        std::make_unique<TestWorkerThreadDelegate>(this),
        task_tracker_.GetTrackedRef());
    ASSERT_TRUE(worker_);
    worker_->Start();
    worker_set_.Signal();
    main_entry_called_.Wait();
  }

  void TearDown() override {
    // |worker_| needs to be released before ~TaskTracker() as it holds a
    // TrackedRef to it.
    worker_->JoinForTesting();
    worker_ = nullptr;
  }

  int TasksPerSequence() const { return GetParam(); }

  // Wait until GetWork() has been called |num_get_work| times.
  void WaitForNumGetWork(size_t num_get_work) {
    CheckedAutoLock auto_lock(lock_);
    while (num_get_work_ < num_get_work)
      num_get_work_cv_->Wait();
  }

  void SetMaxGetWork(size_t max_get_work) {
    CheckedAutoLock auto_lock(lock_);
    max_get_work_ = max_get_work;
  }

  void SetNumSequencesToCreate(size_t num_sequences_to_create) {
    CheckedAutoLock auto_lock(lock_);
    EXPECT_EQ(0U, num_sequences_to_create_);
    num_sequences_to_create_ = num_sequences_to_create;
  }

  size_t NumRunTasks() {
    CheckedAutoLock auto_lock(lock_);
    return num_run_tasks_;
  }

  std::vector<scoped_refptr<TaskSource>> CreatedTaskSources() {
    CheckedAutoLock auto_lock(lock_);
    return created_sequences_;
  }

  std::vector<scoped_refptr<TaskSource>> DidProcessTaskSequences() {
    CheckedAutoLock auto_lock(lock_);
    return did_run_task_sources_;
  }

  scoped_refptr<WorkerThread> worker_;

 private:
  class TestWorkerThreadDelegate : public WorkerThreadDefaultDelegate {
   public:
    TestWorkerThreadDelegate(ThreadPoolWorkerTest* outer) : outer_(outer) {}

    ~TestWorkerThreadDelegate() override {
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());
    }

    // WorkerThread::Delegate:
    void OnMainEntry(const WorkerThread* worker) override {
      outer_->worker_set_.Wait();
      EXPECT_EQ(outer_->worker_.get(), worker);
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());

      // Without synchronization, OnMainEntry() could be called twice without
      // generating an error.
      CheckedAutoLock auto_lock(outer_->lock_);
      EXPECT_FALSE(outer_->main_entry_called_.IsSignaled());
      outer_->main_entry_called_.Signal();
    }

    RunIntentWithRegisteredTaskSource GetWork(WorkerThread* worker) override {
      EXPECT_FALSE(IsCallToDidProcessTaskExpected());
      EXPECT_EQ(outer_->worker_.get(), worker);

      {
        CheckedAutoLock auto_lock(outer_->lock_);

        // Increment the number of times that this method has been called.
        ++outer_->num_get_work_;
        outer_->num_get_work_cv_->Signal();

        // Verify that this method isn't called more times than expected.
        EXPECT_LE(outer_->num_get_work_, outer_->max_get_work_);

        // Check if a Sequence should be returned.
        if (outer_->num_sequences_to_create_ == 0)
          return nullptr;
        --outer_->num_sequences_to_create_;
      }

      // Create a Sequence with TasksPerSequence() Tasks.
      scoped_refptr<Sequence> sequence =
          MakeRefCounted<Sequence>(TaskTraits{ThreadPool()}, nullptr,
                                   TaskSourceExecutionMode::kParallel);
      Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
      for (int i = 0; i < outer_->TasksPerSequence(); ++i) {
        Task task(FROM_HERE,
                  BindOnce(&ThreadPoolWorkerTest::RunTaskCallback,
                           Unretained(outer_)),
                  TimeDelta());
        EXPECT_TRUE(outer_->task_tracker_.WillPostTask(
            &task, sequence->shutdown_behavior()));
        sequence_transaction.PushTask(std::move(task));
      }
      auto registered_task_source =
          outer_->task_tracker_.WillQueueTaskSource(sequence);
      EXPECT_TRUE(registered_task_source);

      ExpectCallToDidProcessTask();

      {
        // Add the Sequence to the vector of created Sequences.
        CheckedAutoLock auto_lock(outer_->lock_);
        outer_->created_sequences_.push_back(sequence);
      }
      auto run_intent = registered_task_source->WillRunTask();
      EXPECT_TRUE(run_intent);
      return {std::move(registered_task_source), std::move(run_intent)};
    }

    // This override verifies that |task_source| has the expected number of
    // Tasks and adds it to |did_run_task_sources_|. Unlike a normal
    // DidProcessTask() implementation, it doesn't add |task_source| to a queue
    // for further execution.
    void DidProcessTask(RegisteredTaskSource registered_task_source) override {
      {
        CheckedAutoLock auto_lock(expect_did_run_task_lock_);
        EXPECT_TRUE(expect_did_run_task_);
        expect_did_run_task_ = false;
      }

      // If TasksPerSequence() is 1, |registered_task_source| should be nullptr.
      // Otherwise, |registered_task_source| should contain TasksPerSequence() -
      // 1 Tasks.
      if (outer_->TasksPerSequence() == 1) {
        EXPECT_FALSE(registered_task_source);
      } else {
        EXPECT_TRUE(registered_task_source);

        // Verify the number of Tasks in |registered_task_source|.
        auto transaction(registered_task_source->BeginTransaction());
        for (int i = 0; i < outer_->TasksPerSequence() - 1; ++i) {
          auto run_intent = registered_task_source->WillRunTask();
          EXPECT_TRUE(transaction.TakeTask(&run_intent));
          EXPECT_EQ(i == outer_->TasksPerSequence() - 2,
                    !transaction.DidProcessTask(std::move(run_intent)));
        }

        scoped_refptr<TaskSource> task_source =
            registered_task_source.Unregister();
        {
          // Add |task_source| to |did_run_task_sources_|.
          CheckedAutoLock auto_lock(outer_->lock_);
          outer_->did_run_task_sources_.push_back(std::move(task_source));
          EXPECT_LE(outer_->did_run_task_sources_.size(),
                    outer_->created_sequences_.size());
        }
      }
    }

   private:
    // Expect a call to DidProcessTask() before the next call to any other
    // method of this delegate.
    void ExpectCallToDidProcessTask() {
      CheckedAutoLock auto_lock(expect_did_run_task_lock_);
      expect_did_run_task_ = true;
    }

    bool IsCallToDidProcessTaskExpected() const {
      CheckedAutoLock auto_lock(expect_did_run_task_lock_);
      return expect_did_run_task_;
    }

    ThreadPoolWorkerTest* outer_;

    // Synchronizes access to |expect_did_run_task_|.
    mutable CheckedLock expect_did_run_task_lock_;

    // Whether the next method called on this delegate should be
    // DidProcessTask().
    bool expect_did_run_task_ = false;

    DISALLOW_COPY_AND_ASSIGN(TestWorkerThreadDelegate);
  };

  void RunTaskCallback() {
    CheckedAutoLock auto_lock(lock_);
    ++num_run_tasks_;
    EXPECT_LE(num_run_tasks_, created_sequences_.size());
  }

  TaskTracker task_tracker_{"Test"};

  // Synchronizes access to all members below.
  mutable CheckedLock lock_;

  // Signaled once OnMainEntry() has been called.
  WaitableEvent main_entry_called_;

  // Number of Sequences that should be created by GetWork(). When this
  // is 0, GetWork() returns nullptr.
  size_t num_sequences_to_create_ = 0;

  // Number of times that GetWork() has been called.
  size_t num_get_work_ = 0;

  // Maximum number of times that GetWork() can be called.
  size_t max_get_work_ = 0;

  // Condition variable signaled when |num_get_work_| is incremented.
  std::unique_ptr<ConditionVariable> num_get_work_cv_;

  // Sequences created by GetWork().
  std::vector<scoped_refptr<TaskSource>> created_sequences_;

  // Sequences passed to DidProcessTask().
  std::vector<scoped_refptr<TaskSource>> did_run_task_sources_;

  // Number of times that RunTaskCallback() has been called.
  size_t num_run_tasks_ = 0;

  // Signaled after |worker_| is set.
  WaitableEvent worker_set_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolWorkerTest);
};

}  // namespace

// Verify that when GetWork() continuously returns Sequences, all Tasks in these
// Sequences run successfully. The test wakes up the WorkerThread once.
TEST_P(ThreadPoolWorkerTest, ContinuousWork) {
  // Set GetWork() to return |kNumSequencesPerTest| Sequences before starting to
  // return nullptr.
  SetNumSequencesToCreate(kNumSequencesPerTest);

  // Expect |kNumSequencesPerTest| calls to GetWork() in which it returns a
  // Sequence and one call in which its returns nullptr.
  const size_t kExpectedNumGetWork = kNumSequencesPerTest + 1;
  SetMaxGetWork(kExpectedNumGetWork);

  // Wake up |worker_| and wait until GetWork() has been invoked the
  // expected amount of times.
  worker_->WakeUp();
  WaitForNumGetWork(kExpectedNumGetWork);

  // All tasks should have run.
  EXPECT_EQ(kNumSequencesPerTest, NumRunTasks());

  // If Sequences returned by GetWork() contain more than one Task, they aren't
  // empty after the worker pops Tasks from them and thus should be returned to
  // DidProcessTask().
  if (TasksPerSequence() > 1)
    EXPECT_EQ(CreatedTaskSources(), DidProcessTaskSequences());
  else
    EXPECT_TRUE(DidProcessTaskSequences().empty());
}

// Verify that when GetWork() alternates between returning a Sequence and
// returning nullptr, all Tasks in the returned Sequences run successfully. The
// test wakes up the WorkerThread once for each Sequence.
TEST_P(ThreadPoolWorkerTest, IntermittentWork) {
  for (size_t i = 0; i < kNumSequencesPerTest; ++i) {
    // Set GetWork() to return 1 Sequence before starting to return
    // nullptr.
    SetNumSequencesToCreate(1);

    // Expect |i + 1| calls to GetWork() in which it returns a Sequence and
    // |i + 1| calls in which it returns nullptr.
    const size_t expected_num_get_work = 2 * (i + 1);
    SetMaxGetWork(expected_num_get_work);

    // Wake up |worker_| and wait until GetWork() has been invoked
    // the expected amount of times.
    worker_->WakeUp();
    WaitForNumGetWork(expected_num_get_work);

    // The Task should have run
    EXPECT_EQ(i + 1, NumRunTasks());

    // If Sequences returned by GetWork() contain more than one Task, they
    // aren't empty after the worker pops Tasks from them and thus should be
    // returned to DidProcessTask().
    if (TasksPerSequence() > 1)
      EXPECT_EQ(CreatedTaskSources(), DidProcessTaskSequences());
    else
      EXPECT_TRUE(DidProcessTaskSequences().empty());
  }
}

INSTANTIATE_TEST_SUITE_P(OneTaskPerSequence,
                         ThreadPoolWorkerTest,
                         ::testing::Values(1));
INSTANTIATE_TEST_SUITE_P(TwoTasksPerSequence,
                         ThreadPoolWorkerTest,
                         ::testing::Values(2));

namespace {

class ControllableCleanupDelegate : public WorkerThreadDefaultDelegate {
 public:
  class Controls : public RefCountedThreadSafe<Controls> {
   public:
    Controls() = default;

    void HaveWorkBlock() { work_running_.Reset(); }

    void UnblockWork() { work_running_.Signal(); }

    void WaitForWorkToRun() { work_processed_.Wait(); }

    void WaitForCleanupRequest() { cleanup_requested_.Wait(); }

    void WaitForDelegateDestroy() { destroyed_.Wait(); }

    void WaitForMainExit() { exited_.Wait(); }

    void set_expect_get_work(bool expect_get_work) {
      expect_get_work_ = expect_get_work;
    }

    void ResetState() {
      work_running_.Signal();
      work_processed_.Reset();
      cleanup_requested_.Reset();
      exited_.Reset();
      work_requested_ = false;
    }

    void set_can_cleanup(bool can_cleanup) { can_cleanup_ = can_cleanup; }

   private:
    friend class ControllableCleanupDelegate;
    friend class RefCountedThreadSafe<Controls>;
    ~Controls() = default;

    WaitableEvent work_running_{WaitableEvent::ResetPolicy::MANUAL,
                                WaitableEvent::InitialState::SIGNALED};
    WaitableEvent work_processed_;
    WaitableEvent cleanup_requested_;
    WaitableEvent destroyed_;
    WaitableEvent exited_;

    bool expect_get_work_ = true;
    bool can_cleanup_ = false;
    bool work_requested_ = false;

    DISALLOW_COPY_AND_ASSIGN(Controls);
  };

  ControllableCleanupDelegate(TaskTracker* task_tracker)
      : task_tracker_(task_tracker), controls_(new Controls()) {}

  ~ControllableCleanupDelegate() override { controls_->destroyed_.Signal(); }

  RunIntentWithRegisteredTaskSource GetWork(WorkerThread* worker) override {
    EXPECT_TRUE(controls_->expect_get_work_);

    // Sends one item of work to signal |work_processed_|. On subsequent calls,
    // sends nullptr to indicate there's no more work to be done.
    if (controls_->work_requested_) {
      if (CanCleanup(worker)) {
        OnCleanup();
        worker->Cleanup();
        controls_->set_expect_get_work(false);
      }
      return nullptr;
    }

    controls_->work_requested_ = true;
    scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
        TaskTraits(ThreadPool(), WithBaseSyncPrimitives(),
                   TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN),
        nullptr, TaskSourceExecutionMode::kParallel);
    Task task(
        FROM_HERE,
        BindOnce(
            [](WaitableEvent* work_processed, WaitableEvent* work_running) {
              work_processed->Signal();
              work_running->Wait();
            },
            Unretained(&controls_->work_processed_),
            Unretained(&controls_->work_running_)),
        TimeDelta());
    EXPECT_TRUE(
        task_tracker_->WillPostTask(&task, sequence->shutdown_behavior()));
    sequence->BeginTransaction().PushTask(std::move(task));
    auto registered_task_source =
        task_tracker_->WillQueueTaskSource(std::move(sequence));
    EXPECT_TRUE(registered_task_source);
    auto run_intent = registered_task_source->WillRunTask();
    return {std::move(registered_task_source), std::move(run_intent)};
  }

  void DidProcessTask(RegisteredTaskSource) override {}

  void OnMainExit(WorkerThread* worker) override {
    controls_->exited_.Signal();
  }

  bool CanCleanup(WorkerThread* worker) {
    // Saving |can_cleanup_| now so that callers waiting on |cleanup_requested_|
    // have the thread go to sleep and then allow timing out.
    bool can_cleanup = controls_->can_cleanup_;
    controls_->cleanup_requested_.Signal();
    return can_cleanup;
  }

  void OnCleanup() {
    EXPECT_TRUE(controls_->can_cleanup_);
    EXPECT_TRUE(controls_->cleanup_requested_.IsSignaled());
  }

  // ControllableCleanupDelegate:
  scoped_refptr<Controls> controls() { return controls_; }

 private:
  scoped_refptr<Sequence> work_sequence_;
  TaskTracker* const task_tracker_;
  scoped_refptr<Controls> controls_;

  DISALLOW_COPY_AND_ASSIGN(ControllableCleanupDelegate);
};

class MockedControllableCleanupDelegate : public ControllableCleanupDelegate {
 public:
  MockedControllableCleanupDelegate(TaskTracker* task_tracker)
      : ControllableCleanupDelegate(task_tracker) {}
  ~MockedControllableCleanupDelegate() override = default;

  // WorkerThread::Delegate:
  MOCK_METHOD1(OnMainEntry, void(const WorkerThread* worker));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockedControllableCleanupDelegate);
};

}  // namespace

// Verify that calling WorkerThread::Cleanup() from GetWork() causes
// the WorkerThread's thread to exit.
TEST(ThreadPoolWorkerTest, WorkerCleanupFromGetWork) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  MockedControllableCleanupDelegate* delegate =
      new StrictMock<MockedControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();
  controls->set_can_cleanup(true);
  EXPECT_CALL(*delegate, OnMainEntry(_));
  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, WrapUnique(delegate),
                                   task_tracker.GetTrackedRef());
  worker->Start();
  worker->WakeUp();
  controls->WaitForWorkToRun();
  Mock::VerifyAndClear(delegate);
  controls->WaitForMainExit();
}

TEST(ThreadPoolWorkerTest, WorkerCleanupDuringWork) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, std::move(delegate),
                                   task_tracker.GetTrackedRef());
  worker->Start();
  worker->WakeUp();

  controls->WaitForWorkToRun();
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
}

TEST(ThreadPoolWorkerTest, WorkerCleanupDuringWait) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, std::move(delegate),
                                   task_tracker.GetTrackedRef());
  worker->Start();
  worker->WakeUp();

  controls->WaitForCleanupRequest();
  worker->Cleanup();
  worker = nullptr;
  controls->WaitForDelegateDestroy();
}

TEST(ThreadPoolWorkerTest, WorkerCleanupDuringShutdown) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, std::move(delegate),
                                   task_tracker.GetTrackedRef());
  worker->Start();
  worker->WakeUp();

  controls->WaitForWorkToRun();
  test::ShutdownTaskTracker(&task_tracker);
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
}

// Verify that Start() is a no-op after Cleanup().
TEST(ThreadPoolWorkerTest, CleanupBeforeStart) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the delegate
  // may destroy on a different thread. Mocks aren't designed with that in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();
  controls->set_expect_get_work(false);

  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, std::move(delegate),
                                   task_tracker.GetTrackedRef());

  worker->Cleanup();
  worker->Start();

  EXPECT_FALSE(worker->ThreadAliveForTesting());
}

namespace {

class CallJoinFromDifferentThread : public SimpleThread {
 public:
  CallJoinFromDifferentThread(WorkerThread* worker_to_join)
      : SimpleThread("WorkerThreadJoinThread"),
        worker_to_join_(worker_to_join) {}

  ~CallJoinFromDifferentThread() override = default;

  void Run() override {
    run_started_event_.Signal();
    worker_to_join_->JoinForTesting();
  }

  void WaitForRunToStart() { run_started_event_.Wait(); }

 private:
  WorkerThread* const worker_to_join_;
  WaitableEvent run_started_event_;
  DISALLOW_COPY_AND_ASSIGN(CallJoinFromDifferentThread);
};

}  // namespace

TEST(ThreadPoolWorkerTest, WorkerCleanupDuringJoin) {
  TaskTracker task_tracker("Test");
  // Will be owned by WorkerThread.
  // No mock here as that's reasonably covered by other tests and the
  // delegate may destroy on a different thread. Mocks aren't designed with that
  // in mind.
  std::unique_ptr<ControllableCleanupDelegate> delegate =
      std::make_unique<ControllableCleanupDelegate>(&task_tracker);
  scoped_refptr<ControllableCleanupDelegate::Controls> controls =
      delegate->controls();

  controls->HaveWorkBlock();

  auto worker =
      MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL, std::move(delegate),
                                   task_tracker.GetTrackedRef());
  worker->Start();
  worker->WakeUp();

  controls->WaitForWorkToRun();
  CallJoinFromDifferentThread join_from_different_thread(worker.get());
  join_from_different_thread.Start();
  join_from_different_thread.WaitForRunToStart();
  // Sleep here to give the other thread a chance to call JoinForTesting().
  // Receiving a signal that Run() was called doesn't mean JoinForTesting() was
  // necessarily called, and we can't signal after JoinForTesting() as
  // JoinForTesting() blocks until we call UnblockWork().
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  worker->Cleanup();
  worker = nullptr;
  controls->UnblockWork();
  controls->WaitForDelegateDestroy();
  join_from_different_thread.Join();
}

namespace {

class ExpectThreadPriorityDelegate : public WorkerThreadDefaultDelegate {
 public:
  ExpectThreadPriorityDelegate()
      : priority_verified_in_get_work_event_(
            WaitableEvent::ResetPolicy::AUTOMATIC,
            WaitableEvent::InitialState::NOT_SIGNALED),
        expected_thread_priority_(ThreadPriority::BACKGROUND) {}

  void SetExpectedThreadPriority(ThreadPriority expected_thread_priority) {
    expected_thread_priority_ = expected_thread_priority;
  }

  void WaitForPriorityVerifiedInGetWork() {
    priority_verified_in_get_work_event_.Wait();
  }

  // WorkerThread::Delegate:
  void OnMainEntry(const WorkerThread* worker) override {
    VerifyThreadPriority();
  }
  RunIntentWithRegisteredTaskSource GetWork(WorkerThread* worker) override {
    VerifyThreadPriority();
    priority_verified_in_get_work_event_.Signal();
    return nullptr;
  }

 private:
  void VerifyThreadPriority() {
    CheckedAutoLock auto_lock(expected_thread_priority_lock_);
    EXPECT_EQ(expected_thread_priority_,
              PlatformThread::GetCurrentThreadPriority());
  }

  // Signaled after GetWork() has verified the priority of the worker thread.
  WaitableEvent priority_verified_in_get_work_event_;

  // Synchronizes access to |expected_thread_priority_|.
  CheckedLock expected_thread_priority_lock_;

  // Expected thread priority for the next call to OnMainEntry() or GetWork().
  ThreadPriority expected_thread_priority_;

  DISALLOW_COPY_AND_ASSIGN(ExpectThreadPriorityDelegate);
};

}  // namespace

TEST(ThreadPoolWorkerTest, BumpPriorityOfAliveThreadDuringShutdown) {
  if (!CanUseBackgroundPriorityForWorkerThread())
    return;

  TaskTracker task_tracker("Test");

  // Block shutdown to ensure that the worker doesn't exit when StartShutdown()
  // is called.
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits{ThreadPool(), TaskShutdownBehavior::BLOCK_SHUTDOWN}, nullptr,
      TaskSourceExecutionMode::kParallel);
  auto registered_task_source =
      task_tracker.WillQueueTaskSource(std::move(sequence));

  std::unique_ptr<ExpectThreadPriorityDelegate> delegate(
      new ExpectThreadPriorityDelegate);
  ExpectThreadPriorityDelegate* delegate_raw = delegate.get();
  delegate_raw->SetExpectedThreadPriority(ThreadPriority::BACKGROUND);
  auto worker = MakeRefCounted<WorkerThread>(ThreadPriority::BACKGROUND,
                                             std::move(delegate),
                                             task_tracker.GetTrackedRef());
  worker->Start();

  // Verify that the initial thread priority is BACKGROUND (or NORMAL if thread
  // priority can't be increased).
  worker->WakeUp();
  delegate_raw->WaitForPriorityVerifiedInGetWork();

  // Verify that the thread priority is bumped to NORMAL during shutdown.
  delegate_raw->SetExpectedThreadPriority(ThreadPriority::NORMAL);
  task_tracker.StartShutdown();
  worker->WakeUp();
  delegate_raw->WaitForPriorityVerifiedInGetWork();

  worker->JoinForTesting();
}

namespace {

class VerifyCallsToObserverDelegate : public WorkerThreadDefaultDelegate {
 public:
  VerifyCallsToObserverDelegate(test::MockWorkerThreadObserver* observer)
      : observer_(observer) {}

  // WorkerThread::Delegate:
  void OnMainEntry(const WorkerThread* worker) override {
    Mock::VerifyAndClear(observer_);
  }

  void OnMainExit(WorkerThread* worker) override {
    observer_->AllowCallsOnMainExit(1);
  }

 private:
  test::MockWorkerThreadObserver* const observer_;

  DISALLOW_COPY_AND_ASSIGN(VerifyCallsToObserverDelegate);
};

}  // namespace

// Flaky: crbug.com/846121
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_WorkerThreadObserver DISABLED_WorkerThreadObserver
#else
#define MAYBE_WorkerThreadObserver WorkerThreadObserver
#endif

// Verify that the WorkerThreadObserver is notified when the worker enters
// and exits its main function.
TEST(ThreadPoolWorkerTest, MAYBE_WorkerThreadObserver) {
  StrictMock<test::MockWorkerThreadObserver> observer;
  {
    TaskTracker task_tracker("Test");
    auto delegate = std::make_unique<VerifyCallsToObserverDelegate>(&observer);
    auto worker = MakeRefCounted<WorkerThread>(ThreadPriority::NORMAL,
                                               std::move(delegate),
                                               task_tracker.GetTrackedRef());

    EXPECT_CALL(observer, OnWorkerThreadMainEntry());
    worker->Start(&observer);
    worker->Cleanup();
    worker = nullptr;
  }
  Mock::VerifyAndClear(&observer);
}

}  // namespace internal
}  // namespace base
