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


#include "pagespeed/kernel/base/message_handler.h"

#include <cstdarg>

#include "base/logging.h"

namespace net_instaweb {

MessageHandler::MessageHandler() : min_message_type_(kInfo) {
}

MessageHandler::~MessageHandler() {
}

const char* MessageHandler::MessageTypeToString(const MessageType type) const {
  const char* type_string = NULL;

  // Don't include a 'default:' clause so that the compiler can tell us when we
  // are missing an enum value.  Instead use a null check for 'type_string' to
  // indicate a data corruption that avoids hitting any of the cases.
  switch (type) {
    case kInfo:
      type_string = "Info";
      break;
    case kWarning:
      type_string = "Warning";
      break;
    case kError:
      type_string = "Error";
      break;
    case kFatal:
      type_string = "Fatal";
      break;
  }
  CHECK(type_string != NULL) << "INVALID MessageType!";
  return type_string;
}

MessageType MessageHandler::StringToMessageType(const StringPiece& msg) {
  if (StringCaseEqual(msg, "Info")) {
    return kInfo;
  } else if (StringCaseEqual(msg, "Warning")) {
    return kWarning;
  } else if (StringCaseEqual(msg, "Error")) {
    return kError;
  } else if (StringCaseEqual(msg, "Fatal")) {
    return kFatal;
  }
  CHECK(false) << "Invalid msg level: " << msg;
  return kInfo;
}

void MessageHandler::Message(MessageType type, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  MessageV(type, msg, args);
  va_end(args);
}

void MessageHandler::MessageV(MessageType type, const char* msg, va_list args) {
  if (type >= min_message_type_) {
    MessageVImpl(type, msg, args);
  }
}

void MessageHandler::MessageS(
    MessageType type, const GoogleString& message) {
  if (type >= min_message_type_) {
    MessageSImpl(type, message);
  }
}

// Default implementation of MessageVImpl formats and then calls MessageS.
void MessageHandler::MessageVImpl(
    MessageType type, const char* msg, va_list args) {
  GoogleString buffer;
  FormatTo(&buffer, msg, args);
  MessageSImpl(type, buffer);
}

void MessageHandler::FileMessage(MessageType type, const char* file, int line,
                                 const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FileMessageV(type, file, line, msg, args);
  va_end(args);
}

void MessageHandler::FileMessageV(MessageType type, const char* filename,
                                  int line, const char* msg, va_list args) {
  if (type >= min_message_type_) {
    FileMessageVImpl(type, filename, line, msg, args);
  }
}

void MessageHandler::FileMessageS(MessageType type, const char* filename,
                                  int line, const GoogleString& message) {
  if (type >= min_message_type_) {
    FileMessageSImpl(type, filename, line, message);
  }
}

void MessageHandler::FileMessageVImpl(
    MessageType type, const char* filename, int line,
    const char* msg, va_list args) {
  GoogleString buffer;
  FormatTo(&buffer, msg, args);
  FileMessageSImpl(type, filename, line, buffer);
}

void MessageHandler::Check(bool condition, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  CheckV(condition, msg, args);
  va_end(args);
}

void MessageHandler::CheckV(bool condition, const char* msg, va_list args) {
  if (!condition) {
    MessageV(kFatal, msg, args);
  }
}

void MessageHandler::Info(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  InfoV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::Warning(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  WarningV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::Error(const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  ErrorV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::FatalError(
    const char* file, int line, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  FatalErrorV(file, line, msg, args);
  va_end(args);
}

void MessageHandler::FormatTo(GoogleString* buffer,
                              const char* msg, va_list args) {
  // Ignore the name of this routine: it formats with vsnprintf.
  // See base/stringprintf.cc.
  StringAppendV(buffer, msg, args);
}

bool MessageHandler::Dump(Writer* writer) {
  return false;
}

void MessageHandler::ParseMessageDumpIntoMessages(
    StringPiece message_dump, StringPieceVector* messages) {
  // Ignore the first line to make sure all the messages dumped are complete.
  stringpiece_ssize_type pos = message_dump.find("\n");
  if (pos != StringPiece::npos) {
    message_dump.remove_prefix(pos);
  }
  SplitStringPieceToVector(message_dump, "\n", messages, false);
}

MessageType MessageHandler::GetMessageType(StringPiece message) {
  switch (message[0]) {
    case 'E': {
      return kError;
    }
    case 'W': {
      return kWarning;
    }
    case 'F': {
      return kFatal;
    }
    default: {
      return kInfo;
    }
  }
}

StringPiece MessageHandler::ReformatMessage(StringPiece message) {
  // Remove the first character which is the type indicator.
  return message.substr(1);
}

}  // namespace net_instaweb
