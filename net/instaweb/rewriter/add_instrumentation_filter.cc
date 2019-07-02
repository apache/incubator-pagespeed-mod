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


#include "net/instaweb/rewriter/public/add_instrumentation_filter.h"

#include "base/logging.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/request_timing_info.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "pagespeed/kernel/base/escaping.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

// The javascript tag to insert in the top of the <head> element.  We want this
// as early as possible in the html.  It must be short and fast.
const char kHeadScriptPedantic[] =
    "<script type='text/javascript'>"
    "window.mod_pagespeed_start = Number(new Date());"
    "</script>";

// script tag without type attribute
const char kHeadScriptNonPedantic[] =
    "<script>"
    "window.mod_pagespeed_start = Number(new Date());"
    "</script>";

}  // namespace

// Timing tag for total page load time.  Also embedded in kTailScriptFormat
// above via the second %s.
// TODO(jud): These values would be better set to "load" and "beforeunload".
const char AddInstrumentationFilter::kLoadTag[] = "load:";
const char AddInstrumentationFilter::kUnloadTag[] = "unload:";

// Counters.
const char AddInstrumentationFilter::kInstrumentationScriptAddedCount[] =
    "instrumentation_filter_script_added_count";
AddInstrumentationFilter::AddInstrumentationFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      found_head_(false),
      added_head_script_(false),
      added_unload_script_(false) {
  Statistics* stats = driver->server_context()->statistics();
  instrumentation_script_added_count_ = stats->GetVariable(
      kInstrumentationScriptAddedCount);
}

AddInstrumentationFilter::~AddInstrumentationFilter() {}

void AddInstrumentationFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kInstrumentationScriptAddedCount);
}

void AddInstrumentationFilter::StartDocumentImpl() {
  found_head_ = false;
  added_head_script_ = false;
  added_unload_script_ = false;
}

void AddInstrumentationFilter::AddHeadScript(HtmlElement* element) {
  // IE doesn't like tags other than title or meta at the start of the
  // head. The MSDN page says:
  //   The X-UA-Compatible header isn't case sensitive; however, it must appear
  //   in the header of the webpage (the HEAD section) before all other elements
  //   except for the title element and other meta elements.
  // Reference: http://msdn.microsoft.com/en-us/library/jj676915(v=vs.85).aspx
  if (element->keyword() != HtmlName::kTitle &&
      element->keyword() != HtmlName::kMeta) {
    added_head_script_ = true;
    // TODO(abliss): add an actual element instead, so other filters can
    // rewrite this JS
    HtmlCharactersNode* script = nullptr;
    if (driver()->options()->Enabled(RewriteOptions::kPedantic)) {
        script = driver()->NewCharactersNode(nullptr, kHeadScriptPedantic);
    } else {
        script = driver()->NewCharactersNode(nullptr, kHeadScriptNonPedantic);
    }
    driver()->InsertNodeBeforeCurrent(script);
    instrumentation_script_added_count_->Add(1);
  }
}

void AddInstrumentationFilter::StartElementImpl(HtmlElement* element) {
  if (found_head_ && !added_head_script_) {
    AddHeadScript(element);
  }
  if (!found_head_ && element->keyword() == HtmlName::kHead) {
    found_head_ = true;
  }
}

void AddInstrumentationFilter::EndElementImpl(HtmlElement* element) {
  if (found_head_ && element->keyword() == HtmlName::kHead) {
    if (!added_head_script_) {
      AddHeadScript(element);
    }
    if (driver()->options()->report_unload_time() &&
        !added_unload_script_) {
      GoogleString js = GetScriptJs(kUnloadTag);
      HtmlElement* script = driver()->NewElement(element, HtmlName::kScript);
      if (!driver()->defer_instrumentation_script()) {
        driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                               StringPiece());
      }
      driver()->InsertNodeBeforeCurrent(script);
      AddJsToElement(js, script);
      added_unload_script_ = true;
    }
  }
}

