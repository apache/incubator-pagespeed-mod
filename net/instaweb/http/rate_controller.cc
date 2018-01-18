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


#include "net/instaweb/http/public/rate_controller.h"

#include <cstddef>
#include <queue>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_annotations.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

// Keeps track of the objects required while deferring a fetch.
struct DeferredFetch {
  DeferredFetch(const GoogleString& in_url,
                UrlAsyncFetcher* fetcher,
                AsyncFetch* in_fetch,
                MessageHandler* in_handler)
      : url(in_url),
        fetcher(fetcher),
        fetch(in_fetch),
        handler(in_handler) {}

  GoogleString url;
  UrlAsyncFetcher* fetcher;
  AsyncFetch* fetch;
  MessageHandler* handler;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeferredFetch);
};

}  // namespace

const char RateController::kQueuedFetchCount[] =
    "queued-fetch-count";
const char RateController::kDroppedFetchCount[] =
    "dropped-fetch-count";
const char RateController::kCurrentGlobalFetchQueueSize[] =
    "current-fetch-queue-size";

// Keeps track of all the pending and enqueued fetches for a given host.
class RateController::HostFetchInfo
    : public RefCounted<RateController::HostFetchInfo> {
 public:
  // Takes ownership of the mutex passed in.
  HostFetchInfo(const GoogleString& host,
                int per_host_outgoing_request_threshold,
                int per_host_queued_request_threshold,
                AbstractMutex* mutex)
      : host_(host),
        num_outbound_fetches_(0),
        per_host_outgoing_request_threshold_(
            per_host_outgoing_request_threshold),
        per_host_queued_request_threshold_(per_host_queued_request_threshold),
        mutex_(mutex) {}

  ~HostFetchInfo() {}

  // Returns the number of outbound fetches for the given host.
  int num_outbound_fetches() LOCKS_EXCLUDED(mutex_) {
    ScopedMutex lock(mutex_.get());
    return num_outbound_fetches_;
  }

  // Checks if the number of outbound fetches is less than the threshold. If so,
  // increments the number of outbound fetches and returns true. Returns false
  // otherwise.
  bool IncrementIfCanTriggerFetch() EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (num_outbound_fetches_ < per_host_outgoing_request_threshold_) {
      ++num_outbound_fetches_;
      return true;
    }
    return false;
  }

  // Decreases the number of outbound fetches by 1.
  void decrement_num_outbound_fetches() LOCKS_EXCLUDED(mutex_) {
    ScopedMutex lock(mutex_.get());
    DCHECK_GT(num_outbound_fetches_, 0);
    --num_outbound_fetches_;
  }

  // Increases the number of outbound fetches by 1.
  void increment_num_outbound_fetches() EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    DCHECK_GE(num_outbound_fetches_, 0);
    ++num_outbound_fetches_;
  }
  // Pushes the fetch to the back of the queue.
  bool EnqueueFetchIfWithinThreshold(const GoogleString& url,
                                     UrlAsyncFetcher* fetcher,
                                     MessageHandler* handler,
                                     AsyncFetch* fetch)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (fetch_queue_.size() <
        static_cast<size_t>(per_host_queued_request_threshold_)) {
      fetch_queue_.push(new DeferredFetch(url, fetcher, fetch, handler));
      return true;
    }
    return false;
  }

  // Gets the next fetch from the queue. Returns NULL if the queue is empty.
  DeferredFetch* PopNextFetchAndIncrementCountIfWithinThreshold()
      LOCKS_EXCLUDED(mutex_) {
    ScopedMutex lock(mutex_.get());
    if (fetch_queue_.empty() ||
        num_outbound_fetches_ >= per_host_outgoing_request_threshold_) {
      return NULL;
    }
    DeferredFetch* fetch = fetch_queue_.front();
    fetch_queue_.pop();
    ++num_outbound_fetches_;
    return fetch;
  }

  // Returns the host associated with this HostFetchInfo object.
  const GoogleString& host() { return host_; }

  bool AnyInFlightOrQueuedFetches() const LOCKS_EXCLUDED(mutex_) {
    ScopedMutex lock(mutex_.get());
    DCHECK_GE(num_outbound_fetches_, 0);
    return num_outbound_fetches_ > 0 || !fetch_queue_.empty();
  }

  void Lock() EXCLUSIVE_LOCK_FUNCTION(mutex_) {
    mutex_->Lock();
  }

  void Unlock() UNLOCK_FUNCTION(mutex_) {
    mutex_->Unlock();
  }

 private:
  GoogleString host_;
  int num_outbound_fetches_ GUARDED_BY(mutex_);
  const int per_host_outgoing_request_threshold_;
  const int per_host_queued_request_threshold_;
  scoped_ptr<AbstractMutex> mutex_;
  std::queue<DeferredFetch*> fetch_queue_ GUARDED_BY(mutex_);

  DISALLOW_COPY_AND_ASSIGN(HostFetchInfo);
};

