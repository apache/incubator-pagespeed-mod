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


#include "net/instaweb/rewriter/public/rewritten_content_scanning_filter.h"

#include <memory>

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

const char
RewrittenContentScanningFilter::kNumProxiedRewrittenResourcesProperty[] =
    "num_proxied_rewritten_resources";

RewrittenContentScanningFilter::RewrittenContentScanningFilter(
    RewriteDriver* driver)
    : driver_(driver),
      num_proxied_rewritten_resources_(0) {}

RewrittenContentScanningFilter::~RewrittenContentScanningFilter() {
}

void RewrittenContentScanningFilter::StartDocument() {
  num_proxied_rewritten_resources_ = 0;
}

void RewrittenContentScanningFilter::EndDocument() {
  // Update number of rewritten resources in the property cahce.
  driver_->UpdatePropertyValueInDomCohort(
      driver_->property_page(),
      kNumProxiedRewrittenResourcesProperty,
      IntegerToString(num_proxied_rewritten_resources_));
}

void RewrittenContentScanningFilter::StartElement(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    StringPiece url(attributes[i].url->DecodedValueOrNull());
    if (url.empty()) {
      continue;
    }
    switch (attributes[i].category) {
      case semantic_type::kImage:
      case semantic_type::kScript:
      case semantic_type::kStylesheet:
      case semantic_type::kOtherResource: {
        GoogleUrl gurl(driver_->base_url(), url);
        if (driver_->server_context()->url_namer()->IsProxyEncoded(gurl)) {
          ++num_proxied_rewritten_resources_;
        }
        break;
      }
      case semantic_type::kPrefetch:
      case semantic_type::kHyperlink:
      case semantic_type::kUndefined:
        break;
    }
  }
}

}  // namespace net_instaweb
