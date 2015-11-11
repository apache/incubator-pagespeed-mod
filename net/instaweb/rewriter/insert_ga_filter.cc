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

// Authors: nforman@google.com (Naomi Forman)
//          jefftk@google.com (Jeff Kaufman)
//
// Implements the insert_ga_snippet filter, which inserts the Google Analytics
// tracking snippet into html pages.  When experiments are enabled, also inserts
// snippets to report experiment status back.

#include "net/instaweb/rewriter/public/insert_ga_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/util/re2.h"

namespace {

// Name for statistics variable.
const char kInsertedGaSnippets[] = "inserted_ga_snippets";

}  // namespace

namespace net_instaweb {

// This filter primarily exists to support PageSpeed experiments that report
// back to Google Analytics for reporting.  You can also use it just to insert
// the Google Analytics tracking snippet, though.
//
// GA had a rewrite recently, switching from ga.js to analytics.js with a new
// API.  They also released support for content experiments.  The older style of
// reporting is to use a custom variable.  This filter can report to a content
// experiment with either ga.js or analytics.js; with ga.js reporting to a
// custom variable is still supported.
//
// If no snippet is present on the page then PageSpeed will insert one.
// Additionally, if you're running an experiment then PageSpeed will insert the
// JS necessary to report details back to GA.  This can look like any of these
// three things:
//
// ga.js + custom variables:
//   <script>kGAExperimentSnippet
//           kGAJsSnippet</script> [ possibly existing ]
//
// ga.js + content experiments:
//   <script src="kContentExperimentsJsClientUrl"></script>
//   <script>kContentExperimentsSetChosenVariantSnippet
//           kGAJsSnippet</script> [ possibly existing ]
//
// analytics.js + content experiments:
//   <script>kAnalyticsJsSnippet</script> [ possibly existing ]
//   kContentExperimentsSetExpAndVariantSnippet goes inside the analytics js
//   snippet, just before the location identified by kSendPageviewRegexp.

// Google Analytics snippet for setting experiment related variables.  Use with
// old ga.js and custom variable experiment reporting. Arguments are:
//   %s: Optional snippet to increase site speed tracking.
//   %u: Which ga.js custom variable to support to.
//   %s: Experiment spec string, shown in the GA UI.
extern const char kGAExperimentSnippet[] =
    "var _gaq = _gaq || [];"
    "%s"
    "_gaq.push(['_setCustomVar', %u, 'ExperimentState', '%s'"
    "]);";

// Google Analytics async snippet along with the _trackPageView call.
extern const char kGAJsSnippet[] =
    "if (window.parent == window) {"
    "var _gaq = _gaq || [];"
    "_gaq.push(['_setAccount', '%s']);"  // %s is the GA account number.
    "_gaq.push(['_setDomainName', '%s']);"  // %s is the domain name
    "_gaq.push(['_setAllowLinker', true]);"
    "%s"  // // Optional snippet to increase site speed tracking.
    "_gaq.push(['_trackPageview']);"
    "(function() {"
    "var ga = document.createElement('script'); ga.type = 'text/javascript';"
    "ga.async = true;"
    "ga.src = 'https//ssl.google-analytics.com/ga.js';"
    "var s = document.getElementsByTagName('script')[0];"
    "s.parentNode.insertBefore(ga, s);"
    "})();"
    "}";

// Google Universal analytics snippet.  First argument is the GA account number,
// second is kContentExperimentsSetExpAndVariantSnippet or nothing.
extern const char kAnalyticsJsSnippet[] =
    "if (window.parent == window) {"
    "(function(i,s,o,g,r,a,m){"
    "i['GoogleAnalyticsObject']=r;"
    "i[r]=i[r]||function(){"
    "(i[r].q=i[r].q||[]).push(arguments)},i[r].l=1*new Date();"
    "a=s.createElement(o), m=s.getElementsByTagName(o)[0];"
    "a.async=1;a.src=g;m.parentNode.insertBefore(a,m)"
    "})(window,document,'script',"
    "'//www.google-analytics.com/analytics.js','ga');"
    "ga('create', '%s', 'auto');"
    "%s"
    "ga('send', 'pageview');"
    "}";

// When using content experiments with ga.js you need to do a sychronous load
// of /cx/api.js first.
extern const char kContentExperimentsJsClientUrl[] =
    "//www.google-analytics.com/cx/api.js";

// When using content experimentsd with ga.js, after /cx/api.js has loaded and
// before ga.js loads you need to call this.  The first argument is the
// variant id, the second is the experiment id.
extern const char kContentExperimentsSetChosenVariantSnippet[] =
    "cxApi.setChosenVariant('%s', '%s');";

// When using content experimentsd with analytics.js, after ga('create', ..._)
// and before ga('[...].send', 'pageview'), identified with kSendPageviewRegexp,
// we need to insert:
extern const char kContentExperimentsSetExpAndVariantSnippet[] =
    "ga('set', 'expId', '%s');"
    "ga('set', 'expVar', '%s');";

// Set the sample rate to 100%.
// TODO(nforman): Allow this to be configurable through RewriteOptions.
// TODO(jefftk): set this when using analytics.js
extern const char kGASpeedTracking[] =
    "_gaq.push(['_setSiteSpeedSampleRate', 100]);";

// Matches ga('send', 'pageview') plus all the optional extra stuff people are
// allowed to put in that command.  The offset of the first match group tells
// you where in the string it matched.
// TODO(jefftk): compile this
const char kSendPageviewRegexp[] =
    "(ga\\s*\\("
    "\\s*['\"]([^.,)]*.)?"
    "send['\"]\\s*,"
    "\\s*['\"]pageview['\"]"
    "\\s*[\\),])";

InsertGAFilter::InsertGAFilter(RewriteDriver* rewrite_driver)
    : CommonFilter(rewrite_driver),
      script_element_(NULL),
      added_analytics_js_(false),
      added_experiment_snippet_(false),
      ga_id_(rewrite_driver->options()->ga_id()),
      found_snippet_(false),
      increase_speed_tracking_(
          rewrite_driver->options()->increase_speed_tracking()),
      seen_ga_js_(false) {
  Statistics* stats = driver()->statistics();
  inserted_ga_snippets_count_ = stats->GetVariable(kInsertedGaSnippets);
  DCHECK(!ga_id_.empty()) << "Enabled ga insertion, but did not provide ga id.";
}

void InsertGAFilter::InitStats(Statistics* stats) {
  stats->AddVariable(kInsertedGaSnippets);
}

InsertGAFilter::~InsertGAFilter() {}

void InsertGAFilter::StartDocumentImpl() {
  found_snippet_ = false;
  script_element_ = NULL;
  added_analytics_js_ = false;
  added_experiment_snippet_ = false;
  if (driver()->options()->running_experiment()) {
    driver()->message_handler()->Message(
        kInfo, "run_experiment: %s",
        driver()->options()->ToExperimentDebugString().c_str());
  }
}

// Start looking for ga snippet.
void InsertGAFilter::StartElementImpl(HtmlElement* element) {
  if (!found_snippet_ && element->keyword() == HtmlName::kScript &&
      script_element_ == NULL) {
    script_element_ = element;
  }
}

// This isn't perfect but matches all the cases we've found.
InsertGAFilter::AnalyticsStatus InsertGAFilter::FindSnippetInScript(
    const GoogleString& s) {
  if (!seen_ga_js_ &&
      s.find("google-analytics.com/ga.js") != GoogleString::npos) {
    seen_ga_js_ = true;
  }
  if (s.find(StrCat("'", ga_id_, "'")) == GoogleString::npos &&
      s.find(StrCat("\"", ga_id_, "\"")) == GoogleString::npos) {
    return kNoSnippetFound;
  }
  if (s.find(".google-analytics.com/urchin.js") != GoogleString::npos) {
    return kUnusableSnippetFound;  // urchin.js is too old.
  } else if (s.find(".google-analytics.com/ga.js") != GoogleString::npos) {
    if (s.find("_setAccount") != GoogleString::npos) {
      return kGaJs;  // Asynchronous ga.js
    }
    return kUnusableSnippetFound;
  } else if (seen_ga_js_ &&
             s.find("_getTracker") != GoogleString::npos &&
             s.find("_trackPageview") != GoogleString::npos) {
    // Synchronous ga.js was split over two script tags: first one to do the
    // loading then one to do the initiailization and page tracking.  We want to
    // process the second one.
    return kGaJs;  // Syncronous ga.js
  } else if (s.find(".google-analytics.com/analytics.js")) {
    if (RE2::PartialMatch(s, kSendPageviewRegexp)) {
      return kAnalyticsJs;
    }
    return kUnusableSnippetFound;
  }
  return kNoSnippetFound;
}

void InsertGAFilter::AddScriptNode(HtmlElement* current_element,
                                   const GoogleString& text,
                                   const GoogleString& url,
                                   bool append_child) {
  DCHECK(text.empty() != url.empty()) <<
      "Exactly one of text/url should be set.";

  HtmlElement* parent;
  if (append_child) {
    parent = current_element;
  } else {
    parent = current_element->parent();
    if (parent == NULL) {
      LOG(INFO) << "Null parent in insert_ga can't insert following node.";
      return;
    }
  }

  HtmlElement* script_element = driver()->NewElement(
      parent, HtmlName::kScript);
  script_element->set_style(HtmlElement::EXPLICIT_CLOSE);
  driver()->AddAttribute(script_element, HtmlName::kType,
                         "text/javascript");
  driver()->AppendChild(parent, script_element);

  if (text.empty()) {
    driver()->AddAttribute(script_element, HtmlName::kSrc, url);
  } else {
    HtmlNode* snippet =
        driver()->NewCharactersNode(script_element, text);
    driver()->AppendChild(script_element, snippet);
  }
}

GoogleString InsertGAFilter::AnalyticsJsExperimentSnippet() {
  if (driver()->options()->is_content_experiment()) {
    return StringPrintf(
        kContentExperimentsSetExpAndVariantSnippet,
        driver()->options()->content_experiment_id().c_str(),
        driver()->options()->content_experiment_variant_id().c_str());
  }
  LOG(WARNING) << "Experiment framework requires a content experiment when"
      " used with analytics.js.";
  return "";
}

// Handle the end of a body tag.
// If we've already inserted any GA snippet or if we found a GA
// snippet in the original page, don't do anything.
// If we haven't found anything, and haven't inserted anything yet,
// insert the GA js snippet.
// Caveat: The snippet should ideally be placed in <head> for accurate
// collection of data (e.g. pageviews etc.). We place it at the end of the
// <body> tag so that we won't add duplicate analytics js code for any page.
// For pages which don't already have analytics js, this might result in some
// data being lost.
void InsertGAFilter::HandleEndBody(HtmlElement* body) {
  // There is a chance (e.g. if there are two body tags), that we have
  // already inserted the snippet.  In that case, don't do it again.
  if (added_analytics_js_ || found_snippet_) {
    return;
  }

  // No snippets have been found, and we haven't added any snippets yet, so add
  // one now.  Include experiment setup if experiments are on.

  GoogleString js_text;
  GoogleString experiment_snippet;
  if (driver()->options()->use_analytics_js()) {
    if (ShouldInsertExperimentTracking()) {
      experiment_snippet = AnalyticsJsExperimentSnippet();
    }
    js_text = StringPrintf(
        kAnalyticsJsSnippet, ga_id_.c_str(), experiment_snippet.c_str());
  } else {
    if (ShouldInsertExperimentTracking()) {
      if (driver()->options()->is_content_experiment()) {
        AddScriptNode(body,
                      "" /* external script; no text */,
                      kContentExperimentsJsClientUrl,
                      true /* append_child */);
        experiment_snippet = StringPrintf(
            kContentExperimentsSetChosenVariantSnippet,
            driver()->options()->content_experiment_variant_id().c_str(),
            driver()->options()->content_experiment_id().c_str());
      } else {
        experiment_snippet = StringPrintf(
            kGAExperimentSnippet,
            "" /* don't change speed tracking here, we add it below */,
            driver()->options()->experiment_ga_slot(),
            driver()->options()->ToExperimentString().c_str());
      }
    }

    // Domain for this html page.
    GoogleString domain = driver()->google_url().Host().as_string();
    GoogleString speed_tracking =
        increase_speed_tracking_ ? kGASpeedTracking : "";
    js_text = StrCat(experiment_snippet,
                     StringPrintf(kGAJsSnippet,
                                  ga_id_.c_str(),
                                  domain.c_str(),
                                  speed_tracking.c_str()));
  }
  AddScriptNode(body, js_text,
                "" /* inline script; no url */,
                true /* append_child */);
  added_analytics_js_ = true;
  inserted_ga_snippets_count_->Add(1);
  return;
}

bool InsertGAFilter::ShouldInsertExperimentTracking() {
  if (driver()->options()->running_experiment()) {
    int experiment_state = driver()->options()->experiment_id();
    if (experiment_state != experiment::kExperimentNotSet &&
        experiment_state != experiment::kNoExperiment) {
      return true;
    }
  }
  return false;
}

void InsertGAFilter::RewriteInlineScript(HtmlCharactersNode* characters) {
  AnalyticsStatus analytics_status =
      FindSnippetInScript(characters->contents());
  if (analytics_status == kNoSnippetFound) {
    return;  // This inline script isn't for GA; nothing to change.
  }

  found_snippet_ = true;

  if (ShouldInsertExperimentTracking()) {
    if (analytics_status == kUnusableSnippetFound) {
      LOG(INFO) << "Page contains unusual Google Analytics snippet that we're"
          " not able to modify to add experiment tracking.";
    } else if (analytics_status == kAnalyticsJs) {
      GoogleString snippet_text = AnalyticsJsExperimentSnippet();
      if (!snippet_text.empty()) {
        // We want to find the index of ga('send', 'pageview') in the buffer
        // so we can insert before it.  JS is very flexible, so we need to use
        // a regexp to find it.  The way you get the match offset out of RE2
        // is to give it a char* and then look at the start pointer of the
        // StringPiece it gives back.  Yes, really.
        GoogleString* script = characters->mutable_contents();
        const char* tmp_script_cstr = script->c_str();
        Re2StringPiece ga_send_pageview;
        bool found_match = RE2::PartialMatch(
            tmp_script_cstr, kSendPageviewRegexp, &ga_send_pageview);
        CHECK(found_match);  // FindSnippetInScript already found this.
        int match_offset = ga_send_pageview.data() - tmp_script_cstr;
        script->insert(match_offset, snippet_text);

        added_experiment_snippet_ = true;
      }
    } else if (analytics_status == kGaJs) {
      if (driver()->options()->is_content_experiment()) {
        // The API for content experiments with ga.js unfortunately requires
        // a synchronous script load first.  Ideally people would switch to
        // analytics.js, which doesn't have this problem, but we need to
        // support people who haven't switched as well.
        //
        // We can't do InsertBeforeCurrent here, because we could be in the
        // horrible case where "<script>" has been flushed and now we're
        // rewriting the script body.  So the best we can do is:
        // * Blank out this script.
        // * Append the blocking external script load.
        // * Append the edited body of the original script tag as a new
        //   inline script.
        postponed_script_body_ = characters->contents();
        characters->mutable_contents()->clear();
      } else {
        GoogleString speed_tracking =
            increase_speed_tracking_ ? kGASpeedTracking : "";
        GoogleString snippet_text = StringPrintf(
            kGAExperimentSnippet,
            speed_tracking.c_str(),
            driver()->options()->experiment_ga_slot(),
            driver()->options()->ToExperimentString().c_str());
        GoogleString* script = characters->mutable_contents();
        // Prepend snippet_text to the script block.
        script->insert(0, snippet_text);
        added_experiment_snippet_ = true;
      }
    }
  }
}

// If RewriteInlineScript decided to insert any new script nodes, do that
// insertion here.
void InsertGAFilter::HandleEndScript(HtmlElement* script) {
  if (!postponed_script_body_.empty()) {
    DCHECK(script == script_element_);
    GoogleString snippet_text = StringPrintf(
        kContentExperimentsSetChosenVariantSnippet,
        driver()->options()->content_experiment_variant_id().c_str(),
        driver()->options()->content_experiment_id().c_str());

    AddScriptNode(script,
                  "" /* external script; no text */,
                  kContentExperimentsJsClientUrl,
                  false /* append_child */);
    AddScriptNode(script,
                  StrCat(snippet_text, postponed_script_body_),
                  "" /* inline script; no url */,
                  false /* append_child */);
    added_experiment_snippet_ = true;
    postponed_script_body_.clear();
  }
  script_element_ = NULL;
}

void InsertGAFilter::EndElementImpl(HtmlElement* element) {
  if (ga_id_.empty()) {
    // We only DCHECK that it's non-empty above, but there's nothing useful we
    // can do if it hasn't been set.  Checking here means we'll make no changes.
    return;
  }
  if (element->keyword() == HtmlName::kBody) {
    HandleEndBody(element);
  }
  if (element->keyword() == HtmlName::kScript) {
    HandleEndScript(element);
  }
}

void InsertGAFilter::Characters(HtmlCharactersNode* characters) {
  if (script_element_ != NULL && !found_snippet_ &&
      !added_experiment_snippet_) {
    RewriteInlineScript(characters);
  }
}

}  // namespace net_instaweb
