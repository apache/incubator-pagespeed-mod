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


#include "net/instaweb/rewriter/public/debug_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/critical_images_finder.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"

namespace {

const int64 kTimeNotSet = -1;

}  // namespace

namespace net_instaweb {

DebugFilter::Event::Event() {
  Clear();
}

void DebugFilter::Event::Clear() {
  start_us_ = kTimeNotSet;
  duration_us_ = 0;
  total_us_ = 0;
}

void DebugFilter::Event::Start(int64 now_us) {
  DCHECK_EQ(kTimeNotSet, start_us_);
  start_us_ = now_us;
}

void DebugFilter::Event::End(int64 now_us) {
  DCHECK_NE(kTimeNotSet, start_us_);
  duration_us_ += now_us - start_us_;
  start_us_ = kTimeNotSet;
}

void DebugFilter::Event::AddToTotal() {
  DCHECK_EQ(kTimeNotSet, start_us_);
  total_us_ += duration_us_;
  duration_us_ = 0;
}

DebugFilter::DebugFilter(RewriteDriver* driver)
    : driver_(driver),
      timer_(driver->server_context()->timer()) {
  Clear();
  driver_->SetDynamicallyDisabledFilterList(&dynamically_disabled_filter_list_);
}

DebugFilter::~DebugFilter() {}

void DebugFilter::Clear() {
  num_flushes_ = 0;
  end_document_seen_ = false;
  idle_.Clear();
  parse_.Clear();
  render_.Clear();
  start_doc_time_us_ = kTimeNotSet;
  flush_messages_.clear();
  critical_image_urls_.clear();
  dynamically_disabled_filter_list_.clear();
}

void DebugFilter::InitParse() {
  Clear();
  start_doc_time_us_ = timer_->NowUs();
  idle_.Start(start_doc_time_us_);
}

void DebugFilter::StartParse() {
  int64 now_us = timer_->NowUs();
  idle_.End(now_us);
  parse_.Start(now_us);
}

void DebugFilter::EndParse() {
  int64 now_us = timer_->NowUs();
  parse_.End(now_us);
  idle_.Start(now_us);
}

void DebugFilter::StartRender() {
  int64 now_us = timer_->NowUs();
  idle_.End(now_us);
  render_.Start(now_us);
}

GoogleString DebugFilter::FormatFlushMessage(int64 time_since_init_parse_us,
                                             int64 parse_duration_us,
                                             int64 render_duration_us,
                                             int64 idle_duration_us) {
  // This format is designed for easy searching in View->Page Source.
  return StrCat(
      "\n"
      "#Flush after     ", Integer64ToString(time_since_init_parse_us), "us\n"
      "#Parse duration  ", Integer64ToString(parse_duration_us), "us\n"
      "#Render duration ", Integer64ToString(render_duration_us), "us\n",
      StrCat(
          "#Idle duration   ",  Integer64ToString(idle_duration_us), "us\n"));
}

GoogleString DebugFilter::FormatEndDocumentMessage(
    int64 time_since_init_parse_us, int64 total_parse_duration_us,
    int64 total_render_duration_us, int64 total_idle_duration_us,
    int num_flushes, bool is_critical_images_beacon_enabled,
    const StringSet& critical_image_urls,
    const StringVector& dynamically_disabled_filter_list) {
  // This format is designed for easy searching in View->Page Source.
  GoogleString str = StrCat(
      "\n"
      "#NumFlushes            ",
      IntegerToString(num_flushes),
      "\n"
      "#EndDocument after     ",
      Integer64ToString(time_since_init_parse_us),
      "us\n"
      "#Total Parse duration  ",
      Integer64ToString(total_parse_duration_us), "us\n");
  StrAppend(&str, "#Total Render duration ",
            Integer64ToString(total_render_duration_us),
            "us\n"
            "#Total Idle duration   ",
            Integer64ToString(total_idle_duration_us), "us\n");

  if (is_critical_images_beacon_enabled) {
    if (critical_image_urls.empty()) {
      StrAppend(&str, "No critical images detected.\n");
    } else {
      StrAppend(&str, "Critical Images:\n\t",
                JoinCollection(critical_image_urls, "\n\t"), "\n");
    }
  }

  if (dynamically_disabled_filter_list.empty()) {
    StrAppend(&str, "No filters were disabled for this request.\n");
  } else {
    StrAppend(&str, "The following filters were disabled for this request:\n\t",
              JoinCollection(dynamically_disabled_filter_list, "\n\t"), "\n");
  }
  return str;
}

GoogleString DebugFilter::ListActiveFiltersAndOptions() const {
  const RewriteOptions* options = driver_->options();
  GoogleString settings_list("\nmod_pagespeed on\nFilters:\n");
  StrAppend(&settings_list, options->EnabledFiltersToString());
  StrAppend(&settings_list, "\nOptions:\n",
            options->SafeEnabledOptionsToString());
  return settings_list;
}

void DebugFilter::EndElement(HtmlElement* element) {
  if (!flush_messages_.empty()) {
    driver_->InsertComment(flush_messages_);
    flush_messages_.clear();
  }

  // Keep track of critical images to print out the list of them at the end. We
  // can't just use the critical images stored in the property cache since it
  // stores the image hashes, not the full URLs.
  if (element->keyword() == HtmlName::kImg) {
    // Check if the lazyload filter has modified the original src attribute, and
    // if so, use the original src.
    HtmlElement::Attribute* src;
    if ((src = element->FindAttribute(HtmlName::kDataPagespeedLazySrc)) !=
            NULL ||
        (src = element->FindAttribute(HtmlName::kSrc)) != NULL) {
      GoogleUrl gurl(driver_->base_url(),
                     StringPiece(src->DecodedValueOrNull()));
      GoogleString url_str = gurl.UncheckedSpec().as_string();
      CriticalImagesFinder* finder =
          driver_->server_context()->critical_images_finder();
      if (finder->IsHtmlCriticalImage(url_str, driver_)) {
        critical_image_urls_.insert(url_str);
      }
    }
  }
}

void DebugFilter::Flush() {
  int64 time_since_init_parse_us = render_.start_us() - start_doc_time_us_;
  int64 now_us = timer_->NowUs();

  // We get a special StartRender call from RewriteDriver, but we just use
  // our Flush event to detect EndRender.
  render_.End(now_us);

  // Only print a FLUSH message if there is at least one mid-document;
  // we don't need to print a FLUSH message at the end of the document
  // if there were no other flushes, the summary is sufficient.
  if ((num_flushes_ > 0) || !end_document_seen_) {
    GoogleString flush_message = FormatFlushMessage(time_since_init_parse_us,
                                                    parse_.duration_us(),
                                                    render_.duration_us(),
                                                    idle_.duration_us());
    // If a <style> block spans multiple flushes, calling InsertComment here
    // will return false, since we can't insert safely into a literal block.
    // Instead, buffer the messages, and then print when we reach the closing
    // tag (in EndElement).
    if (!driver_->InsertComment(flush_message)) {
      StrAppend(&flush_messages_, flush_message);
    }
  }

  // Capture the flush-durations in the grand totals to be emitted at
  // end of document.
  parse_.AddToTotal();
  render_.AddToTotal();
  idle_.AddToTotal();

  if (end_document_seen_) {
    driver_->InsertComment(
        StrCat(ListActiveFiltersAndOptions(),
               FormatEndDocumentMessage(
                   time_since_init_parse_us, parse_.total_us(),
                   render_.total_us(), idle_.total_us(), num_flushes_,
                   driver_->is_critical_images_beacon_enabled(),
                   critical_image_urls_, dynamically_disabled_filter_list_)));
  } else {
    // We don't count the flush at end-of-document because that is automatically
    // called by RewriteDriver/HtmlParse, and is not initiated from upstream,
    // e.g. from PHP $flush.
    ++num_flushes_;

    // Restart the idle-time now that the Flush is over.
    idle_.Start(now_us);
  }
}

void DebugFilter::EndDocument() {
  // Despite the tempting symmetry, we can't call idle_.End(...) here because
  // this actually gets called during Rendering, when we are not idle.
  end_document_seen_ = true;
}

}  // namespace net_instaweb
