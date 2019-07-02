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


#include "net/instaweb/rewriter/public/scan_filter.h"

#include <memory>

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/csp.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

ScanFilter::ScanFilter(RewriteDriver* driver)
    : driver_(driver) {
}

ScanFilter::~ScanFilter() {
}

void ScanFilter::StartDocument() {
  // TODO(jmarantz): consider having rewrite_driver access the url in this
  // class, rather than poking it into rewrite_driver.
  seen_any_nodes_ = false;
  seen_refs_ = false;
  seen_base_ = false;
  seen_meta_tag_charset_ = false;

  // Set the driver's containing charset to whatever the headers set it to; if
  // they don't set it to anything, blank the driver's so we know it's not set.
  const ResponseHeaders* headers = driver_->response_headers();
  driver_->set_containing_charset(headers == NULL ? "" :
                                  headers->DetermineCharset());

  driver_->mutable_content_security_policy()->Clear();
  if (driver_->options()->honor_csp() && headers != nullptr) {
    ConstStringStarVector values;
    if (headers->Lookup(HttpAttributes::kContentSecurityPolicy, &values)) {
      for (const GoogleString* policy : values) {
        driver_->mutable_content_security_policy()->AddPolicy(
            CspPolicy::Parse(*policy));
      }
    }
  }
}

void ScanFilter::Cdata(HtmlCdataNode* cdata) {
  seen_any_nodes_ = true;
}

void ScanFilter::Comment(HtmlCommentNode* comment) {
  seen_any_nodes_ = true;
}

void ScanFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  seen_any_nodes_ = true;
}

void ScanFilter::Directive(HtmlDirectiveNode* directive) {
  seen_any_nodes_ = true;
}

void ScanFilter::Characters(HtmlCharactersNode* characters) {
  // Check for a BOM at the start of the document. All other event handlers
  // set the flag to false without using it, so if it's true on entry then
  // this must be the first event.
  if (!seen_any_nodes_ && driver_->containing_charset().empty()) {
    StringPiece charset = GetCharsetForBom(characters->contents());
    if (!charset.empty()) {
      driver_->set_containing_charset(charset);
    }
  }
  seen_any_nodes_ = true;  // ignore any subsequent BOMs.
}

void ScanFilter::StartElement(HtmlElement* element) {
  seen_any_nodes_ = true;
  // <base>
  if (element->keyword() == HtmlName::kBase) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage
    // /semantics.html#the-base-element
    //
    if (href != nullptr) {
      if (href->DecodedValueOrNull() == nullptr) {
        // Can't decode base well, so give up on using.
        driver_->set_other_base_problem();
        return;
      }

      // It would be much better if we were to use IsBasePermitted here, but
      // we may not be able to set previous_origin accurately. So instead,
      // we act overly conservatively and handle
      if (driver_->content_security_policy().HasDirective(
              CspDirective::kBaseUri)) {
        driver_->InsertDebugComment(
            "Unable to check safety of a base with CSP base-uri, "
            "proceeding conservatively.",
            element);
        driver_->set_other_base_problem();
        return;
      }


      // TODO(jmarantz): consider having rewrite_driver access the url in this
      // class, rather than poking it into rewrite_driver.
      GoogleString new_base = href->DecodedValueOrNull();
      driver_->options()->domain_lawyer()->AddProxySuffix(driver_->google_url(),
                                                          &new_base);
      driver_->SetBaseUrlIfUnset(new_base);
      seen_base_ = true;
      if (seen_refs_) {
        driver_->set_refs_before_base();
      }
    }
    // TODO(jmarantz): handle base targets in addition to hrefs.
  } else {
    resource_tag_scanner::UrlCategoryVector attributes;
    resource_tag_scanner::ScanElement(element, driver_->options(), &attributes);
    for (int i = 0, n = attributes.size(); i < n; ++i) {
      // Don't count <html manifest=...> as a ref for the purpose of determining
      // if there are refs before base.  It's also important not to count <head
      // profile=...> but ScanElement skips that.
      if (!seen_refs_ && !seen_base_ &&
          !(element->keyword() == HtmlName::kHtml &&
            attributes[i].url->keyword() == HtmlName::kManifest)) {
        seen_refs_ = true;
      }
    }
  }

  if (driver_->options()->honor_csp() &&
      element->keyword() == HtmlName::kMeta) {
    // Note: https://html.spec.whatwg.org/multipage/semantics.html#attr-meta-http-equiv-content-security-policy
    // requires us to check whether the meta element is a child of a <head>.
    // We cannot do it reliably since we don't do full HTML5 parsing (complete
    // with inventing missing nodes), so we conservatively assume that the
    // policy applies.
    const char* equiv = element->AttributeValue(HtmlName::kHttpEquiv);
    const char* content = element->AttributeValue(HtmlName::kContent);
    if (equiv && content &&
        StringCaseEqual(equiv, HttpAttributes::kContentSecurityPolicy) &&
        !StringPiece(content).empty()) {
      driver_->mutable_content_security_policy()->AddPolicy(
          CspPolicy::Parse(content));
    }
  }

  // Get/set the charset of the containing HTML page.
  // HTTP1.1 says the default charset is ISO-8859-1 but as the W3C says (in
  // http://www.w3.org/International/O-HTTP-charset.en.php) not many browsers
  // actually do this so we default to "" instead so that we can tell if it
  // has been set. The following logic is taken from
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#
  // determining-the-character-encoding:
  // 1. If the UA specifies an encoding, use that (not relevant to us).
  // 2. If the transport layer specifies an encoding, use that.
  //    Implemented by using the charset from any Content-Type header.
  // 3. If there is a BOM at the start of the file, use the relevant encoding.
  // 4. If there is a meta tag in the HTML, use the encoding specified if any.
  // 5. There are various other heuristics listed which are not implemented.
  // 6. Otherwise, use no charset or default to something "sensible".
  if (!seen_meta_tag_charset_ &&
      driver_->containing_charset().empty() &&
      element->keyword() == HtmlName::kMeta) {
    GoogleString content, mime_type, charset;
    if (CommonFilter::ExtractMetaTagDetails(*element, NULL,
                                            &content, &mime_type, &charset)) {
      if (!charset.empty()) {
        driver_->set_containing_charset(charset);
        seen_meta_tag_charset_ = true;
      }
    }
  }
}

void ScanFilter::EndElement(HtmlElement* element) {
  if (element->keyword() == HtmlName::kBase &&
      !driver_->options()->domain_lawyer()->proxy_suffix().empty()) {
    HtmlElement::Attribute* href = element->FindAttribute(HtmlName::kHref);
    if (href != NULL) {
      href->SetValue(driver_->base_url().AllExceptQuery());
    }
  }
}

void ScanFilter::Flush() {
  driver_->server_context()->rewrite_stats()->num_flushes()->Add(1);
}

}  // namespace net_instaweb
