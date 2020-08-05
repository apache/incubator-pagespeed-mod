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

#include "test/pagespeed/kernel/base/null_thread_system.h"

#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/timer.h"
#include "test/pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

namespace {

// Mock read-write-lock.  This does no locking.
class NullRWLock : public ThreadSystem::RWLock {
 public:
  NullRWLock() {}
  ~NullRWLock() override;
  bool ReaderTryLock() override { return true; }
  void ReaderLock() override {}
  void ReaderUnlock() override {}
  bool TryLock() override { return true; }
  void Lock() override {}
  void Unlock() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NullRWLock);
};

class NullThreadId : public ThreadSystem::ThreadId {
 public:
  explicit NullThreadId(const NullThreadSystem* system)
      : id_(system->current_thread()), system_(system) {}

  ~NullThreadId() override {}

  bool IsEqual(const ThreadId& that) const override {
    return (id_ == dynamic_cast<const NullThreadId&>(that).id_);
  }

  bool IsCurrentThread() const override {
    return id_ == system_->current_thread();
  }

 private:
  int id_;
  const NullThreadSystem* system_;

  DISALLOW_COPY_AND_ASSIGN(NullThreadId);
};

}  // namespace

NullCondvarCapableMutex::~NullCondvarCapableMutex() {}

NullRWLock::~NullRWLock() {}

NullThreadSystem::~NullThreadSystem() {}

NullCondvarCapableMutex* NullThreadSystem::NewMutex() {
  return new NullCondvarCapableMutex();
}

ThreadSystem::RWLock* NullThreadSystem::NewRWLock() { return new NullRWLock; }

Timer* NullThreadSystem::NewTimer() {
  // TODO(jmarantz): consider removing the responsibility of creating timers
  // from the thread system.
  return new MockTimer(new NullMutex, 0);
}

ThreadSystem::ThreadId* NullThreadSystem::GetThreadId() const {
  return new NullThreadId(this);
}

ThreadSystem::ThreadImpl* NullThreadSystem::NewThreadImpl(Thread* wrapper,
                                                          ThreadFlags flags) {
  LOG(FATAL) << "Creating threads in null thread system not supported";
  return nullptr;
}

NullCondvar::~NullCondvar() {
  // All actions should have been examined by the caller.
  if (!actions_.empty()) {
    LOG(FATAL) << "actions_ not empty: " << JoinCollection(actions_, " ");
  }
  // If caller set a callback for TimedWait() then they should also have
  // called TimedWait().
  CHECK(timed_wait_callback_ == nullptr);
}

void NullCondvar::TimedWait(int64 timeout_ms) {
  actions_.push_back(StrCat("TimedWait(", IntegerToString(timeout_ms), ")"));
  if (timed_wait_callback_ != nullptr) {
    timed_wait_callback_->Call();
    timed_wait_callback_ = nullptr;
  }
}

GoogleString NullCondvar::ActionsSinceLastCall() {
  GoogleString response = JoinCollection(actions_, " ");
  actions_.clear();
  return response;
}

NullCondvar* NullCondvarCapableMutex::NewCondvar() {
  return new NullCondvar(this);
}

}  // namespace net_instaweb
