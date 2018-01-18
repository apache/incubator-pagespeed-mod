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


#include "net/instaweb/rewriter/public/strip_scripts_filter.h"

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"

namespace net_instaweb {

class StripScriptsFilterTest : public HtmlParseTestBase {
 protected:
  StripScriptsFilterTest()
      : strip_scripts_filter_(&html_parse_) {
    html_parse_.AddFilter(&strip_scripts_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  StripScriptsFilter strip_scripts_filter_;

  DISALLOW_COPY_AND_ASSIGN(StripScriptsFilterTest);
};

TEST_F(StripScriptsFilterTest, RemoveScriptSrc) {
  ValidateExpected("remove_script_src",
                   "<head><script src='http://www.google.com/javascript"
                   "/ajax_apis.js'></script></head><body>Hello, world!</body>",
                   "<head></head><body>Hello, world!</body>");
}

TEST_F(StripScriptsFilterTest, RemoveScriptInline) {
  ValidateExpected("remove_script_src",
                   "<head><script>alert('Alert, alert!')"
                   "</script></head><body>Hello, world!</body>",
                   "<head></head><body>Hello, world!</body>");
}

}  // namespace net_instaweb
