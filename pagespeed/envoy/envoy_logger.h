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

#include "external/envoy/source/common/common/logger.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace net_instaweb {

static constexpr char logger_str[] = "main";

/**
 * SinkDelegate that redirects logs to pagespeed message handler.
 */
class PagespeedLogSink : public Envoy::Logger::SinkDelegate {
 public:
  PagespeedLogSink(Envoy::Logger::DelegatingLogSinkSharedPtr log_sink,
                   MessageHandler* handler);

 private:
  int getPagespeedLogLevel(spdlog::level::level_enum log_level);
  void log(absl::string_view msg, const spdlog::details::log_msg&) override;
  void flush() override;

  MessageHandler* pagespeed_message_handler_;
  spdlog::level::level_enum log_level_;
};

}  // namespace net_instaweb
