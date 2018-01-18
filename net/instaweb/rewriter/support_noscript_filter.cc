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

#include "net/instaweb/rewriter/public/support_noscript_filter.h"

#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"

namespace net_instaweb {

SupportNoscriptFilter::SupportNoscriptFilter(RewriteDriver* rewrite_driver)
    : rewrite_driver_(rewrite_driver),
      should_insert_noscript_(true) {
}

SupportNoscriptFilter::~SupportNoscriptFilter() {
}

void SupportNoscriptFilter::DetermineEnabled(GoogleString* disabled_reason) {
  // Insert a NOSCRIPT tag only if at least one of the filters requiring
  // JavaScript for execution is enabled.
  should_insert_noscript_ = IsAnyFilterRequiringScriptExecutionEnabled();
  set_is_enabled(should_insert_noscript_);
}

void SupportNoscriptFilter::StartElement(HtmlElement* element) {
  if (should_insert_noscript_ && element->keyword() == HtmlName::kBody) {
    scoped_ptr<GoogleUrl> url_with_psa_off(
        rewrite_driver_->google_url().CopyAndAddQueryParam(
            RewriteQuery::kPageSpeed, RewriteQuery::kNoscriptValue));
    GoogleString escaped_url;
    HtmlKeywords::Escape(url_with_psa_off->Spec(), &escaped_url);
    // TODO(sriharis): Replace the usage of HtmlCharactersNode with HtmlElement
    // and Attribute.
    HtmlCharactersNode* noscript_node = rewrite_driver_->NewCharactersNode(
        element, StringPrintf(kNoScriptRedirectFormatter,
                              escaped_url.c_str(), escaped_url.c_str()));
    rewrite_driver_->PrependChild(element, noscript_node);
    should_insert_noscript_ = false;
  }
  // TODO(sriharis):  Handle the case where there is no body -- insert a body in
  // EndElement of kHtml?
}

bool SupportNoscriptFilter::IsAnyFilterRequiringScriptExecutionEnabled() const {
  const RewriteOptions* options = rewrite_driver_->options();
  const RequestProperties* request_properties =
      rewrite_driver_->request_properties();
  RewriteOptions::FilterVector js_filters;
  options->GetEnabledFiltersRequiringScriptExecution(&js_filters);
  for (int i = 0, n = js_filters.size(); i < n; ++i) {
    RewriteOptions::Filter filter = js_filters[i];
    bool filter_enabled = true;
    switch (filter) {
      case RewriteOptions::kDeferIframe:
      case RewriteOptions::kDeferJavascript:
        filter_enabled = request_properties->SupportsJsDefer(
            options->enable_aggressive_rewriters_for_mobile());
        break;
      case RewriteOptions::kDedupInlinedImages:
      case RewriteOptions::kDelayImages:
      case RewriteOptions::kLazyloadImages:
      case RewriteOptions::kLocalStorageCache:
        filter_enabled = request_properties->SupportsImageInlining();
        break;
      case RewriteOptions::kMobilize:
        filter_enabled = false;
        break;
      default:
        break;
      }
    if (filter_enabled) {
      return true;
    }
  }
  return false;
}

}  // namespace net_instaweb
