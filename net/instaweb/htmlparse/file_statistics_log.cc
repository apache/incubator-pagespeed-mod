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


#include "net/instaweb/htmlparse/public/file_statistics_log.h"

#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

// TODO(jmarantz): convert to statistics interface

namespace net_instaweb {
class MessageHandler;

FileStatisticsLog::FileStatisticsLog(FileSystem::OutputFile* file,
                                     MessageHandler* message_handler)
  : file_(file),
    message_handler_(message_handler) {
}

FileStatisticsLog::~FileStatisticsLog() {
}

void FileStatisticsLog::LogStat(const char *stat_name, int value) {
  // Buffer whole log entry before writing, in case there's interleaving going
  // on (ie avoid multiple writes for single log entry)
  GoogleString buf(stat_name);
  StrAppend(&buf, ": ", IntegerToString(value), "\n");
  file_->Write(buf, message_handler_);
}

}  // namespace net_instaweb
