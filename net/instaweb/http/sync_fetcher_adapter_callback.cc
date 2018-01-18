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


//         lsong@google.com (Libo Song)

#include "net/instaweb/http/public/sync_fetcher_adapter_callback.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

class MessageHandler;

bool SyncFetcherAdapterCallback::ProtectedWriter::Write(
    const StringPiece& buf, MessageHandler* handler) {
  bool ret = true;

  // If the callback has not timed out and been released, then pass
  // the data through.
  if (callback_->LockIfNotReleased()) {
    ret = orig_writer_->Write(buf, handler);
    callback_->Unlock();
  }
  return ret;
}

bool SyncFetcherAdapterCallback::ProtectedWriter::Flush(
    MessageHandler* handler) {
  bool ret = true;

  // If the callback has not timed out and been released, then pass
  // the flush through.
  if (callback_->LockIfNotReleased()) {
    ret = orig_writer_->Flush(handler);
    callback_->Unlock();
  }
  return ret;
}

SyncFetcherAdapterCallback::SyncFetcherAdapterCallback(
    ThreadSystem* thread_system, Writer* writer,
    const RequestContextPtr& request_context)
    : AsyncFetch(request_context),
      mutex_(thread_system->NewMutex()),
      cond_(mutex_->NewCondvar()),
      done_(false),
      success_(false),
      released_(false),
      writer_(new ProtectedWriter(this, writer)) {
}

SyncFetcherAdapterCallback::~SyncFetcherAdapterCallback() {
}

void SyncFetcherAdapterCallback::HandleDone(bool success) {
  mutex_->Lock();
  done_ = true;
  success_ = success;
  if (released_) {
    mutex_->Unlock();
    delete this;
  } else {
    cond_->Signal();
    mutex_->Unlock();
  }
}

void SyncFetcherAdapterCallback::Release() {
  mutex_->Lock();
  DCHECK(!released_);
  released_ = true;
  if (done_) {
    mutex_->Unlock();
    delete this;
  } else {
    mutex_->Unlock();
  }
}

bool SyncFetcherAdapterCallback::IsDone() const {
  ScopedMutex hold_lock(mutex_.get());
  return done_;
}

bool SyncFetcherAdapterCallback::IsDoneLockHeld() const {
  mutex_->DCheckLocked();
  return done_;
}

bool SyncFetcherAdapterCallback::success() const {
  ScopedMutex hold_lock(mutex_.get());
  return success_;
}

bool SyncFetcherAdapterCallback::released() const {
  ScopedMutex hold_lock(mutex_.get());
  return released_;
}

bool SyncFetcherAdapterCallback::LockIfNotReleased() {
  mutex_->Lock();
  if (!released_) {
    return true;
  } else {
    mutex_->Unlock();
    return false;
  }
}

void SyncFetcherAdapterCallback::Unlock() {
  mutex_->Unlock();
}

void SyncFetcherAdapterCallback::TimedWait(int64 timeout_ms) {
  mutex_->DCheckLocked();
  DCHECK(!released_);
  cond_->TimedWait(timeout_ms);
}

}  // namespace net_instaweb
