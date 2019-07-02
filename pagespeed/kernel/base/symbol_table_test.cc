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


// Unit-test the symbol table

#include "pagespeed/kernel/base/symbol_table.h"

#include "pagespeed/kernel/base/atom.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class SymbolTableTest : public testing::Test {
};

TEST_F(SymbolTableTest, TestInternSensitive) {
  SymbolTableSensitive symbol_table;
  GoogleString s1("hello");
  GoogleString s2("hello");
  GoogleString s3("goodbye");
  GoogleString s4("Goodbye");
  EXPECT_NE(s1.c_str(), s2.c_str());
  Atom a1 = symbol_table.Intern(s1);
  Atom a2 = symbol_table.Intern(s2);
  Atom a3 = symbol_table.Intern(s3);
  Atom a4 = symbol_table.Intern(s4);
  EXPECT_TRUE(a1 == a2);
  EXPECT_EQ(a1.Rep()->data(), a2.Rep()->data());
  EXPECT_FALSE(a1 == a3);
  EXPECT_NE(a1.Rep()->data(), a3.Rep()->data());
  EXPECT_FALSE(a3 == a4);

  EXPECT_EQ(s1, a1.Rep()->as_string());
  EXPECT_EQ(s2, a2.Rep()->as_string());
  EXPECT_EQ(s3, a3.Rep()->as_string());
  EXPECT_EQ(s4, a4.Rep()->as_string());

  Atom empty = symbol_table.Intern("");
  EXPECT_TRUE(Atom() == empty);
}

TEST_F(SymbolTableTest, TestInternInsensitive) {
  SymbolTableInsensitive symbol_table;
  GoogleString s1("hello");
  GoogleString s2("Hello");
  GoogleString s3("goodbye");
  Atom a1 = symbol_table.Intern(s1);
  Atom a2 = symbol_table.Intern(s2);
  Atom a3 = symbol_table.Intern(s3);
  EXPECT_TRUE(a1 == a2);
  EXPECT_EQ(a1.Rep()->data(), a2.Rep()->data());
  EXPECT_FALSE(a1 == a3);
  EXPECT_NE(*a1.Rep(), *a3.Rep());
  EXPECT_NE(a1.Rep(), a3.Rep());
  EXPECT_NE(a1.Rep()->data(), a3.Rep()->data());

  EXPECT_EQ(0, StringCaseCompare(s1, a1.Rep()->as_string()));
  EXPECT_EQ(0, StringCaseCompare(s2, a2.Rep()->as_string()));
  EXPECT_EQ(0, StringCaseCompare(s3, a3.Rep()->as_string()));

  Atom empty = symbol_table.Intern("");
  EXPECT_TRUE(Atom() == empty);
}

TEST_F(SymbolTableTest, TestClear) {
  SymbolTableSensitive symbol_table;
  Atom a = symbol_table.Intern("a");
  EXPECT_EQ(1, symbol_table.string_bytes_allocated());
  a = symbol_table.Intern("a");
  EXPECT_EQ(1, symbol_table.string_bytes_allocated());
  symbol_table.Clear();
  EXPECT_EQ(0, symbol_table.string_bytes_allocated());
  a = symbol_table.Intern("a");
  EXPECT_EQ(1, symbol_table.string_bytes_allocated());
}

// Symbol table's string storage special cases large items (> 32k) so
// test interleaved allocating of small and large strings.
TEST_F(SymbolTableTest, TestBigInsert) {
  SymbolTableSensitive symbol_table;
  Atom a = symbol_table.Intern(GoogleString(100000, 'a'));
  Atom b = symbol_table.Intern("b");
  Atom c = symbol_table.Intern(GoogleString(100000, 'c'));
  Atom d = symbol_table.Intern("d");
  EXPECT_TRUE(a == symbol_table.Intern(GoogleString(100000, 'a')));
  EXPECT_TRUE(b == symbol_table.Intern("b"));
  EXPECT_TRUE(c == symbol_table.Intern(GoogleString(100000, 'c')));
  EXPECT_TRUE(d == symbol_table.Intern("d"));
}

TEST_F(SymbolTableTest, TestOverflowFirstChunk) {
  SymbolTableSensitive symbol_table;
  for (int i = 0; i < 10000; ++i) {
    symbol_table.Intern(IntegerToString(i));
  }
  EXPECT_LT(32768, symbol_table.string_bytes_allocated());
}

TEST_F(SymbolTableTest, InternEmbeddedNull) {
  const char kBytes[] = { 'A', '\0', 'B' };
  SymbolTableSensitive symbol_table;
  Atom a1 = symbol_table.Intern(StringPiece(kBytes, 1));
  Atom a2 = symbol_table.Intern(StringPiece(kBytes, 3));
  EXPECT_NE(a1, a2);
}

}  // namespace net_instaweb
