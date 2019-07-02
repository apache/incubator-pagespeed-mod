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


#include "pagespeed/kernel/html/canonical_attributes.h"

#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {

class HtmlParse;

CanonicalAttributes::CanonicalAttributes(HtmlParse* html_parse)
    : html_parse_(html_parse),
      num_changes_(0),
      num_errors_(0) {
}

CanonicalAttributes::~CanonicalAttributes() {}

void CanonicalAttributes::StartDocument() {
  num_changes_ = 0;
  num_errors_ = 0;
}

void CanonicalAttributes::StartElement(HtmlElement* element) {
  HtmlElement::AttributeList* attrs = element->mutable_attributes();
  for (HtmlElement::AttributeIterator i(attrs->begin());
       i != attrs->end(); ++i) {
    HtmlElement::Attribute& attribute = *i;
    const char* value = attribute.DecodedValueOrNull();
    if (attribute.decoding_error()) {
      ++num_errors_;
    } else if (value != NULL) {
      ++num_changes_;

      // Recomputes escaped attribute.
      attribute.SetValue(value);
    }
  }
}

}  // namespace net_instaweb
