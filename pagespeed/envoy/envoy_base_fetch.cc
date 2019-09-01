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

#include <unistd.h> //for usleep

#include "pagespeed/envoy/envoy_base_fetch.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

int EnvoyBaseFetch::active_base_fetches = 0;

EnvoyBaseFetch::EnvoyBaseFetch(StringPiece url,
                           EnvoyServerContext* server_context,
                           const RequestContextPtr& request_ctx,
                           PreserveCachingHeaders preserve_caching_headers,
                           EnvoyBaseFetchType base_fetch_type,
                           const RewriteOptions* options)
    : AsyncFetch(request_ctx),
      url_(url.data(), url.size()),
      server_context_(server_context),
      options_(options),
      need_flush_(false),
      done_called_(false),
      last_buf_sent_(false),
      references_(2),
      base_fetch_type_(base_fetch_type),
      preserve_caching_headers_(preserve_caching_headers),
      detached_(false),
      suppress_(false) {
  if (pthread_mutex_init(&mutex_, NULL)) CHECK(0);
  __sync_add_and_fetch(&EnvoyBaseFetch::active_base_fetches, 1);
}

EnvoyBaseFetch::~EnvoyBaseFetch() {
  pthread_mutex_destroy(&mutex_);
  __sync_add_and_fetch(&EnvoyBaseFetch::active_base_fetches, -1);
}


const char* BaseFetchTypeToCStr(EnvoyBaseFetchType type) {
  switch(type) {
    case kPageSpeedResource:
      return "ps resource";
    case kHtmlTransform:
      return "html transform";
    case kAdminPage:
      return "admin page";
    case kIproLookup:
      return "ipro lookup";
    case kPageSpeedProxy:
      return "pagespeed proxy";
  }
  CHECK(false);
  return "can't get here";
}

void EnvoyBaseFetch::Lock() {
  pthread_mutex_lock(&mutex_);
}

void EnvoyBaseFetch::Unlock() {
  pthread_mutex_unlock(&mutex_);
}

bool EnvoyBaseFetch::HandleWrite(const StringPiece& sp,
                               MessageHandler* handler) {
  Lock();
  buffer_.append(sp.data(), sp.size());
  Unlock();
  return true;
}


void EnvoyBaseFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  bool status_ok = (status_code != 0) && (status_code < 400);

  if ((base_fetch_type_ != kIproLookup) || status_ok) {
    // If this is a 404 response we need to count it in the stats.
    if (response_headers()->status_code() == HttpStatus::kNotFound) {
      server_context_->rewrite_stats()->resource_404_count()->Add(1);
    }
  }

  // For the IPRO lookup, supress notification of the Envou side here.
  // If we send both the headerscomplete event and the one from done, nasty
  // stuff will happen if we loose the race with with the Envoy side destructing
  // this base fetch instance.
  if (base_fetch_type_ == kIproLookup && !status_ok) {
    suppress_ = true;
  }
}

bool EnvoyBaseFetch::HandleFlush(MessageHandler* handler) {
  Lock();
  need_flush_ = true;
  Unlock();
  return true;
}

int EnvoyBaseFetch::DecrementRefCount() {
  return DecrefAndDeleteIfUnreferenced();
}

int EnvoyBaseFetch::IncrementRefCount() {
  return __sync_add_and_fetch(&references_, 1);
}

int EnvoyBaseFetch::DecrefAndDeleteIfUnreferenced() {
  // Creates a full memory barrier.
  int r = __sync_add_and_fetch(&references_, -1);
  if (r == 0) {
    delete this;
  }
  return r;
}

void EnvoyBaseFetch::HandleDone(bool success) {
  CHECK(!done_called_) << "Done already called!";
  Lock();
  done_called_ = true;
  Unlock();
  DecrefAndDeleteIfUnreferenced();
}

bool EnvoyBaseFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      url_, *options_, request_context(), headers);
}

}  // namespace net_instaweb
