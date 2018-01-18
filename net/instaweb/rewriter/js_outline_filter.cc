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


#include "net/instaweb/rewriter/public/js_outline_filter.h"

#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

class MessageHandler;

const char JsOutlineFilter::kFilterId[] = "jo";

JsOutlineFilter::JsOutlineFilter(RewriteDriver* driver)
    : CommonFilter(driver),
      inline_element_(NULL),
      inline_chars_(NULL),
      server_context_(driver->server_context()),
      size_threshold_bytes_(driver->options()->js_outline_min_bytes()),
      script_tag_scanner_(driver) { }

JsOutlineFilter::~JsOutlineFilter() {}

void JsOutlineFilter::StartDocumentImpl() {
  inline_element_ = NULL;
  inline_chars_ = NULL;
}

void JsOutlineFilter::StartElementImpl(HtmlElement* element) {
  // No tags allowed inside script element.
  if (inline_element_ != NULL) {
    // TODO(sligocki): Add negative unit tests to hit these errors.
    driver()->ErrorHere("Tag '%s' found inside script.",
                        CEscape(element->name_str()).c_str());
    inline_element_ = NULL;  // Don't outline what we don't understand.
    inline_chars_ = NULL;
  }

  HtmlElement::Attribute* src;
  // We only deal with Javascript
  if (script_tag_scanner_.ParseScriptElement(element, &src) ==
      ScriptTagScanner::kJavaScript) {
    inline_element_ = element;
    inline_chars_ = NULL;
    // script elements which already have a src should not be outlined.
    if (src != NULL) {
      inline_element_ = NULL;
    }
  }
}

void JsOutlineFilter::EndElementImpl(HtmlElement* element) {
  if (inline_element_ != NULL) {
    if (element != inline_element_) {
      // No other tags allowed inside script element.
      driver()->ErrorHere("Tag '%s' found inside script.",
                          CEscape(element->name_str()).c_str());
    } else if (inline_chars_ != NULL &&
               inline_chars_->contents().size() >= size_threshold_bytes_) {
      OutlineScript(inline_element_, inline_chars_->contents());
    }
    inline_element_ = NULL;
    inline_chars_ = NULL;
  }
}

void JsOutlineFilter::Flush() {
  // If we were flushed in a script element, we cannot outline it.
  inline_element_ = NULL;
  inline_chars_ = NULL;
}

void JsOutlineFilter::Characters(HtmlCharactersNode* characters) {
  if (inline_element_ != NULL) {
    inline_chars_ = characters;
  }
}

// Try to write content and possibly header to resource.
bool JsOutlineFilter::WriteResource(const GoogleString& content,
                                    OutputResource* resource,
                                    MessageHandler* handler) {
  // We don't provide charset here since in generally we can just inherit
  // from the page.
  // TODO(morlovich) check for proper behavior in case of embedded BOM.
  return driver()->Write(
      ResourceVector(), content, &kContentTypeJavascript, StringPiece(),
      resource);
}

// Create file with script content and remove that element from DOM.
void JsOutlineFilter::OutlineScript(HtmlElement* inline_element,
                                    const GoogleString& content) {
  if (driver()->IsRewritable(inline_element)) {
    // Create script file from content.
    MessageHandler* handler = driver()->message_handler();
    // Create outline resource at the document location, not base URL location
    GoogleString failure_reason;
    OutputResourcePtr resource(
        driver()->CreateOutputResourceWithUnmappedUrl(
            driver()->google_url(), kFilterId, "_", kOutlinedResource,
            &failure_reason));
    if (resource.get() == NULL) {
      driver()->InsertDebugComment(failure_reason, inline_element);
    } else if (WriteResource(content, resource.get(), handler)) {
      HtmlElement* outline_element = driver()->CloneElement(inline_element);
      driver()->AddAttribute(outline_element, HtmlName::kSrc,
                             resource->url());
      // Add <script src=...> element to DOM.
      driver()->InsertNodeBeforeNode(inline_element, outline_element);
      // Remove original script element from DOM.
      if (!driver()->DeleteNode(inline_element)) {
        driver()->FatalErrorHere("Failed to delete inline script element");
      }
    } else {
      driver()->InsertDebugComment("Failed to write outlined script resource.",
                                   inline_element);
      driver()->ErrorHere("Failed to write outlined script resource.");
    }
  }
}

}  // namespace net_instaweb
