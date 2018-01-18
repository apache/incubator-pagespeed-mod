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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_

#include <cstddef>

#include "net/instaweb/rewriter/public/css_filter.h"
#include "net/instaweb/rewriter/public/css_hierarchy.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/http/google_url.h"

namespace Css {

class Values;

}  // namespace Css

namespace net_instaweb {

class CacheExtender;
class ImageCombineFilter;
class ImageRewriteFilter;
class MessageHandler;
class Statistics;

class CssImageRewriter {
 public:
  CssImageRewriter(CssFilter::Context* root_context,
                   CssFilter* filter,
                   CacheExtender* cache_extender,
                   ImageRewriteFilter* image_rewriter,
                   ImageCombineFilter* image_combiner);
  ~CssImageRewriter();

  static void InitStats(Statistics* statistics);

  // Attempts to rewrite the given CSS, starting nested rewrites for each
  // import and image to be rewritten. If successful, it mutates the CSS
  // to point to new images and flattens all @imports (if enabled).
  // Returns true if rewriting is enabled.
  bool RewriteCss(int64 image_inline_max_bytes,
                  RewriteContext* parent,
                  CssHierarchy* hierarchy,
                  MessageHandler* handler);

  // Is @import flattening enabled?
  bool FlatteningEnabled() const;

  // Are any rewrites enabled?
  bool RewritesEnabled(int64 image_inline_max_bytes) const;

  // Rewrite an image already loaded into a slot. Used by RewriteImage and
  // AssociationTransformer to rewrite images in either case.
  void RewriteSlot(const ResourceSlotPtr& slot,
                   int64 image_inline_max_bytes,
                   RewriteContext* parent);

  // Propagates image information in child rewrites of context into it.
  // Expected to be called from context->Harvest().
  static void InheritChildImageInfo(RewriteContext* context);

 private:
  RewriteDriver* driver() const {
    return filter_->driver();
  }
  bool RewriteImport(RewriteContext* parent, CssHierarchy* hierarchy,
                     bool* is_authorized);
  bool RewriteImage(int64 image_inline_max_bytes, const GoogleUrl& trim_url,
                    const GoogleUrl& original_url, RewriteContext* parent,
                    Css::Values* values, size_t value_index,
                    bool* is_authorized);

  // Needed for import flattening.
  CssFilter* filter_;

  // Top level context for rewriting root CSS file itself.
  CssFilter::Context* root_context_;

  // Pointers to other HTML filters used to rewrite images.
  // TODO(sligocki): morlovich suggests separating this out as some
  // centralized API call like rewrite_driver_->RewriteImage().
  CacheExtender* cache_extender_;
  ImageCombineFilter* image_combiner_;
  ImageRewriteFilter* image_rewriter_;

  DISALLOW_COPY_AND_ASSIGN(CssImageRewriter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_IMAGE_REWRITER_H_
