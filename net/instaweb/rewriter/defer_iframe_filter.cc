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


#include "net/instaweb/rewriter/public/defer_iframe_filter.h"

#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

const char DeferIframeFilter::kDeferIframeInit[] =
    "pagespeed.deferIframeInit();";
const char DeferIframeFilter::kDeferIframeIframeJs[] =
    "\npagespeed.deferIframe.convertToIframe();";

DeferIframeFilter::DeferIframeFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      static_asset_manager_(
          driver->server_context()->static_asset_manager()),
      script_inserted_(false) {}

DeferIframeFilter::~DeferIframeFilter() {
}

void DeferIframeFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(driver()->request_properties()->SupportsJsDefer(
      driver()->options()->enable_aggressive_rewriters_for_mobile()));
}

void DeferIframeFilter::StartDocumentImpl() {
  script_inserted_ = false;
}

void DeferIframeFilter::StartElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    return;
  }
  if (element->keyword() == HtmlName::kIframe) {
    if (!script_inserted_) {
      HtmlElement* script = driver()->NewElement(element->parent(),
                                                HtmlName::kScript);
      driver()->InsertNodeBeforeNode(element, script);

      GoogleString js = StrCat(
          static_asset_manager_->GetAsset(
              StaticAssetEnum::DEFER_IFRAME, driver()->options()),
              kDeferIframeInit);
      AddJsToElement(js, script);
      script_inserted_ = true;
    }
    element->set_name(driver()->MakeName(HtmlName::kPagespeedIframe));
  }
}

void DeferIframeFilter::EndElementImpl(HtmlElement* element) {
  if (noscript_element() != NULL) {
    return;
  }
  if (element->keyword() == HtmlName::kPagespeedIframe) {
    HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
    driver()->AddAttribute(script, HtmlName::kType, "text/javascript");
    HtmlCharactersNode* script_content = driver()->NewCharactersNode(
        script, kDeferIframeIframeJs);
    driver()->AppendChild(element, script);
    driver()->AppendChild(script, script_content);
  }
}

}  // namespace net_instaweb
