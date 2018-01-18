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


#ifndef NET_INSTAWEB_HTMLPARSE_PUBLIC_STATISTICS_LOG_H_
#define NET_INSTAWEB_HTMLPARSE_PUBLIC_STATISTICS_LOG_H_

#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

class StatisticsLog {
 public:
  StatisticsLog() { }
  virtual ~StatisticsLog();
  virtual void LogStat(const char *statName, int value) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatisticsLog);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_HTMLPARSE_PUBLIC_STATISTICS_LOG_H_
