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


#ifndef PAGESPEED_KERNEL_BASE_POSIX_TIMER_H_
#define PAGESPEED_KERNEL_BASE_POSIX_TIMER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/timer.h"

namespace net_instaweb {

class PosixTimer : public Timer {
 public:
  PosixTimer() {}
  virtual ~PosixTimer();

  virtual int64 NowUs() const;
  virtual void SleepUs(int64 us);

 private:
  DISALLOW_COPY_AND_ASSIGN(PosixTimer);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_POSIX_TIMER_H_
