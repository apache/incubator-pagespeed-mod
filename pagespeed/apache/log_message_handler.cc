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


#include "pagespeed/apache/log_message_handler.h"

#include <unistd.h>

#include <limits>
#include <string>

#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/apache/apache_logging_includes.h"

// Make sure we don't attempt to use LOG macros here, since doing so
// would cause us to go into an infinite log loop.
#undef LOG
#define LOG USING_LOG_HERE_WOULD_CAUSE_INFINITE_RECURSION

namespace {

apr_pool_t* log_pool = NULL;

const int kMaxInt = std::numeric_limits<int>::max();
int log_level_cutoff = kMaxInt;
GoogleString* mod_pagespeed_version = NULL;

int GetApacheLogLevel(int severity) {
  switch (severity) {
    case logging::LOG_INFO:
      // Note: ap_log_perror only prints NOTICE and higher messages.
      // TODO(sligocki): Find some way to print these as INFO if we can.
      //return APLOG_INFO;
      return APLOG_NOTICE;
    case logging::LOG_WARNING:
      return APLOG_WARNING;
    case logging::LOG_ERROR:
      return APLOG_ERR;
    case logging::LOG_ERROR_REPORT:
      return APLOG_CRIT;
    case logging::LOG_FATAL:
      return APLOG_ALERT;
    default:  // For VLOG()s
      // TODO(sligocki): return APLOG_DEBUG;
      return APLOG_NOTICE;
  }
}

bool LogMessageHandler(int severity, const char* file, int line,
                       size_t message_start, const GoogleString& str) {
  const int this_log_level = GetApacheLogLevel(severity);

  GoogleString message = str;
  if (severity == logging::LOG_FATAL) {
    if (base::debug::BeingDebugged()) {
      base::debug::BreakDebugger();
    } else {
      base::debug::StackTrace trace;
      std::ostringstream stream;
      trace.OutputToStream(&stream);
      message.append(stream.str());
    }
  }

  // Trim the newline off the end of the message string.
  size_t last_msg_character_index = message.length() - 1;
  if (message[last_msg_character_index] == '\n') {
    message.resize(last_msg_character_index);
  }

  if (this_log_level <= log_level_cutoff || log_level_cutoff == kMaxInt) {
    ap_log_perror(APLOG_MARK, this_log_level, APR_SUCCESS, log_pool,
                  "[mod_pagespeed %s @%ld] %s",
                  (mod_pagespeed_version == NULL)
                    ? ""
                    : mod_pagespeed_version->c_str(),
                  static_cast<long>(getpid()),
                  message.c_str());
  }

  if (severity == logging::LOG_FATAL) {
    // Crash the process to generate a dump.
    base::debug::BreakDebugger();
  }

  return true;
}

}  // namespace


namespace net_instaweb {

namespace log_message_handler {

void Install(apr_pool_t* pool) {
  log_pool = pool;
  logging::SetLogMessageHandler(&LogMessageHandler);
}

void ShutDown() {
  if (mod_pagespeed_version != NULL) {
    delete mod_pagespeed_version;
    mod_pagespeed_version = NULL;
  }
}

// What Google level of logs to display when Apache LogLevel is Debug.
// -2 means all VLOG(2) and higher will be displayed as INFOs
const int kDebugLogLevel = -2;

// TODO(sligocki): This is not thread-safe, do we care? Error case is when
// you have multiple server_rec's with different LogLevels which start-up
// simultaneously. In which case we might get a non-min global LogLevel
void AddServerConfig(const server_rec* server, const StringPiece& version) {
  // TODO(sligocki): Maybe use ap_log_error(server) if there is exactly one
  // server added?
  int curr_log_level_cutoff;
#if (AP_SERVER_MAJORVERSION_NUMBER == 2) && (AP_SERVER_MINORVERSION_NUMBER >= 4)
  // Apache 2.4 adds per-module log level configuration.
  curr_log_level_cutoff =
      ap_get_server_module_loglevel(server, APLOG_MODULE_INDEX);
#else
  curr_log_level_cutoff = server->loglevel;
#endif
  log_level_cutoff = std::min(curr_log_level_cutoff, log_level_cutoff);

  // Get VLOG(x) and above if LogLevel is set to Debug.
  if (log_level_cutoff >= APLOG_DEBUG) {
    logging::SetMinLogLevel(kDebugLogLevel);
  }
  if (mod_pagespeed_version == NULL) {
    mod_pagespeed_version = new GoogleString(version.as_string());
  } else {
    *mod_pagespeed_version = version.as_string();
  }
}


}  // namespace log_message_handler

}  // namespace net_instaweb
