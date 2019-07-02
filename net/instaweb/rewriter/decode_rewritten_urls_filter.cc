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

#include "net/instaweb/rewriter/public/decode_rewritten_urls_filter.h"

#include <memory>

#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/log_record.h"

namespace net_instaweb {

DecodeRewrittenUrlsFilter::~DecodeRewrittenUrlsFilter() {
}

void DecodeRewrittenUrlsFilter::StartElement(HtmlElement* element) {
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    StringPiece url(attributes[i].url->DecodedValueOrNull());
    if (url.empty() || IsDataUrl(url)) {
      continue;
    }
    GoogleUrl gurl(driver_->base_url(), url);
    if (gurl.IsWebValid()) {
      StringVector decoded_url;
      if (driver_->DecodeUrl(gurl, &decoded_url)) {
        // An encoded URL.
        if (decoded_url.size() == 1) {
          driver_->log_record()->SetRewriterLoggingStatus(
              RewriteOptions::FilterId(RewriteOptions::kDecodeRewrittenUrls),
              RewriterApplication::APPLIED_OK);
          // Replace attribute's url with the decoded one.
          attributes[i].url->SetValue(decoded_url.at(0));
        } else {
          // A combined encoded URL.
          // TODO(sriharis):  What can we do?  Creating elements for each
          // constituent (that are other wise identical to 'element')?
          driver_->log_record()->SetRewriterLoggingStatus(
              RewriteOptions::FilterId(RewriteOptions::kDecodeRewrittenUrls),
              RewriterApplication::NOT_APPLIED);
        }
      }
    }
  }
}

}  // namespace net_instaweb
