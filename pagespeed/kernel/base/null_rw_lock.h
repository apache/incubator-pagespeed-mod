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


#ifndef PAGESPEED_KERNEL_BASE_NULL_RW_LOCK_H_
#define PAGESPEED_KERNEL_BASE_NULL_RW_LOCK_H_

#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implements an empty mutex for single-threaded programs that need to work
// with interfaces that require mutexes.
class LOCKABLE NullRWLock : public ThreadSystem::RWLock {
 public:
  NullRWLock() {}
  virtual ~NullRWLock();
  virtual bool TryLock() EXCLUSIVE_TRYLOCK_FUNCTION(true);
  virtual void Lock() EXCLUSIVE_LOCK_FUNCTION();
  virtual void Unlock() UNLOCK_FUNCTION();
  virtual bool ReaderTryLock() SHARED_TRYLOCK_FUNCTION(true);
  virtual void ReaderLock() SHARED_LOCK_FUNCTION();
  virtual void ReaderUnlock() UNLOCK_FUNCTION();
  virtual void DCheckReaderLocked();
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_NULL_RW_LOCK_H_
