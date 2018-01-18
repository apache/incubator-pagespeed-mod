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


// Unit-test the resource slot comparator.

#include "net/instaweb/rewriter/public/resource_slot.h"

#include <set>
#include <utility>  // for std::pair

#include "net/instaweb/rewriter/public/data_url_input_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"               // for StrCat
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_writer_filter.h"
#include "pagespeed/kernel/http/content_type.h"

namespace {

static const char kHtmlUrl[] = "http://html.parse.test/event_list_test.html";

}  // namespace

namespace net_instaweb {

class ResourceSlotTest : public RewriteTestBase {
 protected:
  virtual bool AddBody() const { return false; }

  virtual void SetUp() {
    RewriteTestBase::SetUp();

    // Set up 4 slots for testing.
    RewriteDriver* driver = rewrite_driver();
    driver->AddFilters();
    ASSERT_TRUE(driver->StartParseId(kHtmlUrl, "resource_slot_test",
                                     kContentTypeHtml));
    elements_[0] = driver->NewElement(NULL, HtmlName::kLink);
    driver->AddAttribute(elements_[0], HtmlName::kHref, "v1");
    driver->AddAttribute(elements_[0], HtmlName::kSrc, "v2");
    elements_[1] = driver->NewElement(NULL, HtmlName::kLink);
    driver->AddAttribute(element(1), HtmlName::kHref, "v3");
    driver->AddAttribute(element(1), HtmlName::kSrc, "v4");

    driver->AddElement(element(0), 1);
    driver->CloseElement(element(0), HtmlElement::BRIEF_CLOSE, 1);
    driver->AddElement(element(1), 2);
    driver->CloseElement(element(1), HtmlElement::BRIEF_CLOSE, 3);

    slots_[0] = MakeSlot(0, 0);
    slots_[1] = MakeSlot(0, 1);
    slots_[2] = MakeSlot(1, 0);
    slots_[3] = MakeSlot(1, 1);
  }

  virtual void TearDown() {
    rewrite_driver()->FinishParse();
    RewriteTestBase::TearDown();
  }

  HtmlResourceSlotPtr MakeSlot(int element_index, int attribute_index) {
    ResourcePtr empty;
    HtmlResourceSlot* slot = new HtmlResourceSlot(
        empty, element(element_index),
        attribute(element_index, attribute_index),
        html_parse());
    return HtmlResourceSlotPtr(slot);
  }

  bool InsertAndReturnTrueIfAdded(const HtmlResourceSlotPtr& slot) {
    std::pair<HtmlResourceSlotSet::iterator, bool> p = slot_set_.insert(slot);
    return p.second;
  }

  int num_slots() const { return slot_set_.size(); }
  const HtmlResourceSlotPtr slot(int index) const { return slots_[index]; }
  HtmlElement* element(int index) { return elements_[index]; }
  HtmlElement::Attribute* attribute(int element_index, int attribute_index) {
    HtmlElement* el = element(element_index);
    HtmlElement::AttributeList* attrs = el->mutable_attributes();
    int pos = 0;
    for (net_instaweb::HtmlElement::AttributeIterator i(attrs->begin());
         i != attrs->end(); ++i, ++pos) {
      if (pos == attribute_index) {
        return i.Get();
      }
    }
    return NULL;
  }

  GoogleString GetHtmlDomAsString() {
    output_buffer_.clear();
    html_parse()->ApplyFilter(html_writer_filter_.get());
    return output_buffer_;
  }

 private:
  HtmlResourceSlotSet slot_set_;
  HtmlResourceSlotPtr slots_[4];
  HtmlElement* elements_[2];
};

TEST_F(ResourceSlotTest, Accessors) {
  EXPECT_EQ(element(0), slot(0)->element());
  EXPECT_EQ(attribute(0, 0), slot(0)->attribute());
  EXPECT_EQ(element(0), slot(1)->element());
  EXPECT_EQ(attribute(0, 1), slot(1)->attribute());
  EXPECT_EQ(element(1), slot(2)->element());
  EXPECT_EQ(attribute(1, 0), slot(2)->attribute());
  EXPECT_EQ(element(1), slot(3)->element());
  EXPECT_EQ(attribute(1, 1), slot(3)->attribute());
  EXPECT_FALSE(slot(0)->was_optimized());
  slot(0)->set_was_optimized(true);
  EXPECT_TRUE(slot(0)->was_optimized());

  EXPECT_EQ("resource_slot_test:1", slot(0)->LocationString());
  EXPECT_EQ("resource_slot_test:2-3", slot(2)->LocationString());

  const char kDataUrl[] = "data:text/plain,Huh";
  ResourcePtr resource =
      DataUrlInputResource::Make(kDataUrl, rewrite_driver());
  ResourceSlotPtr fetch_slot(new FetchResourceSlot(resource));
  EXPECT_EQ(StrCat("Fetch of ", kDataUrl), fetch_slot->LocationString());
}

TEST_F(ResourceSlotTest, Comparator) {
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(InsertAndReturnTrueIfAdded(slot(i)));
  }
  EXPECT_EQ(4, num_slots());

  // Adding an equivalent slot should fail and leave the number of remembered
  // slots unchanged.
  ResourcePtr empty;
  HtmlResourceSlotPtr s4_dup(MakeSlot(1, 1));
  EXPECT_FALSE(InsertAndReturnTrueIfAdded(s4_dup))
      << "s4_dup is equivalent to slots_[3] so it should not add to the set";
  EXPECT_EQ(4, num_slots());
}

// Tests that a slot resource-update has the desired effect on the DOM.
TEST_F(ResourceSlotTest, RenderUpdate) {
  SetupWriter();

  // Before update: first href=v1.
  EXPECT_EQ("<link href=\"v1\" src=\"v2\"/><link href=\"v3\" src=\"v4\"/>",
            GetHtmlDomAsString());

  GoogleUrl gurl("http://html.parse.test/new_css.css");
  bool unused;
  ResourcePtr updated(rewrite_driver()->CreateInputResource(
      gurl, RewriteDriver::InputRole::kStyle, &unused));
  slot(0)->SetResource(updated);
  slot(0)->Render();

  // After update: first href=new_css.css. Note: that we relativize the URL.
  EXPECT_EQ(
      "<link href=\"new_css.css\" src=\"v2\"/><link href=\"v3\" src=\"v4\"/>",
      GetHtmlDomAsString());
}

// Tests that a slot deletion takes effect as expected.
TEST_F(ResourceSlotTest, RenderDelete) {
  SetupWriter();

  // Before update: first link is present.
  EXPECT_EQ("<link href=\"v1\" src=\"v2\"/><link href=\"v3\" src=\"v4\"/>",
            GetHtmlDomAsString());

  EXPECT_FALSE(slot(0)->should_delete_element());
  EXPECT_FALSE(slot(0)->disable_further_processing());
  slot(0)->RequestDeleteElement();
  EXPECT_TRUE(slot(0)->should_delete_element());
  EXPECT_TRUE(slot(0)->disable_further_processing());
  slot(0)->Render();

  // After update, first link is gone.
  EXPECT_EQ("<link href=\"v3\" src=\"v4\"/>", GetHtmlDomAsString());
}

}  // namespace net_instaweb
