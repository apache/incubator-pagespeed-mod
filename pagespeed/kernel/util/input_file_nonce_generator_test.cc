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



#include "pagespeed/kernel/util/input_file_nonce_generator.h"

#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/util/nonce_generator_test_base.h"

namespace net_instaweb {
namespace {

class InputFileNonceGeneratorTest : public NonceGeneratorTestBase {
 protected:
  InputFileNonceGeneratorTest() {
    main_generator_.reset(new InputFileNonceGenerator(
        file_system_.OpenInputFile("/dev/urandom", &handler_),
        &file_system_, new NullMutex, &handler_));
    other_generator_.reset(new InputFileNonceGenerator(
        file_system_.OpenInputFile("/dev/urandom", &handler_),
        &file_system_, new NullMutex, &handler_));
  }

  GoogleMessageHandler handler_;
  StdioFileSystem file_system_;
};

TEST_F(InputFileNonceGeneratorTest, DuplicateFreedom) {
  DuplicateFreedom();
}

TEST_F(InputFileNonceGeneratorTest, DifferentNonOverlap) {
  DifferentNonOverlap();
}

TEST_F(InputFileNonceGeneratorTest, AllBitsUsed) {
  AllBitsUsed();
}

}  // namespace
}  // namespace net_instaweb
