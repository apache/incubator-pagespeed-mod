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

#include "envoy_logger.h"

#include <iostream>
#include <string>

#include "external/envoy/source/common/common/assert.h"

namespace net_instaweb {

PagespeedLogSink::PagespeedLogSink(
    Envoy::Logger::DelegatingLogSinkSharedPtr log_sink, MessageHandler* handler)
    : SinkDelegate(log_sink), pagespeed_message_handler_(handler) {
  log_level_ = Envoy::Logger::Registry::logger(logger_str)->level();
}

int PagespeedLogSink::getPagespeedLogLevel(
    spdlog::level::level_enum log_level) {
  switch (log_level) {
    case spdlog::level::trace:
    case spdlog::level::debug:
    case spdlog::level::info:
      return kInfo;
    case spdlog::level::warn:
      return kWarning;
    case spdlog::level::err:
      return kError;
    case spdlog::level::critical:
      return kFatal;
    case spdlog::level::off:
      // TODO : Handle log level off
      return kInfo;
    case spdlog::level::n_levels:
      IS_ENVOY_BUG("unexpected log spdlog::level:");
  }
  IS_ENVOY_BUG("unreachable");
  return kFatal;
}

void PagespeedLogSink::log(absl::string_view msg, const spdlog::details::log_msg&) {
  pagespeed_message_handler_->Message(
      static_cast<MessageType>(getPagespeedLogLevel(log_level_)), "%s",
      std::string(msg).c_str());
}

void PagespeedLogSink::flush() {}

}  // namespace net_instaweb
