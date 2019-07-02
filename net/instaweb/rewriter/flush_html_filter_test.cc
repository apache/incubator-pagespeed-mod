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


// Unit-test the FlushHtmlFilter.

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

namespace {

const char kCssFormat[] = "<link rel='stylesheet' href='%s' type='text/css'/>";
const char kImgFormat[] = "<img src='%s'/>";
const char kScriptFormat[] = "<script type=text/javascript src='%s'></script>";

class FlushFilterTest : public RewriteTestBase  {
 protected:
  virtual void SetUp() {
    options()->set_flush_html(true);
    RewriteTestBase::SetUp();
    rewrite_driver()->AddFilters();
    SetupWriter();
    html_parse()->StartParse("http://example.com");
  }

  virtual void TearDown() {
    html_parse()->FinishParse();
    RewriteTestBase::TearDown();
  }
};

TEST_F(FlushFilterTest, NoExtraFlushes) {
  html_parse()->ParseText(StrCat(StringPrintf(kCssFormat, "a.css"),
                                 StringPrintf(kImgFormat, "b.png")));
  html_parse()->ExecuteFlushIfRequested();
  EXPECT_EQ(0, server_context()->rewrite_stats()->num_flushes()->Get());
}

TEST_F(FlushFilterTest, InduceFlushes) {
  GoogleString lots_of_links;
  for (int i = 0; i < 7; ++i) {
    StrAppend(&lots_of_links,
              StringPrintf(kCssFormat, "a.css"));
  }
  StrAppend(&lots_of_links,
            StringPrintf(kScriptFormat, "b.js"));
  html_parse()->ParseText(lots_of_links);
  html_parse()->ExecuteFlushIfRequested();
  EXPECT_EQ(1, server_context()->rewrite_stats()->num_flushes()->Get());
}

TEST_F(FlushFilterTest, NotEnoughToInduceFlushes) {
  GoogleString lots_of_links;
  for (int i = 0; i < 7; ++i) {
    StrAppend(&lots_of_links,
              StringPrintf(kCssFormat, "a.css"));
  }
  StrAppend(&lots_of_links,
            StringPrintf(kImgFormat, "b.png"));
  html_parse()->ParseText(lots_of_links);
  html_parse()->ExecuteFlushIfRequested();
  EXPECT_EQ(0, server_context()->rewrite_stats()->num_flushes()->Get());
}

}  // namespace

}  // namespace net_instaweb
