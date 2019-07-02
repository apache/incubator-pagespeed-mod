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


#include "pagespeed/kernel/thread/pthread_condvar.h"

#include <pthread.h>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/condvar.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/condvar_test_base.h"
#include "pagespeed/kernel/thread/pthread_mutex.h"

namespace net_instaweb {

class PthreadCondvarTest : public CondvarTestBase {
 protected:
  PthreadCondvarTest()
      : pthread_mutex_(),
        pthread_startup_condvar_(&pthread_mutex_),
        pthread_condvar_(&pthread_mutex_) {
    Init(&pthread_mutex_, &pthread_startup_condvar_, &pthread_condvar_);
  }

  virtual void CreateHelper() {
    pthread_create(&helper_thread_, NULL,
                   &HelperThread, this);
  }

  virtual void FinishHelper() {
    pthread_join(helper_thread_, NULL);
  }

  virtual Timer* timer() { return &timer_; }

  PthreadMutex pthread_mutex_;
  PthreadCondvar pthread_startup_condvar_;
  PthreadCondvar pthread_condvar_;
  pthread_t helper_thread_;
  PosixTimer timer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PthreadCondvarTest);
};

TEST_F(PthreadCondvarTest, TestStartup) {
  StartupTest();
}

TEST_F(PthreadCondvarTest, BlindSignals) {
  BlindSignalsTest();
}

TEST_F(PthreadCondvarTest, BroadcastBlindSignals) {
  signal_method_ = &ThreadSystem::Condvar::Broadcast;
  BlindSignalsTest();
}

TEST_F(PthreadCondvarTest, TestPingPong) {
  PingPongTest();
}

TEST_F(PthreadCondvarTest, BroadcastTestPingPong) {
  signal_method_ = &ThreadSystem::Condvar::Broadcast;
  PingPongTest();
}

TEST_F(PthreadCondvarTest, TestTimeout) {
  TimeoutTest();
}

TEST_F(PthreadCondvarTest, TestLongTimeout1200) {
  // We pick a value over a second because the implementation special-cases
  // that situation.
  LongTimeoutTest(1100);
  LongTimeoutTest(100);
}

TEST_F(PthreadCondvarTest, TimeoutPingPong) {
  TimeoutPingPongTest();
}

TEST_F(PthreadCondvarTest, BroadcastTimeoutPingPong) {
  signal_method_ = &ThreadSystem::Condvar::Broadcast;
  TimeoutPingPongTest();
}

}  // namespace net_instaweb
