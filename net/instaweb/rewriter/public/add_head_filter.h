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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ADD_HEAD_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ADD_HEAD_FILTER_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {
class HtmlElement;
class HtmlParse;

// Guarantees there is a head element in HTML. This enables downstream
// filters to assume that there will be a head.
class AddHeadFilter : public EmptyHtmlFilter {
 public:
  explicit AddHeadFilter(HtmlParse* parser, bool combine_multiple_heads);
  virtual ~AddHeadFilter();

  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndDocument();
  virtual void EndElement(HtmlElement* element);
  virtual void Flush();
  virtual const char* Name() const { return "AddHead"; }

 private:
  HtmlParse* html_parse_;
  bool combine_multiple_heads_;
  bool found_head_;
  HtmlElement* head_element_;

  DISALLOW_COPY_AND_ASSIGN(AddHeadFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ADD_HEAD_FILTER_H_
