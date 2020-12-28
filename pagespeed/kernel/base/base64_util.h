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

#pragma once

#include "absl/strings/escaping.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

inline void Web64Encode(const StringPiece& in, GoogleString* out) {
  *out = absl::WebSafeBase64Escape(in);
}

inline bool Web64Decode(const StringPiece& in, GoogleString* out) {
  return absl::WebSafeBase64Unescape(in, out);
}

inline void Mime64Encode(const StringPiece& in, GoogleString* out) {
  *out = absl::Base64Escape(in);
}

inline bool Mime64Decode(const StringPiece& in, GoogleString* out) {
  return absl::Base64Unescape(in, out);
}

}  // namespace net_instaweb
