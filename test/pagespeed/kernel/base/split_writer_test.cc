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

#include "pagespeed/kernel/base/split_writer.h"

#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/writer.h"
#include "test/pagespeed/kernel/base/gtest.h"

namespace net_instaweb {
class MessageHandler;

namespace {

TEST(SplitWriterTest, SplitsWrite) {
  GoogleString str1, str2;
  StringWriter writer1(&str1), writer2(&str2);
  SplitWriter split_writer(&writer1, &writer2);

  EXPECT_TRUE(str1.empty());
  EXPECT_TRUE(str2.empty());

  EXPECT_TRUE(split_writer.Write("Hello, ", nullptr));
  EXPECT_EQ("Hello, ", str1);
  EXPECT_EQ("Hello, ", str2);

  EXPECT_TRUE(writer1.Write("World!", nullptr));
  EXPECT_TRUE(writer2.Write("Nobody.", nullptr));
  EXPECT_EQ("Hello, World!", str1);
  EXPECT_EQ("Hello, Nobody.", str2);

  EXPECT_TRUE(split_writer.Write(" Goodbye.", nullptr));
  EXPECT_EQ("Hello, World! Goodbye.", str1);
  EXPECT_EQ("Hello, Nobody. Goodbye.", str2);

  EXPECT_TRUE(split_writer.Flush(nullptr));
}

class FailWriter : public Writer {
 public:
  bool Write(const StringPiece& str, MessageHandler* handler) override {
    return false;
  }

  bool Flush(MessageHandler* handler) override { return false; }
};

TEST(SplitWriterTest, WritesToBothEvenOnFailure) {
  FailWriter fail_writer;
  GoogleString str;
  StringWriter string_writer(&str);

  SplitWriter split_fail_first(&fail_writer, &string_writer);
  EXPECT_TRUE(str.empty());
  EXPECT_FALSE(split_fail_first.Write("Hello, World!", nullptr));
  EXPECT_EQ("Hello, World!", str);
  EXPECT_FALSE(split_fail_first.Flush(nullptr));

  str.clear();

  SplitWriter split_fail_second(&string_writer, &fail_writer);
  EXPECT_TRUE(str.empty());
  EXPECT_FALSE(split_fail_second.Write("Hello, World!", nullptr));
  EXPECT_EQ("Hello, World!", str);
  EXPECT_FALSE(split_fail_second.Flush(nullptr));
}

}  // namespace

}  // namespace net_instaweb
