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



#include "pagespeed/apache/apache_message_handler.h"

#include <signal.h>
#include <unistd.h>

#include "pagespeed/apache/apr_timer.h"
#include "pagespeed/apache/log_message_handler.h"

#include "pagespeed/apache/apache_httpd_includes.h"
#include "pagespeed/apache/apache_logging_includes.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/debug.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"

namespace {

// This name will be prefixed to every logged message.  This could be made
// smaller if people think it's too long.  In my opinion it's probably OK,
// and it would be good to let people know where messages are coming from.
const char kModuleName[] = "mod_pagespeed";

// For crash handler's use.
const server_rec* global_server;

}  // namespace

extern "C" {

static void signal_handler(int sig) {
  // Try to output the backtrace to the log file. Since this may end up
  // crashing/deadlocking/etc. we set an alarm() to abort us if it comes to
  // that.
  alarm(2);
  ap_log_error(APLOG_MARK, APLOG_ALERT, APR_SUCCESS, global_server,
               "[@%s] CRASH with signal:%d at %s",
               net_instaweb::Integer64ToString(getpid()).c_str(),
               sig,
               net_instaweb::StackTraceString().c_str());
  kill(getpid(), SIGKILL);
}

}  // extern "C"

namespace net_instaweb {

// filename_prefix of ApacheRewriteDriverFactory is needed to initialize
// SharedCircuarBuffer. However, ApacheRewriteDriverFactory needs
// ApacheMessageHandler before its filename_prefix is set. So we initialize
// ApacheMessageHandler without SharedCircularBuffer first, then initalize its
// SharedCircularBuffer in RootInit() when filename_prefix is set.
ApacheMessageHandler::ApacheMessageHandler(const server_rec* server,
                                           const StringPiece& version,
                                           Timer* timer, AbstractMutex* mutex)
    : SystemMessageHandler(timer, mutex),
      server_rec_(server),
      version_(version.data(), version.size()) {
  // Tell log_message_handler about this server_rec and version.
  log_message_handler::AddServerConfig(server_rec_, version);
  // TODO(jmarantz): consider making this a little terser by default.
  // The string we expect in is something like "0.9.1.1-171" and we will
  // may be able to pick off some of the 5 fields that prove to be boring.
}

// Installs a signal handler for common crash signals that tries to print
// out a backtrace.
void ApacheMessageHandler::InstallCrashHandler(server_rec* server) {
  global_server = server;
  signal(SIGTRAP, signal_handler);  // On check failures
  signal(SIGABRT, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGSEGV, signal_handler);
}

int ApacheMessageHandler::GetApacheLogLevel(MessageType type) {
  switch (type) {
    case kInfo:
      // TODO(sligocki): Do we want this to be INFO or NOTICE.
      return APLOG_INFO;
    case kWarning:
      return APLOG_WARNING;
    case kError:
      return APLOG_ERR;
    case kFatal:
      return APLOG_ALERT;
  }

  // This should never fall through, but some compilers seem to complain if
  // we don't include this.
  return APLOG_ALERT;
}

void ApacheMessageHandler::MessageSImpl(
    MessageType type, const GoogleString& message) {
  int log_level = GetApacheLogLevel(type);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "[%s %s @%ld] %s",
               kModuleName, version_.c_str(), static_cast<long>(getpid()),
               message.c_str());
  AddMessageToBuffer(type, message);
}

void ApacheMessageHandler::FileMessageSImpl(
    MessageType type, const char* file, int line, const GoogleString& message) {
  int log_level = GetApacheLogLevel(type);
  ap_log_error(APLOG_MARK, log_level, APR_SUCCESS, server_rec_,
               "[%s %s @%ld] %s:%d: %s",
               kModuleName, version_.c_str(), static_cast<long>(getpid()),
               file, line, message.c_str());
  AddMessageToBuffer(type, file, line, message);
}

}  // namespace net_instaweb
