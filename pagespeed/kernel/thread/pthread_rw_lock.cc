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


#include "pagespeed/kernel/thread/pthread_rw_lock.h"

#include <pthread.h>

namespace net_instaweb {

PthreadRWLock::PthreadRWLock() {
  pthread_rwlockattr_init(&attr_);
  // POSIX does not provide any sort of guarantee that prevents writer
  // starvation for reader-writer locks. On, Linux one can avoid
  // writer starvation as long as readers are non-recursive via the
  // call below. (PTHREAD_RWLOCK_PREFER_WRITER_NP does not work).
  //
  // Other OS's (FreeBSD, Darwin, OpenSolaris) documentation suggests
  // that they prefer writers by default.
#ifdef linux
  pthread_rwlockattr_setkind_np(&attr_,
                                PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
  pthread_rwlock_init(&rwlock_, &attr_);
}

PthreadRWLock::~PthreadRWLock() {
  pthread_rwlockattr_destroy(&attr_);
  pthread_rwlock_destroy(&rwlock_);
}

bool PthreadRWLock::TryLock() {
  return (pthread_rwlock_trywrlock(&rwlock_) == 0);
}

void PthreadRWLock::Lock() {
  pthread_rwlock_wrlock(&rwlock_);
}

void PthreadRWLock::Unlock() {
  pthread_rwlock_unlock(&rwlock_);
}

bool PthreadRWLock::ReaderTryLock() {
  return (pthread_rwlock_tryrdlock(&rwlock_) == 0);
}

void PthreadRWLock::ReaderLock() {
  pthread_rwlock_rdlock(&rwlock_);
}

void PthreadRWLock::ReaderUnlock() {
  pthread_rwlock_unlock(&rwlock_);
}

}  // namespace net_instaweb
