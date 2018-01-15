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


#ifndef PAGESPEED_KERNEL_BASE_ABSTRACT_SHARED_MEM_H_
#define PAGESPEED_KERNEL_BASE_ABSTRACT_SHARED_MEM_H_

#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class AbstractMutex;
class MessageHandler;

// This represents a region of memory shared between between multiple processes
// that may contain mutexes.
class AbstractSharedMemSegment {
 public:
  AbstractSharedMemSegment() {}

  // Destroying the segment object detaches from it, making all pointers into it
  // invalid.
  virtual ~AbstractSharedMemSegment();

  // Returns the base address of the segment. Note that there is no guarantee
  // that this address will be the same for other processes attached to the
  // same segment.
  virtual volatile char* Base() = 0;

  // Returns the number of bytes a mutex inside shared memory takes.
  virtual size_t SharedMutexSize() const = 0;

  // To use a mutex in shared memory, you first need to dedicate some
  // [offset, offset + SharedMutexSize()) chunk of memory to it. Then,
  // exactly one process must call InitializeSharedMutex(offset), and
  // all users must call AttachToSharedMutex(offset) afterwards.
  //
  // InitializeSharedMutex returns whether it succeeded or not.
  // AttachToSharedMutex returns a fresh object, giving ownership
  // to the caller. The object returned is outside shared memory,
  // and acts a helper for referring to the shared state.
  virtual bool InitializeSharedMutex(size_t offset,
                                     MessageHandler* handler) = 0;
  virtual AbstractMutex* AttachToSharedMutex(size_t offset) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractSharedMemSegment);
};

// Interface for creating and attaching to named shared memory segments.
// The expectation is that whichever implementation is used at runtime
// will be able to handle the combination of threads & processes used by
// the hosting environment.
//
// The basic flow here is as follows:
//
//            Single process/thread startup stage:
//            CreateSegment
//            InitializeSharedMutex -----+
//           /                           |
//          /                            |
//    process/thread:                   process/thread:
//    AttachToSegment                   AttachToSegment
//    AttachToSharedMutex               AttachToSharedMutex
//       |                                     |
//       |                                     |
//       |------------------------------------/
//       |
//    single process/thread cleanup stage:
//    DestroySegment
//
class AbstractSharedMem {
 public:
  AbstractSharedMem() {}
  virtual ~AbstractSharedMem();

  // Size of mutexes inside shared memory segments.
  virtual size_t SharedMutexSize() const = 0;

  // This should be called upon main process/thread initialization to create
  // a shared memory segment that will be accessed by other processes/threads
  // as identified by a unique name (via AttachToSegment). It will remove
  // any previous segment with the same name. The memory will be zeroed out.
  //
  // May return NULL on failure.
  virtual AbstractSharedMemSegment* CreateSegment(
      const GoogleString& name, size_t size, MessageHandler* handler) = 0;

  // Attaches to an existing segment, which must have been created already.
  // May return NULL on failure
  virtual AbstractSharedMemSegment* AttachToSegment(
      const GoogleString& name, size_t size, MessageHandler* handler) = 0;

  // Cleans up the segment with given name. You should call this after there is
  // no longer any need for AttachToSegment to succeed.
  virtual void DestroySegment(const GoogleString& name,
                              MessageHandler* handler) = 0;

  // Implementors such as NullSharedMem that don't actually support shared
  // memory operations should return true.  All real implementations should
  // leave this as false.
  virtual bool IsDummy() { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AbstractSharedMem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ABSTRACT_SHARED_MEM_H_
