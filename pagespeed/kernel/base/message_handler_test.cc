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


#include "pagespeed/kernel/base/message_handler_test_base.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/message_handler.h"

namespace net_instaweb {

namespace {

class MessageHandlerTest : public testing::Test {
 protected:
  const StringVector& messages() {
    return handler_.messages();
  }

  TestMessageHandler handler_;
};


TEST_F(MessageHandlerTest, Simple) {
  handler_.Message(kWarning, "here is a message");
  handler_.Info("filename.cc", 1, "here is another message");
  ASSERT_EQ(2U, messages().size());
  ASSERT_EQ(messages()[0], "Warning: here is a message");
  ASSERT_EQ(messages()[1], "Info: filename.cc: 1: here is another message");
  ASSERT_EQ(kWarning, MessageHandler::StringToMessageType("Warning"));
  ASSERT_EQ(kFatal, MessageHandler::StringToMessageType("Fatal"));
  //
  // ASSERT_DEATH_IF_SUPPORTED is a cool idea, but it prints:
  //   [WARNING] testing/gtest/src/gtest-death-test.cc:789:: Death tests use
  //   fork(), which is unsafe particularly in a threaded context. For this
  //   test, Google Test couldn't detect the number of threads.
  // and seems to core-dump sporadically.
  //
  // ASSERT_DEATH_IF_SUPPORTED(MessageHandler::StringToMessageType("Random"),
  //                           "Invalid msg level: Random");
}

TEST_F(MessageHandlerTest, MinMessageType) {
  handler_.set_min_message_type(kError);
  handler_.Info("filename.cc", 1, "here is a message");
  handler_.Warning("filename.cc", 1, "here is a message");
  ASSERT_EQ(0U, messages().size());
  handler_.Error("filename.cc", 1, "here is another message");
  ASSERT_EQ(1U, messages().size());
  ASSERT_EQ(messages()[0], "Error: filename.cc: 1: here is another message");
}

}  // namespace

}  // namespace net_instaweb
