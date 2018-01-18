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


#include "net/instaweb/rewriter/public/css_image_rewriter.h"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_hierarchy.h"
#include "net/instaweb/rewriter/public/css_resource_slot.h"
#include "net/instaweb/rewriter/public/image_combine_filter.h"
#include "net/instaweb/rewriter/public/image_rewrite_filter.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"  // for StrCat
#include "pagespeed/kernel/http/google_url.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/parser.h"
#include "webutil/css/property.h"
#include "webutil/css/value.h"


namespace net_instaweb {

namespace {

GoogleString CannotImportMessage(StringPiece action, StringPiece url,
                                 bool is_authorized) {
  return StrCat("Cannot ", action, " ", url,
                (is_authorized
                 ? " for an unknown reason"
                 : " as it is on an unauthorized domain"));
}

}  // namespace

CssImageRewriter::CssImageRewriter(CssFilter::Context* root_context,
                                   CssFilter* filter,
                                   CacheExtender* cache_extender,
                                   ImageRewriteFilter* image_rewriter,
                                   ImageCombineFilter* image_combiner)
    : filter_(filter),
      root_context_(root_context),
      // For now we use the same options as for rewriting and cache-extending
      // images found in HTML.
      cache_extender_(cache_extender),
      image_combiner_(image_combiner),
      image_rewriter_(image_rewriter) {
  // TODO(morlovich): Unlike the original CssImageRewriter, this uses the same
  // statistics as underlying filters like CacheExtender. Should it get separate
  // stats instead? sligocki thinks it's useful to know how many images were
  // optimized from CSS files, but people probably also want to know how many
  // total images were cache-extended.
}

CssImageRewriter::~CssImageRewriter() {}

bool CssImageRewriter::RewritesEnabled(
    int64 image_inline_max_bytes) const {
  const RewriteOptions* options = driver()->options();
  return (image_inline_max_bytes > 0 ||
          options->ImageOptimizationEnabled() ||
          options->Enabled(RewriteOptions::kLeftTrimUrls) ||
          options->Enabled(RewriteOptions::kExtendCacheImages) ||
          options->Enabled(RewriteOptions::kSpriteImages));
}

bool CssImageRewriter::RewriteImport(RewriteContext* parent,
                                     CssHierarchy* hierarchy,
                                     bool* is_authorized) {
  GoogleUrl import_url(hierarchy->url());
  ResourcePtr resource = driver()->CreateInputResource(
      import_url, RewriteDriver::InputRole::kStyle, is_authorized);
  if (resource.get() == NULL) {
    return false;
  }

  parent->AddNestedContext(
      filter_->MakeNestedFlatteningContextInNewSlot(
          resource, driver()->UrlLine(), root_context_, parent, hierarchy));
  return true;
}

bool CssImageRewriter::RewriteImage(int64 image_inline_max_bytes,
                                    const GoogleUrl& trim_url,
                                    const GoogleUrl& original_url,
                                    RewriteContext* parent,
                                    Css::Values* values,
                                    size_t value_index,
                                    bool* is_authorized) {
  const RewriteOptions* options = driver()->options();
  ResourcePtr resource = driver()->CreateInputResource(
      original_url, RewriteDriver::InputRole::kImg, is_authorized);
  if (resource.get() == NULL) {
    return false;
  }

  CssResourceSlotPtr slot(
      root_context_->slot_factory()->GetSlot(resource, trim_url, options,
                                             values, value_index));
  if (options->image_preserve_urls()) {
    slot->set_preserve_urls(true);
  }

  RewriteSlot(ResourceSlotPtr(slot), image_inline_max_bytes, parent);
  return true;
}

void CssImageRewriter::RewriteSlot(const ResourceSlotPtr& slot,
                                   int64 image_inline_max_bytes,
                                   RewriteContext* parent) {
  const RewriteOptions* options = driver()->options();
  if (options->ImageOptimizationEnabled() || image_inline_max_bytes > 0) {
    // Do not rewrite external resource if preserve_urls is enabled unless
    // we allow preemptive rewriting.
    if (!slot->preserve_urls() ||
        options->in_place_preemptive_rewrite_css_images()) {
      parent->AddNestedContext(
          image_rewriter_->MakeNestedRewriteContextForCss(
              image_inline_max_bytes, parent, slot));
    }
  }

  if (driver()->MayCacheExtendImages()) {
    parent->AddNestedContext(
        cache_extender_->MakeNestedContext(parent, slot));
  }

  // TODO(sligocki): DomainRewriter or is this done automatically?
}

void CssImageRewriter::InheritChildImageInfo(RewriteContext* context) {
  typedef ImageRewriteFilter::AssociatedImageInfoMap AssociatedImageInfoMap;
  if (!context->Driver()->options()->Enabled(
      RewriteOptions::kExperimentCollectMobImageInfo)) {
    return;
  }

  if (context->num_outputs() != 1) {
    LOG(DFATAL) << "InheritChildImageInfo on context with wrong # of outputs:"
                << context->num_outputs();
    return;
  }

  AssociatedImageInfoMap child_image_info;
  for (int i = 0; i < context->num_nested(); ++i) {
    RewriteContext* nested_context = context->nested(i);
    for (int j = 0; j < nested_context->num_output_partitions(); ++j) {
      const CachedResult* child_result = nested_context->output_partition(j);

      // Image info may be directly included inside the CachedResult, if
      // child_result came from the image filter.
      AssociatedImageInfo from_image_filter;
      if (ImageRewriteFilter::ExtractAssociatedImageInfo(child_result,
                                                         nested_context,
                                                         &from_image_filter)) {
        child_image_info[from_image_filter.url()] = from_image_filter;
      }

      // Info on multiple images may be stored as AssociatedImageInfo by CSS
      // rewriting or flattening.
      for (int k = 0; k < child_result->associated_image_info_size(); ++k) {
        const AssociatedImageInfo& image_info =
            child_result->associated_image_info(k);
        child_image_info[image_info.url()] = image_info;
      }
    }
  }

  for (AssociatedImageInfoMap::iterator i = child_image_info.begin(),
                                        e = child_image_info.end();
       i != e; ++i) {
    *context->mutable_output_partition(0)->add_associated_image_info() =
        i->second;
  }
}

bool CssImageRewriter::RewriteCss(int64 image_inline_max_bytes,
                                  RewriteContext* parent,
                                  CssHierarchy* hierarchy,
                                  MessageHandler* handler) {
  const RewriteOptions* options = driver()->options();
  bool spriting_ok = options->Enabled(RewriteOptions::kSpriteImages);

  if (!driver()->FlattenCssImportsEnabled()) {
    // If flattening is disabled completely, mark this hierarchy as having
    // failed flattening, so that later RollUps do the right thing (nothing).
    // This is not something we need to log in the statistics or in debug.
    hierarchy->set_flattening_succeeded(false);
  } else if (hierarchy->flattening_succeeded()) {
    // Flattening of this hierarchy might have already failed because of a
    // problem detected with the containing charset or media, in particular
    // see CssFilter::Start(Inline|Attribute|External)Rewrite.
    if (hierarchy->ExpandChildren()) {
      for (int i = 0, n = hierarchy->children().size(); i < n; ++i) {
        CssHierarchy* child = hierarchy->children()[i];
        if (child->NeedsRewriting()) {
          bool is_authorized;
          if (!RewriteImport(parent, child, &is_authorized)) {
            hierarchy->set_flattening_succeeded(false);
            hierarchy->AddFlatteningFailureReason(
                CannotImportMessage("import", child->url_for_humans(),
                                    is_authorized));
          }
        }
      }
    }
  }

  // TODO(jkarlin): We need a separate flag for CssImagePreserveURLs in case the
  // user is willing to change image URLs in CSS but not in HTML.
  bool is_enabled = RewritesEnabled(image_inline_max_bytes);

  if (is_enabled) {
    if (spriting_ok) {
      image_combiner_->Reset(parent, hierarchy->css_base_url(),
                             hierarchy->input_contents());
    }
    Css::Rulesets& rulesets =
        hierarchy->mutable_stylesheet()->mutable_rulesets();
    for (Css::Rulesets::iterator ruleset_iter = rulesets.begin();
         ruleset_iter != rulesets.end(); ++ruleset_iter) {
      Css::Ruleset* ruleset = *ruleset_iter;
      if (ruleset->type() != Css::Ruleset::RULESET) {
        continue;
      }
      Css::Declarations& decls = ruleset->mutable_declarations();
      bool background_position_found = false;
      bool background_image_found = false;
      for (Css::Declarations::iterator decl_iter = decls.begin();
           decl_iter != decls.end(); ++decl_iter) {
        Css::Declaration* decl = *decl_iter;
        // Only edit image declarations.
        switch (decl->prop()) {
          case Css::Property::BACKGROUND_POSITION:
          case Css::Property::BACKGROUND_POSITION_X:
          case Css::Property::BACKGROUND_POSITION_Y:
            background_position_found = true;
            break;
          case Css::Property::BACKGROUND:
          case Css::Property::BACKGROUND_IMAGE:
          case Css::Property::CONTENT:  // In CSS2 but not CSS2.1
          case Css::Property::CURSOR:
          case Css::Property::LIST_STYLE:
          case Css::Property::LIST_STYLE_IMAGE: {
            // Rewrite all URLs. Technically, background-image should only
            // have a single value which is a URL, but background could have
            // more values.
            Css::Values* values = decl->mutable_values();
            for (size_t value_index = 0; value_index < values->size();
                 value_index++) {
              Css::Value* value = values->at(value_index);
              if (value->GetLexicalUnitType() == Css::Value::URI) {
                background_image_found = true;
                GoogleString rel_url =
                    UnicodeTextToUTF8(value->GetStringValue());
                // TODO(abliss): only do this resolution once.
                const GoogleUrl original_url(hierarchy->css_resolution_base(),
                                             rel_url);
                if (!original_url.IsWebValid()) {
                  continue;
                }
                if (!options->IsAllowed(original_url.Spec())) {
                  continue;
                }
                bool is_authorized;
                if (spriting_ok) {
                  // TODO(sligocki): Pass in the correct base URL here.
                  // Specifically, the final base URL of the CSS that will
                  // be used to trim the final URLs.
                  // hierarchy->css_base_url(), hierarchy->css_trim_url(),
                  // or hierarchy->css_resolution_base()?
                  // Note that currently preserving URLs doesn't work for
                  // image combining filter, so we need to fix that before
                  // testing which URL is correct.
                  if (!image_combiner_->AddCssBackgroundContext(
                          original_url, hierarchy->css_trim_url(),
                          values, value_index, root_context_, &decls,
                          &is_authorized, handler)) {
                    // This doesn't fail flattening but we want to log it.
                    hierarchy->AddFlatteningFailureReason(
                        CannotImportMessage("rewrite", original_url.Spec(),
                                            is_authorized));
                  }
                }
                if (!RewriteImage(image_inline_max_bytes,
                                  hierarchy->css_trim_url(), original_url,
                                  parent, values, value_index,
                                  &is_authorized)) {
                  // This doesn't fail flattening but we want to log it.
                  hierarchy->AddFlatteningFailureReason(
                      CannotImportMessage("rewrite", original_url.Spec(),
                                          is_authorized));
                }
              }
            }
            break;
          }
          default:
            break;
        }
      }
      // All the declarations in this ruleset have been parsed.
      if (spriting_ok && background_position_found && !background_image_found) {
        // A ruleset that contains a background-position but no background image
        // is a signal that we should not be spriting.
        handler->Message(kInfo,
                         "Lone background-position found: Cannot sprite.");
        spriting_ok = false;
      }
    }

    image_combiner_->RegisterOrReleaseContext();
  } else {
    handler->Message(kInfo, "Image rewriting and cache extension not enabled, "
                     "so not rewriting images in CSS in %s",
                     hierarchy->css_base_url().spec_c_str());
  }

  return is_enabled;
}

}  // namespace net_instaweb