// Wrapper fetch that updates the count of outgoing fetches for the host when
// completed. It also triggers a fetch for any other pending requests for the
// domain.
class RateController::CustomFetch : public SharedAsyncFetch {
 public:
  CustomFetch(const HostFetchInfoPtr& fetch_info,
              AsyncFetch* fetch,
              RateController* controller)
      : SharedAsyncFetch(fetch),
        fetch_info_(fetch_info),
        controller_(controller) {}

  virtual void HandleDone(bool success) {
    SharedAsyncFetch::HandleDone(success);
    fetch_info_->decrement_num_outbound_fetches();
    // Check if there is any fetch queued up for this host and the number of
    // outstanding fetches for the host is less than the threshold.
    DeferredFetch* deferred_fetch =
        fetch_info_->PopNextFetchAndIncrementCountIfWithinThreshold();
    if (deferred_fetch != NULL) {
      DCHECK_GT(controller_->current_global_fetch_queue_size_->Get(), 0);
      controller_->current_global_fetch_queue_size_->Add(-1);
      // Trigger a fetch for the queued up request.
      CustomFetch* wrapper_fetch = new CustomFetch(
          fetch_info_, deferred_fetch->fetch, controller_);

      if (controller_->is_shut_down()) {
        deferred_fetch->handler->Message(
            kWarning, "RateController: drop deferred fetch of %s on shutdown",
            deferred_fetch->url.c_str());
        wrapper_fetch->Done(false);
      } else {
        deferred_fetch->fetcher->Fetch(deferred_fetch->url,
                                       deferred_fetch->handler,
                                       wrapper_fetch);
      }
      delete deferred_fetch;
    } else {
      controller_->DeleteFetchInfoIfPossible(fetch_info_);
    }
    delete this;
  }

 private:
  HostFetchInfoPtr fetch_info_;
  RateController* controller_;
  DISALLOW_COPY_AND_ASSIGN(CustomFetch);
};

RateController::RateController(
    int max_global_queue_size,
    int per_host_outgoing_request_threshold,
    int per_host_queued_request_threshold,
    ThreadSystem* thread_system,
    Statistics* statistics)
    : max_global_queue_size_(max_global_queue_size),
      per_host_outgoing_request_threshold_(
          per_host_outgoing_request_threshold),
      per_host_queued_request_threshold_(per_host_queued_request_threshold),
      thread_system_(thread_system),
      mutex_(thread_system->NewMutex()) {
  CHECK_GE(max_global_queue_size, 0);
  CHECK_GE(per_host_outgoing_request_threshold, 0);
  CHECK_GE(per_host_queued_request_threshold, 0);
  CHECK_GE(max_global_queue_size, per_host_queued_request_threshold);
  queued_fetch_count_ = statistics->GetTimedVariable(kQueuedFetchCount);
  dropped_fetch_count_ = statistics->GetTimedVariable(kDroppedFetchCount);
  current_global_fetch_queue_size_ = statistics->GetUpDownCounter(
      kCurrentGlobalFetchQueueSize);
}

RateController::~RateController() {
}

