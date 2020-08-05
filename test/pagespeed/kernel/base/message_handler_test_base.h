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

#include <cstdarg>

#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

#ifndef PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_TEST_BASE_H_
#define PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_TEST_BASE_H_

namespace net_instaweb {

class TestMessageHandler : public net_instaweb::MessageHandler {
 public:
  const StringVector& messages() { return messages_; }

 protected:
  void MessageVImpl(MessageType type, const char* msg, va_list args) override;
  void MessageSImpl(MessageType type, const GoogleString& message) override;

  void FileMessageVImpl(MessageType type, const char* filename, int line,
                        const char* msg, va_list args) override;
  void FileMessageSImpl(MessageType type, const char* filename, int line,
                        const GoogleString& message) override;

 private:
  StringVector messages_;
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_MESSAGE_HANDLER_TEST_BASE_H_
