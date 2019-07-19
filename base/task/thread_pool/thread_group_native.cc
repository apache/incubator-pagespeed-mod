// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_native.h"

#include <algorithm>
#include <utility>

#include "base/system/sys_info.h"
#include "base/task/thread_pool/task_tracker.h"

namespace base {
namespace internal {

class ThreadGroupNative::ScopedWorkersExecutor
    : public ThreadGroup::BaseScopedWorkersExecutor {
 public:
  ScopedWorkersExecutor(ThreadGroupNative* outer) : outer_(outer) {}
  ~ScopedWorkersExecutor() {
    CheckedLock::AssertNoLockHeldOnCurrentThread();

    for (size_t i = 0; i < num_threadpool_work_to_submit_; ++i)
      outer_->SubmitWork();
  }

  // Sets the number of threadpool work to submit upon destruction.
  void set_num_threadpool_work_to_submit(size_t num) {
    DCHECK_EQ(num_threadpool_work_to_submit_, 0U);
    num_threadpool_work_to_submit_ = num;
  }

 private:
  ThreadGroupNative* const outer_;
  size_t num_threadpool_work_to_submit_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScopedWorkersExecutor);
};

ThreadGroupNative::ThreadGroupNative(TrackedRef<TaskTracker> task_tracker,
                                     TrackedRef<Delegate> delegate,
                                     ThreadGroup* predecessor_thread_group)
    : ThreadGroup(std::move(task_tracker),
                  std::move(delegate),
                  predecessor_thread_group) {}

ThreadGroupNative::~ThreadGroupNative() {
#if DCHECK_IS_ON()
  // Verify join_for_testing has been called to ensure that there is no more
  // outstanding work. Otherwise, work may try to de-reference an invalid
  // pointer to this class.
  DCHECK(join_for_testing_returned_);
#endif
}

void ThreadGroupNative::Start(WorkerEnvironment worker_environment) {
  worker_environment_ = worker_environment;

  StartImpl();

  ScopedWorkersExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(!started_);
  started_ = true;
  EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupNative::JoinForTesting() {
  {
    CheckedAutoLock auto_lock(lock_);
    priority_queue_.EnableFlushTaskSourcesOnDestroyForTesting();
  }

  JoinImpl();
#if DCHECK_IS_ON()
  DCHECK(!join_for_testing_returned_);
  join_for_testing_returned_ = true;
#endif
}

void ThreadGroupNative::RunNextTaskSourceImpl() {
  RunIntentWithRegisteredTaskSource run_intent_with_task_source = GetWork();

  if (run_intent_with_task_source) {
    BindToCurrentThread();
    RegisteredTaskSource task_source = task_tracker_->RunAndPopNextTask(
        std::move(run_intent_with_task_source));
    UnbindFromCurrentThread();

    if (task_source) {
      ScopedWorkersExecutor workers_executor(this);
      ScopedReenqueueExecutor reenqueue_executor;
      auto transaction_with_task_source =
          TransactionWithRegisteredTaskSource::FromTaskSource(
              std::move(task_source));
      CheckedAutoLock auto_lock(lock_);
      ReEnqueueTaskSourceLockRequired(&workers_executor, &reenqueue_executor,
                                      std::move(transaction_with_task_source));
    }
  }
}

RunIntentWithRegisteredTaskSource ThreadGroupNative::GetWork() {
  CheckedAutoLock auto_lock(lock_);
  DCHECK_GT(num_pending_threadpool_work_, 0U);
  --num_pending_threadpool_work_;
  // There can be more pending threadpool work than TaskSources in the
  // PriorityQueue after RemoveTaskSource().
  if (priority_queue_.IsEmpty())
    return nullptr;

  // Enforce the CanRunPolicy.
  const TaskPriority priority = priority_queue_.PeekSortKey().priority();
  if (!task_tracker_->CanRunPriority(priority))
    return nullptr;
  RegisteredTaskSource task_source = priority_queue_.PopTaskSource();
  auto run_intent = task_source->WillRunTask();
  DCHECK(run_intent);
  return {std::move(task_source), std::move(run_intent)};
}

void ThreadGroupNative::UpdateSortKey(
    TransactionWithOwnedTaskSource transaction_with_task_source) {
  ScopedWorkersExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(transaction_with_task_source));
}

void ThreadGroupNative::PushTaskSourceAndWakeUpWorkers(
    TransactionWithRegisteredTaskSource transaction_with_task_source) {
  ScopedWorkersExecutor executor(this);
  PushTaskSourceAndWakeUpWorkersImpl(&executor,
                                     std::move(transaction_with_task_source));
}

void ThreadGroupNative::EnsureEnoughWorkersLockRequired(
    BaseScopedWorkersExecutor* executor) {
  if (!started_)
    return;
  // Ensure that there is at least one pending threadpool work per TaskSource in
  // the PriorityQueue.
  const size_t desired_num_pending_threadpool_work =
      GetNumQueuedCanRunBestEffortTaskSources() +
      GetNumQueuedCanRunForegroundTaskSources();

  if (desired_num_pending_threadpool_work > num_pending_threadpool_work_) {
    static_cast<ScopedWorkersExecutor*>(executor)
        ->set_num_threadpool_work_to_submit(
            desired_num_pending_threadpool_work - num_pending_threadpool_work_);
    num_pending_threadpool_work_ = desired_num_pending_threadpool_work;
  }
}

size_t ThreadGroupNative::GetMaxConcurrentNonBlockedTasksDeprecated() const {
  // Native thread pools give us no control over the number of workers that are
  // active at one time. Consequently, we cannot report a true value here.
  // Instead, the values were chosen to match
  // ThreadPoolInstance::StartWithDefaultParams.
  const int num_cores = SysInfo::NumberOfProcessors();
  return std::max(3, num_cores - 1);
}

void ThreadGroupNative::ReportHeartbeatMetrics() const {
  // Native thread pools do not provide the capability to determine the
  // number of worker threads created.
}

void ThreadGroupNative::DidUpdateCanRunPolicy() {
  ScopedWorkersExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  EnsureEnoughWorkersLockRequired(&executor);
}

}  // namespace internal
}  // namespace base
