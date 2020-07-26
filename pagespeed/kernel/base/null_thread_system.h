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
// Zero-dependency mock thread-system for use in tests that don't
// actually use threads, to help test classes that need some mutexing
// or other thread-safety hooks.
//
// Note that this thread-system does not currently make threads (even
// co-routines), but check-fails if you attempt to spawn a new thread.

#ifndef PAGESPEED_KERNEL_BASE_NULL_THREAD_SYSTEM_H_
#define PAGESPEED_KERNEL_BASE_NULL_THREAD_SYSTEM_H_

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

class Timer;

class NullCondvar : public ThreadSystem::Condvar {
 public:
  explicit NullCondvar(ThreadSystem::CondvarCapableMutex* m)
      : mutex_(m), timed_wait_callback_(NULL) {}
  ~NullCondvar() override;
  ThreadSystem::CondvarCapableMutex* mutex() const override { return mutex_; }
  void Signal() override { actions_.push_back("Signal()"); }
  void Broadcast() override { actions_.push_back("Broadcast()"); }
  void Wait() override { actions_.push_back("Wait()"); }
  void TimedWait(int64 timeout_ms) override;
  GoogleString ActionsSinceLastCall();

  class TimedWaitCallback {
   public:
    virtual ~TimedWaitCallback() {}
    virtual void Call() = 0;
  };

  // Calls callback once the next time TimedWait() is called.  If TimedWait() is
  // not called we will CHECK-fail.  Doesn't take ownership of the callback.
  void set_timed_wait_callback(TimedWaitCallback* x) {
    CHECK(timed_wait_callback_ == NULL);
    timed_wait_callback_ = x;
  }

 private:
  ThreadSystem::CondvarCapableMutex* mutex_;
  StringVector actions_;
  TimedWaitCallback* timed_wait_callback_;

  DISALLOW_COPY_AND_ASSIGN(NullCondvar);
};

// Mock condvar-capable mutex.  Note that this does no actual locking,
// and any condvars it creates are mocks.
class NullCondvarCapableMutex : public ThreadSystem::CondvarCapableMutex {
 public:
  NullCondvarCapableMutex() {}
  ~NullCondvarCapableMutex() override;
  bool TryLock() override { return true; }
  void Lock() override {}
  void Unlock() override {}
  NullCondvar* NewCondvar() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullCondvarCapableMutex);
};

// Mock thread system.  This can create mutexes that do no locking, condvars
// that do no waiting, and can't create threads.  Trying to create a thread will
// result in a fatal error.
class NullThreadSystem : public ThreadSystem {
 public:
  NullThreadSystem() : thread_id_(1) {}
  ~NullThreadSystem() override;
  NullCondvarCapableMutex* NewMutex() override;
  RWLock* NewRWLock() override;
  Timer* NewTimer() override;
  ThreadId* GetThreadId() const override;

  // Provide injection/observation of current thread IDs.
  void set_current_thread(int id) { thread_id_ = id; }
  int current_thread() const { return thread_id_; }

 private:
  ThreadImpl* NewThreadImpl(Thread* wrapper, ThreadFlags flags) override;

 private:
  int thread_id_;

  DISALLOW_COPY_AND_ASSIGN(NullThreadSystem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NULL_THREAD_SYSTEM_H_
