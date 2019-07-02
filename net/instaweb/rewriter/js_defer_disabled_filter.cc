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


#include "net/instaweb/rewriter/public/js_defer_disabled_filter.h"

#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "pagespeed/kernel/base/null_message_handler.h"

namespace net_instaweb {

JsDeferDisabledFilter::JsDeferDisabledFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
}

JsDeferDisabledFilter::~JsDeferDisabledFilter() { }

void JsDeferDisabledFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(ShouldApply(driver()));
}

bool JsDeferDisabledFilter::ShouldApply(RewriteDriver* driver) {
  return driver->request_properties()->SupportsJsDefer(
      driver->options()->enable_aggressive_rewriters_for_mobile());
}

void JsDeferDisabledFilter::InsertJsDeferCode() {
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  const RewriteOptions* options = driver()->options();
  // Insert script node with deferJs code as outlined.
  HtmlElement* defer_js_url_node =
      driver()->NewElement(NULL, HtmlName::kScript);
  driver()->AddAttribute(defer_js_url_node, HtmlName::kType,
                                "text/javascript");
  driver()->AddAttribute(
      defer_js_url_node, HtmlName::kSrc,
      static_asset_manager->GetAssetUrl(StaticAssetEnum::DEFER_JS, options));

  InsertNodeAtBodyEnd(defer_js_url_node);
}

void JsDeferDisabledFilter::EndDocument() {
  if (!ShouldApply(driver())) {
    return;
  }

  InsertJsDeferCode();
}

}  // namespace net_instaweb
