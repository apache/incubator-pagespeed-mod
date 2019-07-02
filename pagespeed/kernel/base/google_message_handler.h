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


// TODO(huibao): Rename GoogleMessageHandler and google_message_handler
// to reflect the fact that they are not google specific.

#ifndef PAGESPEED_KERNEL_BASE_GOOGLE_MESSAGE_HANDLER_H_
#define PAGESPEED_KERNEL_BASE_GOOGLE_MESSAGE_HANDLER_H_

#include <cstdarg>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

// Implementation of an HTML parser message handler that uses Google
// logging to emit messsages.
class GoogleMessageHandler : public MessageHandler {
 public:
  GoogleMessageHandler() { }

  // These are left public so they can be delegated to.
  virtual void MessageVImpl(MessageType type, const char* msg, va_list args);
  virtual void MessageSImpl(MessageType type, const GoogleString& message);

  virtual void FileMessageVImpl(MessageType type, const char* filename,
                                int line, const char* msg, va_list args);
  virtual void FileMessageSImpl(MessageType type, const char* filename,
                                int line, const GoogleString& message);
  GoogleString Format(const char* msg, va_list args);

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleMessageHandler);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_GOOGLE_MESSAGE_HANDLER_H_
