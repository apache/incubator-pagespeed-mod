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


#include "pagespeed/kernel/html/elide_attributes_filter.h"

#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

class ElideAttributesFilterTest : public HtmlParseTestBase {
 protected:
  ElideAttributesFilterTest()
      : elide_attributes_filter_(&html_parse_) {
    html_parse_.AddFilter(&elide_attributes_filter_);
  }

  virtual bool AddBody() const { return false; }

 private:
  ElideAttributesFilter elide_attributes_filter_;

  DISALLOW_COPY_AND_ASSIGN(ElideAttributesFilterTest);
};

TEST_F(ElideAttributesFilterTest, NoChanges) {
  ValidateNoChanges("no_changes",
                    "<head><script src=\"foo.js\"></script></head>"
                    "<body><form method=\"post\">"
                    "<input type=\"checkbox\" checked>"
                    "</form></body>");
}

TEST_F(ElideAttributesFilterTest, RemoveAttrWithDefaultValue) {
  ValidateExpected("remove_attr_with_default_value",
                   "<head></head><body><form method=get></form></body>",
                   "<head></head><body><form></form></body>");
}

TEST_F(ElideAttributesFilterTest, RemoveValueFromAttr) {
  SetDoctype("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
  ValidateExpected("remove_value_from_attr",
                   "<head></head><body><form>"
                   "<input type=checkbox checked=checked></form></body>",
                   "<head></head><body><form>"
                   "<input type=checkbox checked></form></body>");
}

TEST_F(ElideAttributesFilterTest, DoNotRemoveValueFromAttrInXhtml) {
  SetDoctype(kXhtmlDtd);
  ValidateNoChanges("do_not_remove_value_from_attr_in_xhtml",
                    "<head></head><body><form>"
                    "<input type=checkbox checked=checked></form></body>");
}

TEST_F(ElideAttributesFilterTest, DoNotBreakVBScript) {
  SetDoctype("<!doctype html>");
  ValidateExpected("do_not_break_vbscript",
                   "<head><script language=\"JavaScript\">var x=1;</script>"
                   "<script language=\"VBScript\">"
                   "Sub foo(ByVal bar)\n  call baz(bar)\nend sub"
                   "</script></head><body></body>",
                   // Remove language="JavaScript", but not the VBScript one:
                   "<head><script>var x=1;</script>"
                   "<script language=\"VBScript\">"
                   "Sub foo(ByVal bar)\n  call baz(bar)\nend sub"
                   "</script></head><body></body>");
}

TEST_F(ElideAttributesFilterTest, RemoveScriptTypeInHtml5) {
  SetDoctype("<!doctype html>");
  ValidateExpected("remove_script_type_in_html_5",
                   "<head><script src=\"foo.js\" type=\"text/javascript\">"
                   "</script></head><body></body>",
                   "<head><script src=\"foo.js\">"
                   "</script></head><body></body>");
}

// See http://github.com/apache/incubator-pagespeed-mod/issues/59
TEST_F(ElideAttributesFilterTest, DoNotRemoveScriptTypeInHtml4) {
  SetDoctype("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
  ValidateNoChanges("do_not_remove_script_type_in_html_4",
                    "<head><script src=\"foo.js\" type=\"text/javascript\">"
                    "</script></head><body></body>");
}

// Wordpress uses CSS selectors on type=text attributes in inputs, so don't
// remove it.
TEST_F(ElideAttributesFilterTest, DoNotRemoveTypeAttribute) {
  SetDoctype("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" "
             "\"http://www.w3.org/TR/html4/strict.dtd\">");
  ValidateNoChanges("do_not_remove_type_attribute_from_input",
                    "<head></head><body><form>"
                    "<input type=text></form></body>");
}

}  // namespace net_instaweb
