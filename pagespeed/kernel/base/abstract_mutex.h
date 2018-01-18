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


#ifndef PAGESPEED_KERNEL_BASE_ABSTRACT_MUTEX_H_
#define PAGESPEED_KERNEL_BASE_ABSTRACT_MUTEX_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/thread_annotations.h"

namespace net_instaweb {

// Abstract interface for implementing a mutex.
class LOCKABLE AbstractMutex {
 public:
  virtual ~AbstractMutex();
  // Attempt to take mutex, true on success, false if held by another thread.
  virtual bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true) = 0;
  // Block until mutex is available, then take the mutex.
  virtual void Lock() EXCLUSIVE_LOCK_FUNCTION() = 0;
  // Release the mutex, permitting a blocked lock operation (if any) to proceed.
  virtual void Unlock() UNLOCK_FUNCTION() = 0;
  // Optionally checks that lock is held (for invariant checking purposes).
  // Default implementation does no checking.
  virtual void DCheckLocked();
  // Optionally checks that lock is not held (for invariant checking purposes).
  // Default implementation does no checking.
  virtual void DCheckUnlocked();
};

// Helper class for lexically scoped mutexing.
class SCOPED_LOCKABLE ScopedMutex {
 public:
  explicit ScopedMutex(AbstractMutex* mutex) EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex_->Lock();
  }

  void Release() UNLOCK_FUNCTION() {
    if (mutex_ != NULL) {
      mutex_->Unlock();
      mutex_ = NULL;
    }
  }

  ~ScopedMutex() UNLOCK_FUNCTION() { Release(); }

 private:
  AbstractMutex* mutex_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMutex);
};
// Catch bug where variable name is omitted with ScopedMutex, e.g.
// ScopedMutex(&mu);
#define ScopedMutex(x) COMPILE_ASSERT(0, mutex_lock_decl_missing_var_name)

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ABSTRACT_MUTEX_H_
