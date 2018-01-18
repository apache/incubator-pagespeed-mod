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


#include "pagespeed/kernel/base/checking_thread_system.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/atomic_bool.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

class Timer;

// Checked condvar class.  Must only be created via a
// CheckingThreadSystem::Mutex, thus its implementation is kept private.
class CheckingThreadSystem::CheckingCondvar : public ThreadSystem::Condvar {
 public:
  CheckingCondvar(CheckingThreadSystem::Mutex* mutex,
          ThreadSystem::Condvar* condvar)
      : mutex_(mutex), condvar_(condvar) { }
  virtual ~CheckingCondvar() { }
  virtual CheckingThreadSystem::Mutex* mutex() const {
    return mutex_;
  }
  virtual void Signal() {
    condvar_->Signal();
  }
  virtual void Broadcast() {
    condvar_->Broadcast();
  }
  virtual void Wait() {
    mutex_->DropLockControl();
    condvar_->Wait();
    mutex_->TakeLockControl();
  }
  virtual void TimedWait(int64 timeout_ms) {
    mutex_->DropLockControl();
    condvar_->TimedWait(timeout_ms);
    mutex_->TakeLockControl();
  }

 private:
  CheckingThreadSystem::Mutex* mutex_;
  scoped_ptr<ThreadSystem::Condvar> condvar_;
  DISALLOW_COPY_AND_ASSIGN(CheckingCondvar);
};

// Destructor and methods for CheckingThreadSystem::Mutex

CheckingThreadSystem::Mutex::~Mutex() {
  CHECK(!locked_.value()) << "Lock should not be held on destruction.";
}

void CheckingThreadSystem::Mutex::DCheckLocked() {
  CHECK(locked_.value()) << "Lock should have been held.";
}

void CheckingThreadSystem::Mutex::DCheckUnlocked() {
  CHECK(!locked_.value()) << "Lock should not have been held.";
}

void CheckingThreadSystem::Mutex::DropLockControl() {
  DCheckLocked();
  locked_.set_value(false);
}

void CheckingThreadSystem::Mutex::TakeLockControl() {
  CHECK(!locked_.value()) << "Lock should have been available.";
  locked_.set_value(true);
}

bool CheckingThreadSystem::Mutex::TryLock() {
  bool locked = mutex_->TryLock();
  if (locked) {
    TakeLockControl();
  }
  return locked;
}

void CheckingThreadSystem::Mutex::Lock() {
  mutex_->Lock();
  TakeLockControl();
}

void CheckingThreadSystem::Mutex::Unlock() {
  DropLockControl();
  mutex_->Unlock();
}

ThreadSystem::Condvar* CheckingThreadSystem::Mutex::NewCondvar() {
  ThreadSystem::Condvar* enclosed = mutex_->NewCondvar();
  return new CheckingThreadSystem::CheckingCondvar(this, enclosed);
}

// Destructor and methods for CheckingThreadSystem::RWLock

CheckingThreadSystem::RWLock::~RWLock() {
  CHECK_EQ(locked_.value(), 0) << "Lock should not be held on destruction.";
}

void CheckingThreadSystem::RWLock::DCheckLocked() {
  CHECK_EQ(locked_.value(), -1) << "Lock should have been held.";
}

void CheckingThreadSystem::RWLock::DCheckReaderLocked() {
  CHECK_GT(locked_.value(), 0) << "Lock should have been held.";
}

void CheckingThreadSystem::RWLock::DropLockControl() {
  DCheckLocked();
  locked_.set_value(0);
}

void CheckingThreadSystem::RWLock::TakeLockControl() {
  CHECK_EQ(locked_.value(), 0) << "Lock should have been available.";
  locked_.set_value(-1);
}

void CheckingThreadSystem::RWLock::DropReaderLockControl() {
  DCheckReaderLocked();
  locked_.NoBarrierIncrement(-1);
}

void CheckingThreadSystem::RWLock::TakeReaderLockControl() {
  CHECK_GE(locked_.value(), 0) << "Lock should have been available.";
  locked_.NoBarrierIncrement(1);
}

bool CheckingThreadSystem::RWLock::TryLock() {
  bool locked = lock_->TryLock();
  if (locked) {
    TakeLockControl();
  }
  return locked;
}

void CheckingThreadSystem::RWLock::Lock() {
  lock_->Lock();
  TakeLockControl();
}

void CheckingThreadSystem::RWLock::Unlock() {
  DropLockControl();
  lock_->Unlock();
}

bool CheckingThreadSystem::RWLock::ReaderTryLock() {
  bool locked = lock_->ReaderTryLock();
  if (locked) {
    TakeReaderLockControl();
  }
  return locked;
}

void CheckingThreadSystem::RWLock::ReaderLock() {
  lock_->ReaderLock();
  TakeReaderLockControl();
}

void CheckingThreadSystem::RWLock::ReaderUnlock() {
  DropReaderLockControl();
  lock_->ReaderUnlock();
}

// Destructor and methods for CheckingThreadSystem

CheckingThreadSystem::~CheckingThreadSystem() { }

CheckingThreadSystem::Mutex* CheckingThreadSystem::NewMutex() {
  return new Mutex(thread_system_->NewMutex());
}

CheckingThreadSystem::RWLock* CheckingThreadSystem::NewRWLock() {
  return new RWLock(thread_system_->NewRWLock());
}

ThreadSystem::ThreadImpl* CheckingThreadSystem::NewThreadImpl(
    ThreadSystem::Thread* wrapper, ThreadSystem::ThreadFlags flags) {
  return thread_system_->NewThreadImpl(wrapper, flags);
}

Timer* CheckingThreadSystem::NewTimer() {
  return thread_system_->NewTimer();
}

}  // namespace net_instaweb
