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


#include <memory>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/flush_html_filter.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace {

// Controls the number of resource references that will be scanned before a
// Flush is issued.
//
// TODO(jmarantz): Make these configurable via RewriteOptions.
// TODO(jmarantz): Consider gaps in realtime as justification to induce flushes
// as well.  That might be beyond the scope of this filter.
const int kFlushScoreThreshold = 80;
const int kFlushCssScore = 10;     // 8 CSS files induces a flush.
const int kFlushScriptScore = 10;  // 8 Scripts files induces a flush.
const int kFlushImageScore = 2;    // 40 images induces a flush.

}  // namespace

namespace net_instaweb {

class HtmlElement;

FlushHtmlFilter::FlushHtmlFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      score_(0) {
}

FlushHtmlFilter::~FlushHtmlFilter() {}

void FlushHtmlFilter::StartDocumentImpl() {
  score_ = 0;
}

void FlushHtmlFilter::Flush() {
  score_ = 0;
}

void FlushHtmlFilter::StartElementImpl(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver()->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    switch (attributes[i].category) {
      case semantic_type::kStylesheet:
        score_ += kFlushCssScore;
        break;
      case semantic_type::kScript:
        score_ += kFlushScriptScore;
        break;
      case semantic_type::kImage:
        score_ += kFlushImageScore;
        break;
      default:
        break;
    }
  }
}

void FlushHtmlFilter::EndElementImpl(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver()->options(), &attributes);
  if (!attributes.empty() && score_ >= kFlushScoreThreshold) {
    score_ = 0;
    driver()->RequestFlush();
  }
}

}  // namespace net_instaweb
