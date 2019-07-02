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

//         morlovich@google.com (Maksim Orlovich)

#include "pagespeed/kernel/cache/delegating_cache_callback.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/cache/cache_interface.h"

namespace net_instaweb {

DelegatingCacheCallback::DelegatingCacheCallback(
    CacheInterface::Callback* callback)
    : callback_(callback),
      validate_candidate_called_(false) {
}

DelegatingCacheCallback::~DelegatingCacheCallback() {
}

// Note that we have to forward validity faithfully here, as if we're
// wrapping a 2-level cache it will need to know accurately if the value
// is valid or not.
bool DelegatingCacheCallback::ValidateCandidate(
    const GoogleString& key, CacheInterface::KeyState state) {
  validate_candidate_called_ = true;
  callback_->set_value(value());
  return callback_->DelegatedValidateCandidate(key, state);
}

void DelegatingCacheCallback::Done(CacheInterface::KeyState state) {
  DCHECK(validate_candidate_called_);

  // We don't have to do validation or value forwarding ourselves since
  // whatever we are wrapping must have already called ValidateCandidate().
  callback_->DelegatedDone(state);
  delete this;
}

}  // namespace net_instaweb
