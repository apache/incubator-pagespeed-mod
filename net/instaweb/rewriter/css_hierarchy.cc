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


#include "net/instaweb/rewriter/public/css_hierarchy.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_minify.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

const char CssHierarchy::kFailureReasonPrefix[] = "Flattening failed: ";

// Representation of a CSS with all the information required for import
// flattening, image rewriting, and minifying.

CssHierarchy::CssHierarchy(CssFilter* filter)
    : filter_(filter),
      parent_(NULL),
      charset_source_("from unknown"),
      input_contents_resolved_(false),
      flattening_succeeded_(true),
      unparseable_detected_(false),
      flattened_result_limit_(0),
      message_handler_(NULL) {
}

CssHierarchy::~CssHierarchy() {
  STLDeleteElements(&children_);
}

void CssHierarchy::InitializeRoot(const GoogleUrl& css_base_url,
                                  const GoogleUrl& css_trim_url,
                                  const StringPiece input_contents,
                                  bool has_unparseables,
                                  int64 flattened_result_limit,
                                  Css::Stylesheet* stylesheet,
                                  MessageHandler* message_handler) {
  css_base_url_.Reset(css_base_url);
  css_trim_url_.Reset(css_trim_url);
  input_contents_ = input_contents;
  stylesheet_.reset(stylesheet);
  unparseable_detected_ = has_unparseables;
  flattened_result_limit_ = flattened_result_limit;
  message_handler_ = message_handler;
}

void CssHierarchy::InitializeNested(const CssHierarchy& parent,
                                    const GoogleUrl& import_url) {
  css_base_url_.Reset(import_url);
  url_ = css_base_url_.Spec();
  parent_ = &parent;
  // These are invariant and propagate from our parent.
  css_trim_url_.Reset(parent.css_trim_url());
  flattened_result_limit_ = parent.flattened_result_limit_;
  message_handler_ = parent.message_handler_;
}

void CssHierarchy::set_stylesheet(Css::Stylesheet* stylesheet) {
  stylesheet_.reset(stylesheet);
}

void CssHierarchy::set_minified_contents(
    const StringPiece minified_contents) {
  minified_contents.CopyToString(&minified_contents_);
}

void CssHierarchy::ResizeChildren(int n) {
  int i = children_.size();
  if (i < n) {
    // Increase the number of elements, default construct each new one.
    children_.resize(n);
    for (; i < n; ++i) {
      children_[i] = new CssHierarchy(filter_);
    }
  } else if (i > n) {
    // Decrease the number of elements, deleting each discarded one.
    for (--i; i >= n; --i) {
      delete children_[i];
      children_[i] = NULL;
    }
    children_.resize(n);
  }
}

bool CssHierarchy::IsRecursive() const {
  for (const CssHierarchy* ancestor = parent_;
       ancestor != NULL; ancestor = ancestor->parent_) {
    if (ancestor->url_ == url_) {
      return true;
    }
  }
  return false;
}

bool CssHierarchy::DetermineImportMedia(const StringVector& containing_media,
                                        const StringVector& import_media) {
  bool result = true;
  media_ = import_media;
  css_util::ClearVectorIfContainsMediaAll(&media_);
  if (media_.empty()) {
    // Common case: no media specified on the @import (or equivalent explicit
    // 'all') so the caller can just use the containing media.
    media_ = containing_media;
  } else {
    // Media were specified for the @import so we need to determine the
    // minimum subset required relative to the containing media.
    std::sort(media_.begin(), media_.end());
    css_util::EliminateElementsNotIn(&media_, containing_media);
    if (media_.empty()) {
      result = false;  // The media have been reduced to nothing.
    }
  }
  return result;
}

bool CssHierarchy::DetermineRulesetMedia(StringVector* ruleset_media) {
  // Return true if the ruleset has to be written, false if not. It doesn't
  // have to be written if its applicable media are reduced to nothing.
  bool result = true;
  css_util::ClearVectorIfContainsMediaAll(ruleset_media);
  std::sort(ruleset_media->begin(), ruleset_media->end());
  if (!media_.empty()) {
    css_util::EliminateElementsNotIn(ruleset_media, media_);
    if (ruleset_media->empty()) {
      result = false;
    }
  }
  return result;
}