void RateController::Fetch(UrlAsyncFetcher* fetcher,
                           const GoogleString& url,
                           MessageHandler* message_handler,
                           AsyncFetch* fetch) {
  if (is_shut_down()) {
    message_handler->Message(
        kWarning, "RateController: drop fetch of %s on shutdown",
        url.c_str());
    fetch->Done(false);
    return;
  }

  GoogleUrl gurl(url);
  GoogleString host;
  if (gurl.IsWebValid()) {
    host = gurl.Host().as_string();
    LowerString(&host);
  } else {
    // TODO(nikhilmadan): We should ideally just be dropping this fetch, but for
    // now we just hand it off to the base fetcher.
    return fetcher->Fetch(url, message_handler, fetch);
  }

  HostFetchInfoPtr fetch_info_ptr;
  // Lookup the map for the fetch info associated with the given host. Note that
  // it would have been nice to avoid acquiring the mutex for user-facing
  // requests, but we need to lookup the fetch info in order to update the
  // number of outgoing requests. The mutex must also be held until we update
  // the pending request counts, since otherwise we may race against deletion
  // of the map entry and HostFetchInfo in a call to DeleteFetchInfoIfPossible
  // from a completion of a queued fetch.
  mutex_->Lock();

  HostFetchInfoMap::iterator iter = fetch_info_map_.find(host);
  if (iter != fetch_info_map_.end()) {
    fetch_info_ptr = *iter->second;
  } else {
    // Insert a new entry if there wasn't one already.
    HostFetchInfoPtr* new_fetch_info_ptr = new HostFetchInfoPtr(
        new HostFetchInfo(host, per_host_outgoing_request_threshold_,
                          per_host_queued_request_threshold_,
                          thread_system_->NewMutex()));
    fetch_info_ptr = *new_fetch_info_ptr;
    fetch_info_map_[host] = new_fetch_info_ptr;
  }

  fetch_info_ptr->Lock();

  if (!fetch->IsBackgroundFetch() ||
      fetch_info_ptr->IncrementIfCanTriggerFetch()) {
    // If this is a user-facing fetch or the number of outgoing fetches is
    // within the per-host threshold, trigger the fetch immediately.
    if (!fetch->IsBackgroundFetch()) {
      // Increment the count if the request is not a background fetch.
      fetch_info_ptr->increment_num_outbound_fetches();
    }
    fetch_info_ptr->Unlock();
    mutex_->Unlock();
    CustomFetch* wrapper_fetch = new CustomFetch(fetch_info_ptr, fetch, this);
    return fetcher->Fetch(url, message_handler, wrapper_fetch);
  } else if (current_global_fetch_queue_size_->Get() < max_global_queue_size_ &&
             fetch_info_ptr->EnqueueFetchIfWithinThreshold(
                 url, fetcher, message_handler, fetch)) {
    // If the number of globally queued up fetches is within the threshold and
    // the number of queued requests for this host is less than the threshold,
    // push it to the back of the per-host queue.
    // Note that we want to increase the queue size while still holding the
    // fetch_info_ptr lock, since otherwise the entry may get dequeued
    // with the size stat not yet updated, confusing us about it being 0.
    current_global_fetch_queue_size_->Add(1);
    fetch_info_ptr->Unlock();
    mutex_->Unlock();
    queued_fetch_count_->IncBy(1);
    return;
  }

  fetch_info_ptr->Unlock();
  mutex_->Unlock();

  dropped_fetch_count_->IncBy(1);
  message_handler->Message(kInfo, "Dropping request for %s", url.c_str());
  fetch->response_headers()->Add(HttpAttributes::kXPsaLoadShed, "1");
  fetch->Done(false);
  DeleteFetchInfoIfPossible(fetch_info_ptr);
  return;
}

void RateController::InitStats(Statistics* statistics) {
  statistics->AddUpDownCounter(kCurrentGlobalFetchQueueSize);
  statistics->AddTimedVariable(kQueuedFetchCount,
                               Statistics::kDefaultGroup);
  statistics->AddTimedVariable(kDroppedFetchCount,
                               Statistics::kDefaultGroup);
}

void RateController::DeleteFetchInfoIfPossible(
    const HostFetchInfoPtr& fetch_info) {
  ScopedMutex lock(mutex_.get());
  if (fetch_info->AnyInFlightOrQueuedFetches()) {
    return;
  }

  HostFetchInfoMap::iterator iter = fetch_info_map_.find(fetch_info->host());
  if (iter != fetch_info_map_.end()) {
    delete iter->second;
    fetch_info_map_.erase(iter);
  }
}

}  // namespace net_instaweb
