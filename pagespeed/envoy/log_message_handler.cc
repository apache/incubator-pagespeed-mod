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

#include "log_message_handler.h"

#include <unistd.h>

#include <limits>
#include <string>

#include "absl/strings/strip.h"
#include "base/logging.h"
#include "external/envoy/source/common/common/logger.h"
#include "net/instaweb/public/version.h"
#include "pagespeed/kernel/base/string_util.h"

namespace {
class Logger : public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
  bool LogMessageHandler(int severity, const char*, int, size_t,
                         const GoogleString& str) {
    // TODO(oschaaf): if log level is fatal we need to do more:
    // - if debugging, break
    // - else log stack trace.
    StringPiece message = str;
    absl::ConsumeSuffix(&message, "\n");
    constexpr char preamble[] = "[pagespeed %s] %s";
    switch (severity) {
      case logging::LOG_INFO:
        ENVOY_LOG(info, preamble, net_instaweb::kModPagespeedVersion, message);
      case logging::LOG_WARNING:
        ENVOY_LOG(warn, preamble, net_instaweb::kModPagespeedVersion, message);
      case logging::LOG_ERROR:
        ENVOY_LOG(error, preamble, net_instaweb::kModPagespeedVersion, message);
      case logging::LOG_FATAL:
        ENVOY_LOG(critical, preamble, net_instaweb::kModPagespeedVersion,
                  message);
      default:  // For VLOG(s)
        ENVOY_LOG(debug, preamble, net_instaweb::kModPagespeedVersion, message);
    }
    return true;
  }
};

}  // namespace

namespace net_instaweb {

namespace log_message_handler {

void Install() {
  // TODO(oschaaF): Set log filter here according to configuration
  // logging::SetLogMessageHandler(&LogMessageHandler);
  // logging::SetMinLogLevel(-2);
}

}  // namespace log_message_handler
}  // namespace net_instaweb