void CssHierarchy::AddFlatteningFailureReason(const GoogleString& reason) {
  if (!reason.empty()) {
    StringPiece trimmed_reason(reason);
    if (trimmed_reason.starts_with(kFailureReasonPrefix)) {
      trimmed_reason.remove_prefix(STATIC_STRLEN(kFailureReasonPrefix));
    }
    // Don't add the reason if we already have it.
    StringPiece current_reasons(flattening_failure_reason_);
    if (FindIgnoreCase(current_reasons, trimmed_reason) == StringPiece::npos) {
      if (flattening_succeeded_) {
        // This is an informational message only - no prefix required.
        if (!flattening_failure_reason_.empty()) {
          StrAppend(&flattening_failure_reason_, " AND ");
        }
      } else if (flattening_failure_reason_.empty()) {
        flattening_failure_reason_ = kFailureReasonPrefix;
      } else {
        if (FindIgnoreCase(current_reasons,
                           kFailureReasonPrefix) == StringPiece::npos) {
          flattening_failure_reason_ = StrCat(kFailureReasonPrefix,
                                              flattening_failure_reason_);
        }
        StrAppend(&flattening_failure_reason_, " AND ");
      }
      // Finally, add the new reason to whatever we have now.
      StrAppend(&flattening_failure_reason_, trimmed_reason);
    }
  }
}

bool CssHierarchy::CheckCharsetOk(const ResourcePtr& resource,
                                  GoogleString* failure_reason) {
  DCHECK(parent_ != NULL);
  // If we haven't already, determine the charset of this CSS;
  // per the CSS2.1 spec: 1st headers, 2nd @charset, 3rd owning document.
  if (charset_.empty()) {
    charset_ = resource->response_headers()->DetermineCharset();
    charset_source_ = "from headers";
  }
  if (charset_.empty() && !stylesheet()->charsets().empty()) {
    charset_ = UnicodeTextToUTF8(stylesheet()->charset(0));
    charset_source_ = "from an @charset";
  }
  if (charset_.empty()) {
    charset_ = parent_->charset();
    charset_source_ = "from the enclosing CSS";
    return true;  // Since the next if is now trivially false so we can skip it.
  }

  // Now check that it agrees with the owning document's charset since we
  // won't be able to change it in the final inlined CSS.
  if (!StringCaseEqual(charset_, parent_->charset())) {
    *failure_reason = "The charset of ";
    StrAppend(failure_reason, url_for_humans(), " (", charset_, " ",
              charset_source(), ")");
    StrAppend(failure_reason, " is different from that of its parent (",
              parent_->url_for_humans(), "): ", parent_->charset(), " ",
              parent_->charset_source());
    return false;
  }

  return true;
}

bool CssHierarchy::Parse() {
  bool result = true;
  if (stylesheet_.get() == NULL) {
    Css::Parser parser(input_contents_);
    parser.set_preservation_mode(true);
    parser.set_quirks_mode(false);
    Css::Stylesheet* stylesheet = parser.ParseRawStylesheet();
    // Any parser error is bad news but unparseable sections are OK because
    // any problem with an @import results in the error mask bit kImportError
    // being set.
    if (parser.errors_seen_mask() != Css::Parser::kNoError) {
      delete stylesheet;
      stylesheet = NULL;
    }
    if (stylesheet == NULL) {
      result = false;
    } else {
      // Note if we detected anything unparseable.
      if (parser.unparseable_sections_seen_mask() != Css::Parser::kNoError) {
        unparseable_detected_ = true;
      }
      // Reduce the media on the to-be merged rulesets to the minimum required,
      // deleting any rulesets that end up having no applicable media types.
      Css::Rulesets& rulesets = stylesheet->mutable_rulesets();
      for (Css::Rulesets::iterator iter = rulesets.begin();
           iter != rulesets.end(); ) {
        Css::Ruleset* ruleset = *iter;
        StringVector ruleset_media;
        // We currently do not allow flattening of any CSS files with @media
        // that have complex CSS3-version media queries. Only plain media
        // types (like "screen", "print" and "all") are allowed.
        if (css_util::ConvertMediaQueriesToStringVector(
                ruleset->media_queries(), &ruleset_media)) {
          if (DetermineRulesetMedia(&ruleset_media)) {
            css_util::ConvertStringVectorToMediaQueries(
                ruleset_media, &ruleset->mutable_media_queries());
            ++iter;
          } else {
            iter = rulesets.erase(iter);
            delete ruleset;
          }
        } else {
          // ruleset->media_queries() contained complex media queries.
          filter_->num_flatten_imports_complex_queries_->Add(1);
          // Note: This will leave the file partially stripped of rulesets
          // and partially unstripped. This shouldn't matter since we've
          // decided not to flatten this CSS file, but worth a note.
          set_flattening_succeeded(false);
          AddFlatteningFailureReason(
              StrCat("A media query is too complex in ", url_for_humans()));
          break;
        }
      }
      stylesheet_.reset(stylesheet);
    }
  }
  return result;
}

