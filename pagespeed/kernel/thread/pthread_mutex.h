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


#ifndef PAGESPEED_KERNEL_THREAD_PTHREAD_MUTEX_H_
#define PAGESPEED_KERNEL_THREAD_PTHREAD_MUTEX_H_

#include <pthread.h>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/thread_system.h"

namespace net_instaweb {

// Implementation of ThreadSystem::CondvarCapableMutexMutex for Pthread mutexes.
class PthreadMutex : public ThreadSystem::CondvarCapableMutex {
 public:
  PthreadMutex();
  virtual ~PthreadMutex();
  virtual bool TryLock();
  virtual void Lock();
  virtual void Unlock();
  virtual ThreadSystem::Condvar* NewCondvar();

 private:
  friend class PthreadCondvar;

  pthread_mutex_t mutex_;

  DISALLOW_COPY_AND_ASSIGN(PthreadMutex);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_THREAD_PTHREAD_MUTEX_H_
