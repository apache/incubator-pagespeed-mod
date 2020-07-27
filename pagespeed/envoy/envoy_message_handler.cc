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

#include "pagespeed/envoy/envoy_message_handler.h"

#include <csignal>

#include "net/instaweb/public/version.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/debug.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/sharedmem/shared_circular_buffer.h"

namespace net_instaweb {

EnvoyMessageHandler::EnvoyMessageHandler(Timer* timer, AbstractMutex* mutex)
    : SystemMessageHandler(timer, mutex) {}

void EnvoyMessageHandler::MessageSImpl(MessageType type,
                                       const GoogleString& message) {
  GoogleMessageHandler::MessageSImpl(type, message);
  AddMessageToBuffer(type, message);
}

void EnvoyMessageHandler::FileMessageSImpl(MessageType type, const char* file,
                                           int line,
                                           const GoogleString& message) {
  GoogleMessageHandler::FileMessageSImpl(type, file, line, message);
  AddMessageToBuffer(type, file, line, message);
}

}  // namespace net_instaweb
