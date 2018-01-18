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


#include "net/instaweb/rewriter/public/css_move_to_head_filter.h"

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace {

// Names for Statistics variables.
const char kCssElementsMoved[] = "css_elements_moved";

}  // namespace

namespace net_instaweb {

CssMoveToHeadFilter::CssMoveToHeadFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      move_css_to_head_(
          driver->options()->Enabled(RewriteOptions::kMoveCssToHead)),
      move_css_above_scripts_(
          driver->options()->Enabled(RewriteOptions::kMoveCssAboveScripts)) {
  Statistics* stats = driver->statistics();
  css_elements_moved_ = stats->GetVariable(kCssElementsMoved);
}

CssMoveToHeadFilter::~CssMoveToHeadFilter() {}

void CssMoveToHeadFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCssElementsMoved);
}

void CssMoveToHeadFilter::StartDocumentImpl() {
  move_to_element_ = nullptr;
}

void CssMoveToHeadFilter::EndElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* href;
  const char* media;
  if (move_to_element_ == nullptr) {
    // We record the first we see, either </head> or <script>. That will be
    // the anchor for where to move all styles.
    if (move_css_to_head_ &&
        element->keyword() == HtmlName::kHead) {
      move_to_element_ = element;
      element_is_head_ = true;
    } else if (move_css_above_scripts_ &&
               element->keyword() == HtmlName::kScript) {
      move_to_element_ = element;
      element_is_head_ = false;
    }
  } else if (element->keyword() == HtmlName::kStyle ||
             CssTagScanner::ParseCssElement(element, &href, &media)) {
    if (noscript_element() != nullptr ||
        (element->keyword() == HtmlName::kStyle &&
         element->FindAttribute(HtmlName::kScoped) != nullptr)) {
      // Do not move anything out of a <noscript> element, and do not move
      // <style scoped> elements.  These act as a barrier preventing subsequent
      // styles from moving to head.
      move_to_element_ = nullptr;
    } else {
      css_elements_moved_->Add(1);
      // MoveCurrent* methods will check that that we are allowed to move these
      // elements into the approriate places.
      if (element_is_head_) {
        // Move styles to end of head.
        driver()->MoveCurrentInto(move_to_element_);
      } else {
        // Move styles directly before that first script.
        driver()->MoveCurrentBefore(move_to_element_);
      }
    }
  }
}

}  // namespace net_instaweb
