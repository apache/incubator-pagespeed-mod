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


#include "pagespeed/kernel/html/remove_comments_filter.h"

#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/fast_wildcard_group.h"
#include "pagespeed/kernel/base/gtest.h"

namespace net_instaweb {

namespace {

// Basic default implementation.
class OptionsImpl : public RemoveCommentsFilter::OptionsInterface {
  public:
  OptionsImpl() {}

  void RetainComment(const StringPiece& comment) {
    retain_comments_.Allow(comment);
  }

  virtual bool IsRetainedComment(const StringPiece& comment) const {
    return retain_comments_.Match(comment, false);
  }

  private:
  FastWildcardGroup retain_comments_;

  DISALLOW_COPY_AND_ASSIGN(OptionsImpl);
};

}  // namespace

class RemoveCommentsFilterTest : public HtmlParseTestBase {
 protected:
  RemoveCommentsFilterTest()
      : options_(new OptionsImpl()),
        remove_comments_filter_(&html_parse_, options_) {
    html_parse_.AddFilter(&remove_comments_filter_);
  }

  virtual bool AddBody() const { return false; }

 protected:
  // NOTE: The options_ instance is owned by the
  // remove_comments_filter_ instance.
  OptionsImpl* options_;
  RemoveCommentsFilter remove_comments_filter_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveCommentsFilterTest);
};

TEST_F(RemoveCommentsFilterTest, NoComments) {
  ValidateNoChanges("no_comments",
                    "<head><title>Hello</title></head>"
                    "<body>Why, hello there!</body>");
}

TEST_F(RemoveCommentsFilterTest, RemoveComment) {
  ValidateExpected("remove_comment",
                   "<body>hello <!--world--></body>",
                   "<body>hello </body>");
}

TEST_F(RemoveCommentsFilterTest, RemoveMultipleComments) {
  ValidateExpected("remove_multiple_comments",
                   "<head><!--1--><title>Hi</title><!--2--></head>"
                   "<body><!--3-->hello<!--4--><!--5--></body>",
                   "<head><title>Hi</title></head>"
                   "<body>hello</body>");
}

TEST_F(RemoveCommentsFilterTest, DoNotRemoveIEDirective) {
  ValidateNoChanges("do_not_remove_ie_directive",
                    "<body>hello <!--[if IE 8]>world<![endif]--></body>");
}

TEST_F(RemoveCommentsFilterTest, Retain) {
  options_->RetainComment("*google_ad_section_*");
  ValidateNoChanges("do_not_remove_ad_section",
                    "<body>hello <!-- google_ad_section_start --></body>");
  ValidateExpected("remove_comment_not_matching_retained",
                   "<body>hello <!--world--></body>",
                   "<body>hello </body>");
}

TEST_F(RemoveCommentsFilterTest, CommentInTag) {
  ValidateExpected("comment_in_tag", "<div><!--</div>-->", "<div>");
}

TEST_F(RemoveCommentsFilterTest, CommentInXmp) {
  ValidateNoChanges("comment_in_xmp", "<xmp><!-- keep me --></xmp>");
  ValidateNoChanges("comment_in_overlapping_xmp", "<xmp><!--</xmp>-->");
}

}  // namespace net_instaweb
