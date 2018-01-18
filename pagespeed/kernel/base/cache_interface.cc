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


#include "pagespeed/kernel/base/cache_interface.h"

namespace net_instaweb {

namespace {

static const char* kKeyStateNames[] = {
  "available",
  "not_found",
  "overload",
  "network_error",
    "timeout",
};

}  // namespace

const char* CacheInterface::KeyStateName(KeyState state) {
  return kKeyStateNames[state];
}

CacheInterface::CacheInterface() {
}

CacheInterface::~CacheInterface() {
}

CacheInterface::Callback::~Callback() {
}

CacheInterface* CacheInterface::Backend() {
  return this;
}

void CacheInterface::ValidateAndReportResult(const GoogleString& key,
                                             KeyState state,
                                             Callback* callback) {
  if (!callback->ValidateCandidate(key, state)) {
    state = kNotFound;
  }
  callback->Done(state);
}

void CacheInterface::MultiGet(MultiGetRequest* request) {
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback* key_callback = &(*request)[i];
    Get(key_callback->key, key_callback->callback);
  }
  delete request;
}

void CacheInterface::ReportMultiGetNotFound(MultiGetRequest* request) {
  for (int i = 0, n = request->size(); i < n; ++i) {
    KeyCallback& key_callback = (*request)[i];
    ValidateAndReportResult(key_callback.key, kNotFound, key_callback.callback);
  }
  delete request;
}

}  // namespace net_instaweb