bool CssHierarchy::ExpandChildren() {
  bool result = false;
  Css::Imports& imports = stylesheet_->mutable_imports();
  ResizeChildren(imports.size());
  for (int i = 0, n = imports.size(); i < n; ++i) {
    const Css::Import* import = imports[i];
    CssHierarchy* child = children_[i];
    GoogleString url(import->link().utf8_data(), import->link().utf8_length());
    const GoogleUrl import_url(css_resolution_base(), url);
    if (!import_url.IsWebValid()) {
      if (filter_ != NULL) {
        filter_->num_flatten_imports_invalid_url_->Add(1);
      }
      message_handler_->Message(kInfo, "Invalid import URL %s", url.c_str());
      child->set_flattening_succeeded(false);
      child->AddFlatteningFailureReason(StrCat("Invalid import URL ", url,
                                               " in ", url_for_humans()));
    } else {
      // We currently do not allow flattening of any @import statements with
      // complex CSS3-version media queries. Only plain media types (like
      // "screen", "print" and "all") are allowed.
      StringVector media_types;
      if (css_util::ConvertMediaQueriesToStringVector(import->media_queries(),
                                                      &media_types)) {
        if (child->DetermineImportMedia(media_, media_types)) {
          child->InitializeNested(*this, import_url);
          if (child->IsRecursive()) {
            if (filter_ != NULL) {
              filter_->num_flatten_imports_recursion_->Add(1);
            }
            child->set_flattening_succeeded(false);
            child->AddFlatteningFailureReason(
                StrCat("Recursive @import of ", child->url_for_humans()));
          } else {
            result = true;
          }
        }
      } else {
        // import->media_queries() contained complex media queries.
        if (filter_ != NULL) {
          filter_->num_flatten_imports_complex_queries_->Add(1);
        }
        child->set_flattening_succeeded(false);
        child->AddFlatteningFailureReason(
            StrCat("Complex media queries in the @import of ",
                   child->url_for_humans()));
      }
    }
  }
  return result;
}

void CssHierarchy::RollUpContents() {
  // If we have rolled up our contents already, we're done.
  if (!minified_contents_.empty()) {
    return;
  }

  // We need a stylesheet to do anything.
  if (stylesheet_.get() == NULL) {
    // If we don't have one we can try to create it from our contents.
    if (input_contents_.empty()) {
      // The CSS is empty with no contents - that's allowed.
      return;
    } else if (!Parse()) {
      // Even if we can't parse them, we have contents, albeit not minified.
      input_contents_.CopyToString(&minified_contents_);
      return;
    }
  }
  CHECK(stylesheet_.get() != NULL);

  const int n = children_.size();

  // Check if flattening has worked so far for us and all our children.
  for (int i = 0; i < n && flattening_succeeded_; ++i) {
    flattening_succeeded_ &= children_[i]->flattening_succeeded_;
    AddFlatteningFailureReason(children_[i]->flattening_failure_reason_);
    children_[i]->flattening_failure_reason_.clear();
  }

  // Check if any of our children have anything unparseable in them.
  for (int i = 0; i < n && !unparseable_detected_; ++i) {
    unparseable_detected_ = children_[i]->unparseable_detected_;
  }

  // If flattening has worked so far, check that we can get all children's
  // contents. If not, we treat it the same as flattening not succeeding.
  for (int i = 0; i < n && flattening_succeeded_; ++i) {
    // RollUpContents can change flattening_succeeded_ so check it again.
    children_[i]->RollUpContents();
    flattening_succeeded_ &= children_[i]->flattening_succeeded_;
    AddFlatteningFailureReason(children_[i]->flattening_failure_reason_);
    children_[i]->flattening_failure_reason_.clear();
  }

  if (!flattening_succeeded_) {
    // Flattening didn't succeed means we must return the minified version of
    // our stylesheet without any import flattening. children are irrelevant.
    STLDeleteElements(&children_);
    StringWriter writer(&minified_contents_);
    if (!CssMinify::Stylesheet(*stylesheet_.get(), &writer, message_handler_)) {
      // If we can't minify just use our contents, albeit not minified.
      input_contents_.CopyToString(&minified_contents_);
    }
  } else {
    // Flattening succeeded so concatenate our children's minified contents.
    for (int i = 0; i < n; ++i) {
      StrAppend(&minified_contents_, children_[i]->minified_contents());
    }

    // @charset and @import rules are discarded by flattening, but save them
    // until we know that the regeneration and limit check both went ok so we
    // restore the stylesheet back to its original state if not.
    Css::Charsets saved_charsets;
    Css::Imports saved_imports;
    stylesheet_->mutable_charsets().swap(saved_charsets);
    stylesheet_->mutable_imports().swap(saved_imports);

    // If we can't regenerate the stylesheet, or we have a result limit and the
    // flattened result is at or over that limit, flattening hasn't succeeded.
    StringWriter writer(&minified_contents_);
    bool minified_ok = CssMinify::Stylesheet(*stylesheet_.get(), &writer,
                                             message_handler_);
    if (!minified_ok) {
      if (filter_ != NULL) {
        filter_->num_flatten_imports_minify_failed_->Add(1);
      }
      flattening_succeeded_ = false;
      AddFlatteningFailureReason(StrCat("Minification failed for ",
                                        url_for_humans()));
    } else if (flattened_result_limit_ > 0) {
      int64 flattened_result_size = minified_contents_.size();
      if (flattened_result_size >= flattened_result_limit_) {
        if (filter_ != NULL) {
          filter_->num_flatten_imports_limit_exceeded_->Add(1);
        }
        flattening_succeeded_ = false;
        AddFlatteningFailureReason(
            StrCat("Flattening limit (",
                   IntegerToString(flattened_result_limit_),
                   ") exceeded (",
                   IntegerToString(flattened_result_size),
                   ")"));
      }
    }
    if (!flattening_succeeded_) {
      STLDeleteElements(&children_);   // our children are useless now
      // Revert the stylesheet back to how it was.
      stylesheet_->mutable_charsets().swap(saved_charsets);
      stylesheet_->mutable_imports().swap(saved_imports);
      // If minification succeeded but flattening failed, it can only be
      // because we exceeded the flattening limit, in which case we must fall
      // back to the minified form of the original unflattened stylesheet.
      minified_contents_.clear();
      if (!minified_ok || !CssMinify::Stylesheet(*stylesheet_.get(), &writer,
                                                 message_handler_)) {
        // If we can't minify just use our contents, albeit not minified.
        input_contents_.CopyToString(&minified_contents_);
      }
    }
    STLDeleteElements(&saved_imports);  // no-op if empty (was swapped back).
  }
}

