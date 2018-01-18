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


#include "net/instaweb/rewriter/public/dom_stats_filter.h"

#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

DomStatsFilter::DomStatsFilter(RewriteDriver* driver)
  : CommonFilter(driver), script_tag_scanner_(driver) {
  Clear();
}

DomStatsFilter::~DomStatsFilter() {}

void DomStatsFilter::Clear() {
  num_img_tags_ = 0;
  num_inlined_img_tags_ = 0;
  num_external_css_ = 0;
  num_scripts_ = 0;
  num_critical_images_used_ = 0;
}

void DomStatsFilter::StartDocumentImpl() {
  Clear();
}

void DomStatsFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kImg) {
    ++num_img_tags_;

    HtmlElement::Attribute* src = element->FindAttribute(HtmlName::kSrc);
    StringPiece url(src == NULL ? "" : src->DecodedValueOrNull());
    if (!url.empty()) {
      if (IsDataUrl(url)) {
        ++num_inlined_img_tags_;
      } else {
        CriticalImagesFinder* finder =
            driver()->server_context()->critical_images_finder();
        if (finder->Available(driver()) == CriticalImagesFinder::kAvailable) {
          GoogleUrl image_gurl(driver()->base_url(), url);
          if (finder->IsHtmlCriticalImage(image_gurl.Spec(), driver())) {
            ++num_critical_images_used_;
          }
        }
      }
    }
  } else if (element->keyword() == HtmlName::kLink &&
      CssTagScanner::IsStylesheetOrAlternate(
          element->AttributeValue(HtmlName::kRel)) &&
      element->FindAttribute(HtmlName::kHref) != NULL) {
    ++num_external_css_;
  } else {
    HtmlElement::Attribute* src;
    if (script_tag_scanner_.ParseScriptElement(element, &src) ==
        ScriptTagScanner::kJavaScript) {
      ++num_scripts_;
    }
  }
}

}  // namespace net_instaweb
