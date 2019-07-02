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
// This contains QueuedWorker, which runs tasks in a background thread
// in FIFO order.

#ifndef PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_H_
#define PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/thread/worker.h"

namespace net_instaweb {

class Function;

// See file comment.
class QueuedWorker : public Worker {
 public:
  // Initializes the worker. You still need to call ->Start to actually
  // start the thread, however. (Note: start can return false on failure).
  QueuedWorker(StringPiece thread_name, ThreadSystem* runtime);

  // This waits for the running task to terminate.
  virtual ~QueuedWorker();

  // Runs the given closure in the work thread. Not that it's possible for the
  // closure to be deleted without running in case where the system is shutting
  // down.
  //
  // Takes ownership of the closure.
  void RunInWorkThread(Function* closure);

 private:
  virtual bool IsPermitted(Function* closure);

  DISALLOW_COPY_AND_ASSIGN(QueuedWorker);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_QUEUED_WORKER_H_
