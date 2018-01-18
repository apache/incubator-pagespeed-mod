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


#include "pagespeed/kernel/sharedmem/shared_circular_buffer.h"

#include <cstddef>

#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/abstract_shared_mem.h"
#include "pagespeed/kernel/base/circular_buffer.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace {
  const char kSharedCircularBufferObjName[] = "SharedCircularBuffer";
}    // namespace

namespace net_instaweb {

SharedCircularBuffer::SharedCircularBuffer(AbstractSharedMem* shm_runtime,
                                           const int buffer_capacity,
                                           const GoogleString& filename_prefix,
                                           const GoogleString& filename_suffix)
    : shm_runtime_(shm_runtime),
      buffer_capacity_(buffer_capacity),
      buffer_(NULL),
      filename_prefix_(filename_prefix),
      filename_suffix_(filename_suffix) {
}

SharedCircularBuffer::~SharedCircularBuffer() {
}

// Initialize shared mutex.
bool SharedCircularBuffer::InitMutex(MessageHandler* handler) {
  if (!segment_->InitializeSharedMutex(0, handler)) {
    handler->Message(
        kError, "Unable to create mutex for shared memory circular buffer");
    return false;
  }
  return true;
}

bool SharedCircularBuffer::InitSegment(bool parent,
                                       MessageHandler* handler) {
  // Size of segment includes mutex and circular buffer.
  int buffer_size = CircularBuffer::Sizeof(buffer_capacity_);
  size_t total = shm_runtime_->SharedMutexSize() + buffer_size;
  if (parent) {
    // In root process -> initialize the shared memory.
    segment_.reset(
        shm_runtime_->CreateSegment(SegmentName(), total, handler));
    if (segment_.get() == NULL) {
      return false;
    }
    // Initialize mutex.
    if (!InitMutex(handler)) {
      segment_.reset(NULL);
      shm_runtime_->DestroySegment(SegmentName(), handler);
      return false;
    }
  } else {
    // In child process -> attach to existing segment.
    segment_.reset(
        shm_runtime_->AttachToSegment(SegmentName(), total, handler));
    if (segment_.get() == NULL) {
      return false;
    }
  }
  // Attach Mutex.
  mutex_.reset(segment_->AttachToSharedMutex(0));
  // Initialize the circular buffer.
  int pos = shm_runtime_->SharedMutexSize();
  buffer_ = CircularBuffer::Init(
                parent,
                static_cast<void*>(const_cast<char*>(segment_->Base() + pos)),
                buffer_size, buffer_capacity_);
  return true;
}

void SharedCircularBuffer::Clear() {
  ScopedMutex hold_lock(mutex_.get());
  buffer_->Clear();
}

bool SharedCircularBuffer::Write(const StringPiece& message,
                                 MessageHandler* handler) {
  ScopedMutex hold_lock(mutex_.get());
  return buffer_->Write(message);
}

bool SharedCircularBuffer::Dump(Writer* writer, MessageHandler* handler) {
  ScopedMutex hold_lock(mutex_.get());
  return (writer->Write(buffer_->ToString(handler), handler));
}

GoogleString SharedCircularBuffer::ToString(MessageHandler* handler) {
  ScopedMutex hold_lock(mutex_.get());
  return buffer_->ToString(handler);
}

void SharedCircularBuffer::GlobalCleanup(MessageHandler* handler) {
  if (segment_.get() != NULL) {
    shm_runtime_->DestroySegment(SegmentName(), handler);
  }
}

GoogleString SharedCircularBuffer::SegmentName() const {
  return StrCat(filename_prefix_, kSharedCircularBufferObjName, ".",
                filename_suffix_);
}

}  // namespace net_instaweb
