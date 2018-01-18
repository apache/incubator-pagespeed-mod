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


#include "pagespeed/kernel/http/domain_registry.h"

#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

TEST(DomainRegistry, MinimalPrivateSuffix) {
  domain_registry::Init();

  // com is a public suffix, so both google.com and www.google.com should yield
  // google.com
  EXPECT_EQ("google.com",
            domain_registry::MinimalPrivateSuffix("google.com"));
  EXPECT_EQ("google.com",
            domain_registry::MinimalPrivateSuffix("www.google.com"));

  // We should allow trailing dots, which specify fully-qualified domain names.
  EXPECT_EQ("google.com.",
            domain_registry::MinimalPrivateSuffix("www.google.com."));
  EXPECT_EQ("google.com.",
            domain_registry::MinimalPrivateSuffix("google.com."));

  // But two trailing dots is an error, and on errors we "fail secure" by
  // using the whole string.
  EXPECT_EQ("www.google.com..",
            domain_registry::MinimalPrivateSuffix("www.google.com.."));

  // co.uk is a public suffix, so *google.co.uk just becomes google.uk
  EXPECT_EQ("google.co.uk",
            domain_registry::MinimalPrivateSuffix("google.co.uk"));
  EXPECT_EQ("google.co.uk",
            domain_registry::MinimalPrivateSuffix("www.google.co.uk"));
  EXPECT_EQ("google.co.uk",
            domain_registry::MinimalPrivateSuffix("foo.bar.google.co.uk"));

  // Check that we handle lots of url components properly.
  EXPECT_EQ("l.co.uk", domain_registry::MinimalPrivateSuffix(
      "a.b.c.d.e.f.g.h.i.j.k.l.co.uk"));

  // Check that we handle public suffixes that are not tlds.
  EXPECT_EQ("example.appspot.com",
            domain_registry::MinimalPrivateSuffix("example.appspot.com"));
  EXPECT_EQ(
      "example.appspot.com",
      domain_registry::MinimalPrivateSuffix("www.example.appspot.com"));

  // If a tld doesn't exist, again fail secure.
  EXPECT_EQ(
      "a.b.c.this.doesntexist",
      domain_registry::MinimalPrivateSuffix("a.b.c.this.doesntexist"));

  // Check that we don't give errors on various kinds of invalid hostnames.
  EXPECT_EQ("com", domain_registry::MinimalPrivateSuffix("com"));
  EXPECT_EQ("", domain_registry::MinimalPrivateSuffix(""));
  EXPECT_EQ(".", domain_registry::MinimalPrivateSuffix("."));
  EXPECT_EQ("..", domain_registry::MinimalPrivateSuffix(".."));
  EXPECT_EQ("..doesntexist.",
            domain_registry::MinimalPrivateSuffix("..doesntexist."));
}


}  // namespace

}  // namespace net_instaweb
