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


#include "net/instaweb/rewriter/public/css_outline_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

class MessageHandler;

const char kStylesheet[] = "stylesheet";

const char CssOutlineFilter::kFilterId[] = "co";

CssOutlineFilter::CssOutlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      inline_element_(NULL),
      inline_chars_(NULL),
      size_threshold_bytes_(driver->options()->css_outline_min_bytes()) {
}

CssOutlineFilter::~CssOutlineFilter() {}

void CssOutlineFilter::StartDocumentImpl() {
  inline_element_ = NULL;
  inline_chars_ = NULL;
}

void CssOutlineFilter::StartElementImpl(HtmlElement* element) {
  // No tags allowed inside style element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    driver()->ErrorHere("Tag '%s' found inside style.",
                        CEscape(element->name_str()).c_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    inline_chars_ = NULL;
  }
  if (element->keyword() == HtmlName::kStyle &&
      element->FindAttribute(HtmlName::kScoped) == NULL) {
    // <style scoped> can't be directly converted to a <link>. We could
    // theoretically convert it to a <style scoped>@import ... </style>, but
    // given the feature has very little browser support, it's probably not
    // worth the effort, so we just leave it alone.
    // All other <style> blocks are candidates for conversion.
    inline_element_ = element;
    inline_chars_ = NULL;
  }
}

void CssOutlineFilter::EndElementImpl(HtmlElement* element) {
  if (inline_element_ != NULL) {
    CHECK(element == inline_element_);
    if (inline_chars_ != NULL &&
        inline_chars_->contents().size() >= size_threshold_bytes_) {
      OutlineStyle(inline_element_, inline_chars_->contents());
    }
    inline_element_ = NULL;
    inline_chars_ = NULL;
  }
}

void CssOutlineFilter::Flush() {
  // If we were flushed in a style element, we cannot outline it.
  inline_element_ = NULL;
  inline_chars_ = NULL;
}

void CssOutlineFilter::Characters(HtmlCharactersNode* characters) {
  if (inline_element_ != NULL) {
    CHECK(inline_chars_ == NULL) << "Multiple character blocks in style.";
    inline_chars_ = characters;
  }
}

// Try to write content and possibly header to resource.
bool CssOutlineFilter::WriteResource(const StringPiece& content,
                                     OutputResource* resource,
                                     MessageHandler* handler) {
  // We don't provide charset here since in general we can just inherit
  // from the page.
  // TODO(morlovich) check for proper behavior in case of embedded BOM.
  // TODO(matterbury) but AFAICT you cannot have a BOM in a <style> tag.
  return driver()->Write(
      ResourceVector(), content, &kContentTypeCss, StringPiece(), resource);
}

// Create file with style content and remove that element from DOM.
void CssOutlineFilter::OutlineStyle(HtmlElement* style_element,
                                    const GoogleString& content_str) {
  StringPiece content(content_str);
  if (driver()->IsRewritable(style_element)) {
    // Create style file from content.
    const char* type = style_element->AttributeValue(HtmlName::kType);
    // We only deal with CSS styles.  If no type specified, CSS is assumed.
    // See http://www.w3.org/TR/html5/semantics.html#the-style-element
    if (type == NULL || strcmp(type, kContentTypeCss.mime_type()) == 0) {
      MessageHandler* handler = driver()->message_handler();
      // Create outline resource at the document location,
      // not base URL location.
      GoogleString failure_reason;
      OutputResourcePtr output_resource(
          driver()->CreateOutputResourceWithUnmappedUrl(
              driver()->google_url(), kFilterId, "_", kOutlinedResource,
              &failure_reason));

      if (output_resource.get() == NULL) {
        driver()->InsertDebugComment(failure_reason, style_element);
      } else {
        // Rewrite URLs in content.
        GoogleString transformed_content;
        StringWriter writer(&transformed_content);
        bool content_valid = true;
        switch (driver()->ResolveCssUrls(base_url(),
                                         output_resource->resolved_base(),
                                         content,
                                         &writer, handler)) {
          case RewriteDriver::kNoResolutionNeeded:
            break;
          case RewriteDriver::kWriteFailed:
            content_valid = false;
            break;
          case RewriteDriver::kSuccess:
            content = transformed_content;
            break;
        }
        if (content_valid &&
            WriteResource(content, output_resource.get(), handler)) {
          HtmlElement* link_element = driver()->NewElement(
              style_element->parent(), HtmlName::kLink);
          driver()->AddAttribute(link_element, HtmlName::kRel, kStylesheet);
          driver()->AddAttribute(
              link_element, HtmlName::kHref, output_resource->url());
          // Add all style attributes to link.
          const HtmlElement::AttributeList& attrs = style_element->attributes();
          for (HtmlElement::AttributeConstIterator i(attrs.begin());
               i != attrs.end(); ++i) {
            const HtmlElement::Attribute& attr = *i;
            link_element->AddAttribute(attr);
          }
          // Add link to DOM.
          driver()->InsertNodeAfterNode(style_element, link_element);
          // Remove style element from DOM.
          if (!driver()->DeleteNode(style_element)) {
            driver()->FatalErrorHere("Failed to delete inline style element");
          }
        }
      }
    } else {
      driver()->InsertDebugComment(StrCat(
          "Cannot outline stylesheet with non-CSS type=", type), style_element);
      GoogleString element_string = style_element->ToString();
      driver()->InfoHere("Cannot outline non-css stylesheet %s",
                         element_string.c_str());
    }
  }
}

}  // namespace net_instaweb
