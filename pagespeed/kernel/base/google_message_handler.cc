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

#include "pagespeed/kernel/base/google_message_handler.h"

#include <cstdarg>
#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

void GoogleMessageHandler::MessageSImpl(MessageType type,
                                        const GoogleString& message) {
  switch (type) {
    case kInfo:
      LOG(INFO) << message;
      break;
    case kWarning:
      LOG(WARNING) << message;
      break;
    case kError:
      LOG(ERROR) << message;
      break;
    case kFatal:
      LOG(FATAL) << message;
      break;
  }
}
void GoogleMessageHandler::MessageVImpl(MessageType type, const char* msg,
                                        va_list args) {
  // The seeming duplication avoids formatting if loglevel doesn't require it.
  switch (type) {
    case kInfo:
      LOG(INFO) << Format(msg, args);
      break;
    case kWarning:
      LOG(WARNING) << Format(msg, args);
      break;
    case kError:
      LOG(ERROR) << Format(msg, args);
      break;
    case kFatal:
      LOG(FATAL) << Format(msg, args);
      break;
  }
}

void GoogleMessageHandler::FileMessageSImpl(
    MessageType type, const char* file, int line, const GoogleString& message) {
  switch (type) {
    case kInfo:
      LOG(INFO) << file << ":" << line << ": " << message;
      break;
    case kWarning:
      LOG(WARNING) << file << ":" << line << ": " << message;
      break;
    case kError:
      LOG(ERROR) << file << ":" << line << ": " << message;
      break;
    case kFatal:
      LOG(FATAL) << file << ":" << line << ": " << message;
      break;
  }
}

void GoogleMessageHandler::FileMessageVImpl(MessageType type, const char* file,
                                            int line, const char* msg,
                                            va_list args) {
  // The seeming duplication avoids formatting if loglevel doesn't require it.
  switch (type) {
    case kInfo:
      LOG(INFO) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kWarning:
      LOG(WARNING) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kError:
      LOG(ERROR) << file << ":" << line << ": " << Format(msg, args);
      break;
    case kFatal:
      LOG(FATAL) << file << ":" << line << ": " << Format(msg, args);
      break;
  }
}

GoogleString GoogleMessageHandler::Format(const char* msg, va_list args) {
  GoogleString result;
  FormatTo(&result, msg, args);
  return result;
}

}  // namespace net_instaweb
