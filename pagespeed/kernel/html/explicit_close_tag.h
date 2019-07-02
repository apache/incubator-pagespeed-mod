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


#ifndef PAGESPEED_KERNEL_HTML_EXPLICIT_CLOSE_TAG_H_
#define PAGESPEED_KERNEL_HTML_EXPLICIT_CLOSE_TAG_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {

class HtmlElement;

// Makes every tag explicitly or briefly closed so that when we re-serialize
// we can see the structure as interpreted by the parser.
//
// This is intended for validation & unit-testing so that we can see
// the DOM-structure output of the parser in the serialized output.
// In general we will not want to turn this filter on in production
// because it makes the HTML bigger.
class ExplicitCloseTag : public EmptyHtmlFilter {
 public:
  ExplicitCloseTag() {}
  virtual ~ExplicitCloseTag();

  virtual void EndElement(HtmlElement* element);
  virtual const char* Name() const { return "ExplicitCloseTag"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExplicitCloseTag);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_EXPLICIT_CLOSE_TAG_H_
