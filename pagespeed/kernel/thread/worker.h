/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

//
// This contains Worker, which is a base class for classes managing running
// of work in background.

#ifndef PAGESPEED_KERNEL_THREAD_WORKER_H_
#define PAGESPEED_KERNEL_THREAD_WORKER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class Function;
class ThreadSystem;
class Waveform;

// This class is a base for various mechanisms of running things in background.
//
// If you just want to run something in background, you want to use a subclass
// of this, such as a SlowWorker or QueuedWorker instance.
//
// Subclasses should implement bool PermitQueue() and provide an appropriate
// wrapper around QueueIfPermitted().
class Worker {
 public:
  // Tries to start the work thread (if it hasn't been started already).
  void Start();

  // Returns true if there was a job running or any jobs queued at the time
  // this function was called.
  bool IsBusy();

  // Finishes the currently running jobs, and deletes any queued jobs.
  // No further jobs will be accepted after this call either; they will
  // just be canceled. It is safe to call this method multiple times.
  void ShutDown();

  // Sets up a timed-variable statistic indicating the current queue depth.
  //
  // This must be called prior to starting the thread.
  void set_queue_size_stat(Waveform* x) { queue_size_ = x; }

 protected:
  Worker(StringPiece thread_name, ThreadSystem* runtime);
  virtual ~Worker();

  // If IsPermitted() returns true, queues up the given closure to be run,
  // takes ownership of closure, and returns true. (Also wakes up the work
  // thread to actually run it if it's idle)
  //
  // Otherwise it merely returns false, and doesn't do anything else.
  bool QueueIfPermitted(Function* closure);

  // Subclasses should implement this method to implement the policy
  // on whether to run given tasks or not.
  virtual bool IsPermitted(Function* closure) = 0;

  // Returns the number of jobs, including any running and queued jobs.
  // The lock semantics here are as follows:
  // - QueueIfPermitted calls IsPermitted with lock held.
  // - NumJobs assumes lock to be held.
  // => It's safe to call NumJobs from within IsPermitted if desired.
  int NumJobs();

 private:
  class WorkThread;
  friend class WorkThread;

  // This is called whenever a task is added or removed from the queue.
  void UpdateQueueSizeStat(int size);

  scoped_ptr<WorkThread> thread_;
  Waveform* queue_size_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_WORKER_H_
