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


#include "pagespeed/kernel/html/html_attribute_quote_removal.h"

#include "pagespeed/kernel/html/doctype.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_parse.h"
#include "pagespeed/kernel/base/string.h"

namespace {

// Explicit about signedness because we are
// loading a 0-indexed lookup table.
const unsigned char kNoQuoteChars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789-._:";

}  // namespace

// TODO(jmaessen): Make adjustable.
const bool kLogQuoteRemoval = false;

namespace net_instaweb {
// Remove quotes; see description in .h file.

HtmlAttributeQuoteRemoval::HtmlAttributeQuoteRemoval(HtmlParse* html_parse)
    : total_quotes_removed_(0),
      html_parse_(html_parse) {
  // In pidgin Python:
  //    needs_no_quotes[:] = false
  //    needs_no_quotes[kNoQuoteChars] = true

  // TODO(jmarantz): put this in a static Initialize method to avoid
  // per-request construction costs.
  memset(&needs_no_quotes_, 0, sizeof(needs_no_quotes_));
  for (int i = 0; kNoQuoteChars[i] != '\0'; ++i) {
    needs_no_quotes_[kNoQuoteChars[i]] = true;
  }

  // All 8-bit characters can remain unquoted.
  // TODO(jmarantz): uncomment in a follow-up.  This should be fine.
  // for (int i = 128; i < 256; ++i) {
  //   needs_no_quotes_[i] = true;
  // }
}

HtmlAttributeQuoteRemoval::~HtmlAttributeQuoteRemoval() {}

bool HtmlAttributeQuoteRemoval::NeedsQuotes(const char *val) {
  bool needs_quotes = false;
  int i = 0;
  if (val != NULL) {
    for (; val[i] != '\0'; ++i) {
      // Explicit cast to unsigned char ensures that our offset
      // into needs_no_quotes_ is positive.
      needs_quotes = !needs_no_quotes_[static_cast<unsigned char>(val[i])];
      if (needs_quotes) {
        break;
      }
    }
  }
  // Note that due to inconsistencies in empty attribute parsing between Firefox
  // and Chrome (Chrome seems to parse the next thing it sees after whitespace
  // as the attribute value) we leave empty attributes intact.
  return needs_quotes || i == 0;
}

void HtmlAttributeQuoteRemoval::StartElement(HtmlElement* element) {
  // TODO(jmarantz): switch to using mimetype.  To do that we need to have
  // access to the RewriteDriver* to get the response-headers, and so this
  // is not compatible with PageSpeed Insights that uses this filter for
  // HTML minification.
  if (html_parse_->doctype().IsXhtml()) {
    return;  // XHTML doctypes require quotes, so don't remove any.
  }
  int rewritten = 0;
  HtmlElement::AttributeList* attrs = element->mutable_attributes();
  for (HtmlElement::AttributeIterator i(attrs->begin());
       i != attrs->end(); ++i) {
    HtmlElement::Attribute& attr = *i;
    if (attr.quote_style() != HtmlElement::NO_QUOTE &&
        !NeedsQuotes(attr.escaped_value())) {
      attr.set_quote_style(HtmlElement::NO_QUOTE);
      rewritten++;
    }
  }
  if (rewritten > 0) {
    total_quotes_removed_ += rewritten;
    if (kLogQuoteRemoval) {
      const char* plural = (rewritten == 1) ? "" : "s";
      html_parse_->InfoHere("Scrubbed quotes from %d attribute%s",
                            rewritten, plural);
    }
  }
}

}  // namespace net_instaweb
