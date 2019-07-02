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


#ifndef PAGESPEED_KERNEL_HTML_HTML_ATTRIBUTE_QUOTE_REMOVAL_H_
#define PAGESPEED_KERNEL_HTML_HTML_ATTRIBUTE_QUOTE_REMOVAL_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/html/empty_html_filter.h"

namespace net_instaweb {
// Very simple html filter that removes quotes from attributes that don't need
// them.
//
// From http://www.w3.org/TR/REC-html40/intro/sgmltut.html#h-3.2.2:
// In certain cases, authors may specify the value of an attribute without any
// quotation marks. The attribute value may only contain letters (a-z and A-Z),
// digits (0-9), hyphens (ASCII decimal 45), periods (ASCII decimal 46),
// underscores (ASCII decimal 95), and colons (ASCII decimal 58).
//
// This is an experiment, to see if quote removal *actually* saves bandwidth.
// After compression it may not (or may not save enough).  In that case we
// should not bother with quote removal.

class HtmlElement;
class HtmlParse;

class HtmlAttributeQuoteRemoval : public EmptyHtmlFilter {
 public:
  explicit HtmlAttributeQuoteRemoval(HtmlParse* html_parse);
  virtual ~HtmlAttributeQuoteRemoval();
  // Given context in object, does attribute value val require quotes?
  bool NeedsQuotes(const char *val);
  virtual void StartElement(HtmlElement* element);
  // # of quote pairs removed from attributes in *all* documents processed.
  int total_quotes_removed() const {
    return total_quotes_removed_;
  }

  virtual const char* Name() const { return "HtmlAttributeQuoteRemoval"; }

 private:
  int total_quotes_removed_;
  HtmlParse* html_parse_;
  bool needs_no_quotes_[256];  // lookup chars for quotability
  // should be const, but C++ initializer rules are broken.

  DISALLOW_COPY_AND_ASSIGN(HtmlAttributeQuoteRemoval);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_HTML_HTML_ATTRIBUTE_QUOTE_REMOVAL_H_
