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



#include "pagespeed/kernel/http/http_options.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

const HttpOptions kDefaultHttpOptionsForTests = {
  false,  // respect_vary
  5 * Timer::kMinuteMs,  // implicit_cache_ttl_ms
};
const HttpOptions kDeprecatedDefaultHttpOptions = kDefaultHttpOptionsForTests;

}  // namespace net_instaweb
