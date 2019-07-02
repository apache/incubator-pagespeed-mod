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


#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlCdataNode;
class HtmlCommentNode;
class HtmlDirectiveNode;
class HtmlElement;
class HtmlIEDirectiveNode;

EmptyHtmlFilter::EmptyHtmlFilter() {
}

EmptyHtmlFilter::~EmptyHtmlFilter() {
}

void EmptyHtmlFilter::StartDocument() {
}

void EmptyHtmlFilter::EndDocument() {
}

void EmptyHtmlFilter::StartElement(HtmlElement* element) {
}

void EmptyHtmlFilter::EndElement(HtmlElement* element) {
}

void EmptyHtmlFilter::Cdata(HtmlCdataNode* cdata) {
}

void EmptyHtmlFilter::Comment(HtmlCommentNode* comment) {
}

void EmptyHtmlFilter::IEDirective(HtmlIEDirectiveNode* directive) {
}

void EmptyHtmlFilter::Characters(HtmlCharactersNode* characters) {
}

void EmptyHtmlFilter::Directive(HtmlDirectiveNode* directive) {
}

void EmptyHtmlFilter::Flush() {
}

void EmptyHtmlFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(true);
}

}  // namespace net_instaweb
