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


#include "pagespeed/kernel/thread/scheduler_thread.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/thread/scheduler.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

// Helper returned by Scheduler::MakeDeleter, which signals the thread
// to exit and joins on it.
class SchedulerThread::CleanupFunction : public Function {
 public:
  explicit CleanupFunction(SchedulerThread* parent) : parent_(parent) {}
  virtual ~CleanupFunction() {}

 protected:
  virtual void Run() {
    {
      ScopedMutex lock(parent_->scheduler_->mutex());
      parent_->quit_ = true;
      parent_->scheduler_->Signal();
    }
    parent_->Join();
    delete parent_;
  }

  virtual void Cancel() {
    LOG(DFATAL) << "CleanupFunction does not expect to be cancelled";
  }

 private:
  SchedulerThread* parent_;
  DISALLOW_COPY_AND_ASSIGN(CleanupFunction);
};

SchedulerThread::SchedulerThread(ThreadSystem* thread_system,
                                 Scheduler* scheduler)
    : Thread(thread_system, "scheduler_thread", ThreadSystem::kJoinable),
      quit_(false),
      scheduler_(scheduler) {}

SchedulerThread::~SchedulerThread() {}

Function* SchedulerThread::MakeDeleter() {
  return new CleanupFunction(this);
}

void SchedulerThread::Run() {
  ScopedMutex lock(scheduler_->mutex());
  while (!quit_) {
    scheduler_->ProcessAlarmsOrWaitUs(255 * Timer::kSecondUs);
  }
}

}  // namespace net_instaweb
