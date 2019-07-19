// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_POOLED_TASK_RUNNER_DELEGATE_H_
#define BASE_TASK_THREAD_POOL_POOLED_TASK_RUNNER_DELEGATE_H_

#include "base/base_export.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task.h"
#include "base/task/thread_pool/task_source.h"

namespace base {
namespace internal {

// Delegate interface for PooledParallelTaskRunner and
// PooledSequencedTaskRunner.
class BASE_EXPORT PooledTaskRunnerDelegate {
 public:
  PooledTaskRunnerDelegate();
  virtual ~PooledTaskRunnerDelegate();

  // Returns true if a PooledTaskRunnerDelegate instance exists in the
  // process. This is needed in case of unit tests wherein a TaskRunner
  // outlives the ThreadPoolInstance that created it.
  static bool Exists();

  // Invoked when a |task| is posted to the PooledParallelTaskRunner or
  // PooledSequencedTaskRunner. The implementation must post |task| to
  // |sequence| within the appropriate priority queue, depending on |sequence|
  // traits. Returns true if task was successfully posted.
  virtual bool PostTaskWithSequence(Task task,
                                    scoped_refptr<Sequence> sequence) = 0;

  // Invoked when RunsTasksInCurrentSequence() is called on a
  // PooledParallelTaskRunner. Returns true if the current thread is part of the
  // ThreadGroup associated with |traits|.
  virtual bool IsRunningPoolWithTraits(const TaskTraits& traits) const = 0;

  // Invoked when the priority of |sequence|'s TaskRunner is updated. The
  // implementation must update |sequence|'s priority to |priority|, then place
  // |sequence| in the correct priority-queue position within the appropriate
  // thread group.
  virtual void UpdatePriority(scoped_refptr<TaskSource> task_source,
                              TaskPriority priority) = 0;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_POOLED_TASK_RUNNER_DELEGATE_H_
