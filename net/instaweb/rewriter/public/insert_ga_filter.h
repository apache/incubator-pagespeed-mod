/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: nforman@google.com (Naomi Forman)
//
// This provides the InsertGAFilter class which adds a Google Analytics
// snippet to html pages.

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"

namespace net_instaweb {

class HtmlCharactersNode;
class HtmlElement;
class RewriteDriver;
class Statistics;
class Variable;

// Visible only for use in tests.
extern const char kGAExperimentSnippet[];
extern const char kGAJsSnippet[];
extern const char kAnalyticsJsSnippet[];
extern const char kContentExperimentsJsClientUrl[];
extern const char kContentExperimentsSetChosenVariantSnippet[];
extern const char kContentExperimentsSetExpAndVariantSnippet[];
extern const char kGASpeedTracking[];

// This class is the implementation of the insert_ga filter, which handles:
// * Adding a Google Analytics snippet to html pages.
// * Adding js to report experiment data to Google Analytics.
class InsertGAFilter : public CommonFilter {
 public:
  enum AnalyticsStatus {
    kGaJs,                  // Traditional ga.js or urchin.js.
    kAnalyticsJs,           // New "universal analytics" analytics.js.
    kNoSnippetFound,        // Didn't find either.
    kUnusableSnippetFound,  // There's a snippet on the page, but it's unusual
                            // and we can't work with it.
  };

  explicit InsertGAFilter(RewriteDriver* rewrite_driver);
  virtual ~InsertGAFilter();

  // Set up statistics for this filter.
  static void InitStats(Statistics* stats);

  virtual void StartDocumentImpl();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  // HTML Events we expect to be in <script> elements.
  virtual void Characters(HtmlCharactersNode* characters);

  virtual const char* Name() const { return "InsertGASnippet"; }

 private:
  // Construct the custom variable part for experiment of the GA snippet.
  GoogleString ConstructExperimentSnippet() const;

  // If appropriate, insert the GA snippet at the end of the body element.
  void HandleEndBody(HtmlElement* body);

  // If RewriteInlineScript left work to do, finish it now.
  void HandleEndScript(HtmlElement* body);

  // Adds a script node either as a child of the current_element or immediately
  // after current element.  Exactly one of text/url should be set: text for
  // inline js, url for external.
  void AddScriptNode(HtmlElement* current_element,
                     const GoogleString& text,
                     const GoogleString& url,
                     bool insert_immediately_after_current);

  // Handle the body of a script tag.
  // * Look for a GA snippet in the script and record the findings so that we
  //   can optionally add the analytics js at the end of the body if no GA
  //   snippet is present on the page.
  // * If a snippet is present, modify it to add experiment tracking.
  void RewriteInlineScript(HtmlCharactersNode* characters);

  // Indicates whether or not the string contains a GA snippet with the
  // same id as ga_id_, and if so whether it's new-style or old-style.
  //
  // Expects to be called on every script in the document.  Non-const because it
  // needs to use seen_ga_js_ to hold state: whether something is a ga.js
  // snippet depends in part on whether we've already seen a ga.js library load.
  AnalyticsStatus FindSnippetInScript(const GoogleString& s);

  // Determine the snippet of JS we need to log a content experiment to
  // analytics.js.  If no content experiment is configured, log a warning
  // and return "".
  GoogleString AnalyticsJsExperimentSnippet();

  bool ShouldInsertExperimentTracking();

  // Stats on how many tags we moved.
  Variable* inserted_ga_snippets_count_;

  // Script element we're currently in, so we can check it to see if
  // it has the GA snippet already.
  HtmlElement* script_element_;
  // Whether we added the analytics js or not.
  bool added_analytics_js_;
  // Whether we added the experiment snippet or not.
  bool added_experiment_snippet_;

  // GA ID for this site.
  GoogleString ga_id_;

  // Indicates whether or not we've already found a GA snippet so we know
  // whether we need to insert one.
  bool found_snippet_;

  // Increase site-speed tracking to the max allowed.
  bool increase_speed_tracking_;

  // The synchronous usage of ga.js is split over two tags: one to load the
  // library then one to use it.  This is set to true if we've seen something
  // that might be the library load.
  bool seen_ga_js_;

  // RewriteInlineScript runs to process the body of the GA JS inline script.
  // Sometimes it needs to save text for later to be added as a new script body
  // when it gets the end element event for the script.
  GoogleString postponed_script_body_;

  DISALLOW_COPY_AND_ASSIGN(InsertGAFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_INSERT_GA_FILTER_H_
