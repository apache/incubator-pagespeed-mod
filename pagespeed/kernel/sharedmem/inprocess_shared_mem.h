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


#ifndef PAGESPEED_KERNEL_SHAREDMEM_INPROCESS_SHARED_MEM_H_
#define PAGESPEED_KERNEL_SHAREDMEM_INPROCESS_SHARED_MEM_H_

#include <cstddef>
#include <map>

#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class MessageHandler;
class ThreadSystem;

// This class emulates the normally cross-process shared memory API
// within a single process on top of threading APIs, in order to permit
// deploying classes built for shared memory into single-process
// servers or tests. Note, however, that a direct implementation taking
// advantage of much simpler in-process programming model may be
// far superior.
class InProcessSharedMem : public AbstractSharedMem {
 public:
  // Does not take ownership of thread_system.
  explicit InProcessSharedMem(ThreadSystem* thread_system);
  virtual ~InProcessSharedMem();

  // All the methods here implement the AbstractSharedMem API ---
  // see the base class for their docs.
  virtual size_t SharedMutexSize() const;
  virtual AbstractSharedMemSegment* CreateSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);
  virtual AbstractSharedMemSegment* AttachToSegment(
      const GoogleString& name, size_t size, MessageHandler* handler);
  virtual void DestroySegment(const GoogleString& name,
                              MessageHandler* handler);

 private:
  class DelegateMutex;
  class DelegateSegment;
  class Segment;
  typedef std::map<GoogleString, Segment*> SegmentMap;

  ThreadSystem* thread_system_;
  SegmentMap segments_;

  DISALLOW_COPY_AND_ASSIGN(InProcessSharedMem);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_SHAREDMEM_INPROCESS_SHARED_MEM_H_
