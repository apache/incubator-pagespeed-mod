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


#include "pagespeed/kernel/http/domain_registry.h"

#include <cstddef>                     // for size_t

#include "third_party/domain_registry_provider/src/domain_registry/domain_registry.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
namespace domain_registry {

void Init() {
  InitializeDomainRegistry();
}

StringPiece MinimalPrivateSuffix(StringPiece hostname) {
  if (hostname.empty()) {
    return "";
  }

  size_t length_of_public_suffix =
      GetRegistryLength(hostname.as_string().c_str());
  if (length_of_public_suffix == 0) {
    // Unrecognized top level domain.  We don't know what kind of multi-level
    // public suffixes they might have created, so be on the safe side and
    // treat the entire hostname as a private suffix.
    return hostname;
  }

  stringpiece_ssize_type last_dot_before_private_suffix =
      hostname.rfind('.', hostname.size() - length_of_public_suffix
                     - 1 /* don't include the dot */
                     - 1 /* pos is inclusive */);
  if (last_dot_before_private_suffix == StringPiece::npos) {
    // Hostname is already a minimal private suffix.
    last_dot_before_private_suffix = 0;
  } else {
    last_dot_before_private_suffix++;  // Don't include the dot.
  }
  return hostname.substr(last_dot_before_private_suffix);
}

}  // namespace domain_registry
}  // namespace net_instaweb