bool CssHierarchy::RollUpStylesheets() {
  // We need a stylesheet to do anything.
  if (stylesheet_.get() == NULL) {
    // If we don't have one we can try to create it from our contents.
    if (input_contents_.empty()) {
      // The CSS is empty with no contents - that's allowed.
      return true;
    } else if (!Parse()) {
      return false;
    } else {
      // If the contents were loaded from cache it's possible for them to be
      // unable to be flattened. If we can parse them and they have @charset
      // or @import rules then they must have failed to flatten when they
      // were first cached because we expressly remove these below. The earlier
      // failure has already been added to the statistics so don't do so here,
      // nor do we note the reason in debug.
      if (!stylesheet_->charsets().empty() || !stylesheet_->imports().empty()) {
        flattening_succeeded_ = false;
      }
    }
  }
  CHECK(stylesheet_.get() != NULL);

  const int n = children_.size();

  // Check if flattening worked for us and all our children.
  for (int i = 0; i < n && flattening_succeeded_; ++i) {
    flattening_succeeded_ &= children_[i]->flattening_succeeded_;
    AddFlatteningFailureReason(children_[i]->flattening_failure_reason_);
    children_[i]->flattening_failure_reason_.clear();
  }

  // Check if any of our children have anything unparseable in them.
  for (int i = 0; i < n && !unparseable_detected_; ++i) {
    unparseable_detected_ = children_[i]->unparseable_detected_;
  }

  // If flattening succeeded, check that we can get all child stylesheets.
  // If not, we treat it the same as flattening not succeeding. Since this
  // method can change flattening_succeeded_ we have to check it again.
  for (int i = 0; i < n && flattening_succeeded_; ++i) {
    if (!children_[i]->RollUpStylesheets() ||
        !children_[i]->flattening_succeeded_) {
      flattening_succeeded_ = false;
    }
    AddFlatteningFailureReason(children_[i]->flattening_failure_reason_);
    children_[i]->flattening_failure_reason_.clear();
  }

  if (flattening_succeeded_) {
    // Flattening succeeded so delete our @charset and @import rules then
    // merge our children's rulesets and @font-faces (only) into ours.
    stylesheet_->mutable_charsets().clear();
    STLDeleteElements(&stylesheet_->mutable_imports());
    Css::Rulesets& target = stylesheet_->mutable_rulesets();
    Css::FontFaces& fonts_target = stylesheet_->mutable_font_faces();
    for (int i = n - 1; i >= 0; --i) {  // reverse order
      Css::Stylesheet* stylesheet = children_[i]->stylesheet_.get();
      if (stylesheet != NULL) {  // NULL if empty
        Css::Rulesets& source = stylesheet->mutable_rulesets();
        target.insert(target.begin(), source.begin(), source.end());
        source.clear();

        Css::FontFaces& fonts_source = stylesheet->mutable_font_faces();
        fonts_target.insert(fonts_target.begin(),
                            fonts_source.begin(), fonts_source.end());
        fonts_source.clear();
      }
    }
  }

  // If flattening failed we must return our stylesheet as-is and discard any
  // partially flattened children; if flattening succeeded we now hold all
  // the rulesets of the flattened hierarchy so we must discard all children
  // so we don't parse and merge then again. So in both cases ...
  STLDeleteElements(&children_);

  return true;
}

}  // namespace net_instaweb