void AddInstrumentationFilter::EndDocument() {
  // We relied on the existence of a <head> element.  This should have been
  // assured by add_head_filter.
  if (!found_head_) {
    LOG(WARNING) << "No <head> found for URL " << driver()->url();
    return;
  }
  GoogleString js = GetScriptJs(kLoadTag);
  HtmlElement* script = driver()->NewElement(nullptr, HtmlName::kScript);
  if (!driver()->defer_instrumentation_script()) {
    driver()->AddAttribute(script, HtmlName::kDataPagespeedNoDefer,
                           StringPiece());
  }
  InsertNodeAtBodyEnd(script);
  AddJsToElement(js, script);
}

GoogleString AddInstrumentationFilter::GetScriptJs(StringPiece event) {
  GoogleString js;
  StaticAssetManager* static_asset_manager =
      driver()->server_context()->static_asset_manager();
  // Only add the static JS once.
  if (!added_unload_script_) {
    if (driver()->options()->enable_extended_instrumentation()) {
      js = static_asset_manager->GetAsset(
          StaticAssetEnum::EXTENDED_INSTRUMENTATION_JS, driver()->options());
    }
    StrAppend(&js, static_asset_manager->GetAsset(
        StaticAssetEnum::ADD_INSTRUMENTATION_JS, driver()->options()));
  }

  GoogleString js_event = (event == kLoadTag) ? "load" : "beforeunload";

  const RewriteOptions::BeaconUrl& beacons = driver()->options()->beacon_url();
  const GoogleString* beacon_url =
      driver()->IsHttps() ? &beacons.https : &beacons.http;
  GoogleString extra_params;
  if (driver()->options()->running_experiment()) {
    int experiment_state = driver()->options()->experiment_id();
    if (experiment_state != experiment::kExperimentNotSet &&
        experiment_state != experiment::kNoExperiment) {
      StrAppend(&extra_params, "&exptid=",
                IntegerToString(driver()->options()->experiment_id()));
    }
  }

  const RequestTimingInfo& timing_info =
      driver()->request_context()->timing_info();
  int64 header_fetch_ms;
  if (timing_info.GetFetchHeaderLatencyMs(&header_fetch_ms)) {
    // If time taken to fetch the http header is not set, then the response
    // came from cache.
    StrAppend(&extra_params, "&hft=", Integer64ToString(header_fetch_ms));
  }
  int64 fetch_ms;
  if (timing_info.GetFetchLatencyMs(&fetch_ms)) {
    // If time taken to fetch the resource is not set, then the response
    // came from cache.
    StrAppend(&extra_params, "&ft=", Integer64ToString(fetch_ms));
  }
  int64 ttfb_ms;
  if (timing_info.GetTimeToFirstByte(&ttfb_ms)) {
    StrAppend(&extra_params, "&s_ttfb=", Integer64ToString(ttfb_ms));
  }

  // Append the http response code.
  if (driver()->response_headers() != nullptr &&
      driver()->response_headers()->status_code() > 0 &&
      driver()->response_headers()->status_code() != HttpStatus::kOK) {
    StrAppend(&extra_params, "&rc=", IntegerToString(
        driver()->response_headers()->status_code()));
  }
  // Append the request id.
  if (driver()->request_context()->request_id() > 0) {
    StrAppend(&extra_params, "&id=", Integer64ToString(
        driver()->request_context()->request_id()));
  }

  GoogleString html_url;
  EscapeToJsStringLiteral(driver()->google_url().Spec(),
                          false, /* no quotes */
                          &html_url);

  StrAppend(&js, "\npagespeed.addInstrumentationInit(");
  StrAppend(&js, "'", *beacon_url, "', ");
  StrAppend(&js, "'", js_event, "', ");
  StrAppend(&js, "'", extra_params, "', ");
  StrAppend(&js, "'", html_url, "');");

  return js;
}

void AddInstrumentationFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(!driver()->request_properties()->IsBot());
}

}  // namespace net_instaweb
