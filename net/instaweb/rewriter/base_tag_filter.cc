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


#include "public/base_tag_filter.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

namespace net_instaweb {

BaseTagFilter::~BaseTagFilter() {}

void BaseTagFilter::StartElement(HtmlElement* element) {
  if ((element->keyword() == HtmlName::kHead) && !added_base_tag_) {
    added_base_tag_ = true;
    HtmlElement* new_element = driver_->NewElement(element, HtmlName::kBase);
    driver_->AddAttribute(new_element, HtmlName::kHref,
                          driver_->decoded_base());
    driver_->InsertNodeAfterCurrent(new_element);
  }
}

}  // namespace net_instaweb
