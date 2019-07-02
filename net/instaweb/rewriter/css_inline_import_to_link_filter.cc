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


#include "net/instaweb/rewriter/public/css_inline_import_to_link_filter.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "net/instaweb/rewriter/public/css_util.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "util/utf8/public/unicodetext.h"
#include "webutil/css/media.h"
#include "webutil/css/parser.h"

namespace net_instaweb {

namespace {

// names for Statistics variables.
const char kCssImportsToLinks[] = "css_imports_to_links";

}  // namespace

CssInlineImportToLinkFilter::CssInlineImportToLinkFilter(RewriteDriver* driver,
                                                         Statistics* statistics)
    : driver_(driver),
      counter_(statistics->GetVariable(kCssImportsToLinks)) {
  ResetState();
}

CssInlineImportToLinkFilter::~CssInlineImportToLinkFilter() {}

void CssInlineImportToLinkFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kCssImportsToLinks);
}

void CssInlineImportToLinkFilter::StartDocument() {
  ResetState();
}

void CssInlineImportToLinkFilter::EndDocument() {
  ResetState();
}

void CssInlineImportToLinkFilter::StartElement(HtmlElement* element) {
  DCHECK(style_element_ == NULL);  // HTML Parser guarantees this.
  if (style_element_ == NULL && element->keyword() == HtmlName::kStyle) {
    // The contents are ok to rewrite iff its type is text/css or it has none.
    // See http://www.w3.org/TR/html5/semantics.html#the-style-element
    const char* type = element->AttributeValue(HtmlName::kType);
    if (type == NULL || strcmp(type, kContentTypeCss.mime_type()) == 0) {
      style_element_ = element;
      style_characters_ = NULL;
    }
  }
}

void CssInlineImportToLinkFilter::EndElement(HtmlElement* element) {
  if (style_element_ == element) {
    InlineImportToLinkStyle();
    ResetState();
  }
}

void CssInlineImportToLinkFilter::Characters(HtmlCharactersNode* characters) {
  if (style_element_ != NULL) {
    DCHECK(style_characters_ == NULL);  // HTML Parser guarantees this.
    style_characters_ = characters;
  }
}

void CssInlineImportToLinkFilter::Flush() {
  // If we were flushed in a style element, we cannot rewrite it.
  if (style_element_ != NULL) {
    ResetState();
  }
}

void CssInlineImportToLinkFilter::ResetState() {
  style_element_ = NULL;
  style_characters_ = NULL;
}

namespace {

// Extract the given style's media attribute, if any. Fail if can't decode it.
bool ExtractMediaFromStyle(const HtmlElement* style_element,
                           GoogleString* media_attribute) {
  const HtmlElement::Attribute* styles_media =
      style_element->FindAttribute(HtmlName::kMedia);
  if (styles_media!= NULL) {
    const char* decoded_value = styles_media->DecodedValueOrNull();
    if (decoded_value == NULL) {
      return false;
    } else {
      media_attribute->assign(decoded_value);
    }
  }
  return true;
}

// Determine if the import has a single simple media that matches the style's.
bool MediaMatch(const GoogleString& media_attribute,
                const Css::Import* import) {
  bool result = false;
  if (media_attribute.empty()) {
    // The style doesn't have a media attribute to match against.
  } else if (import->media_queries().size() != 1) {
    // The import doesn't have a single media.
  } else if (css_util::IsComplexMediaQuery(*import->media_queries()[0])) {
    // The import doesn't have a simple media.
  } else {
    // TODO(jmarantz): this code would feel a bit better if
    // attribute-decoding supported UTF8.
    const StringPiece import_media(
        import->media_queries()[0]->media_type().utf8_data(),
        import->media_queries()[0]->media_type().utf8_length());
    result = media_attribute == import_media;
  }
  return result;
}

// Check if the given import can be converted to a link elements.
// media_attribute is the original style's media attribute; link_media is set
// to the import's media iff it has one and the style doesn't; style_media is
// used to store the vectorized version of media_attribute and is lazily
// initialized by this function when it is first required;
// style_media_is_determined is the flag that records that style_media is set.
bool CheckConversionOfImportToLink(const Css::Import* import,
                                   const GoogleString& media_attribute,
                                   GoogleString* link_media,
                                   bool* style_media_is_determined,
                                   StringVector* style_media) {
  if (import->link().utf8_length() == 0) {
    // Empty URLs are problematic so we give up if we hit any.
    return false;
  } else if (import->media_queries().empty()) {
    // No media queries is easy - just copy any media into the link.
  } else if (MediaMatch(media_attribute, import)) {
    // A 'simple' media query that matches the style's is also good.
  } else {
    // If the style has media then the @import may specify no media or the
    // same media; if the style has no media use the @import's, if any.
    StringVector import_media;
    if (css_util::ConvertMediaQueriesToStringVector(
            import->media_queries(), &import_media)) {
      if (!media_attribute.empty()) {
        if (!*style_media_is_determined) {
          css_util::VectorizeMediaAttribute(media_attribute, style_media);
          std::sort(style_media->begin(), style_media->end());
          *style_media_is_determined = true;
        }
        // VectorizeMediaAttribute returns an empty vector if any medium
        // is "all", so be careful to do the same to import_media.
        css_util::ClearVectorIfContainsMediaAll(&import_media);
        std::sort(import_media.begin(), import_media.end());
        // We have sorted both the vectors because the order of media is not
        // significant as they're additive: screen,print == print,screen.
        return (*style_media == import_media);
      } else {
        // Note the import's media to copy it to the corresponding link.
        *link_media = css_util::StringifyMediaVector(import_media);
      }
    } else {
      // If we can't parse the media query then it's too complex for us.
      return false;
    }
  }
  return true;
}

}  // namespace

// Pull out each @import from a <style> element into <link> elements.
void CssInlineImportToLinkFilter::InlineImportToLinkStyle() {
  // Conditions for rewriting @imports from within a style element:
  // * The element isn't empty.
  // * The element is rewritable.
  // * It doesn't already have an href or rel attribute, since we add these.
  // * It doesn't have a scoped attribute, since scoped styles can't be
  //   done with a <link>
  // * It begins with one or more valid @import statement.
  // * Each @import actually imports something (the url isn't empty).
  // * Each @import's media, if any, are the same as style's, if any.
  if (style_characters_ != NULL &&
      driver_->IsRewritable(style_element_) &&
      style_element_->FindAttribute(HtmlName::kHref) == NULL &&
      style_element_->FindAttribute(HtmlName::kRel) == NULL &&
      style_element_->FindAttribute(HtmlName::kScoped) == NULL) {
    // Parse imports until we hit the end of them; if there's anything else
    // in the CSS we leave that in the inline style.
    Css::Parser parser(style_characters_->contents());
    Css::Imports imports;
    Css::Import* import;
    StringVector media;

    // Extract the style's media attribute, if any. Fail if we can't decode it.
    GoogleString media_attribute;
    bool ok = ExtractMediaFromStyle(style_element_, &media_attribute);

    // The style's media converted to a vector of media types. This is parsed
    // and set on first use but it's actually a loop invariant that could be
    // set before the loop, but we don't in case we never end up needing it.
    StringVector style_media;
    bool style_media_is_determined = false;

    // Check each import in turn, failing if any of them have a problem.
    while (ok && (import = parser.ParseNextImport()) != NULL) {
      imports.push_back(import);
      // Default the media for the link to the style's media attribute;
      // CheckConversion... overrides that if the @import has its own media.
      media.push_back(media_attribute);
      ok = CheckConversionOfImportToLink(import, media_attribute, &media.back(),
                                         &style_media_is_determined,
                                         &style_media);
    }

    if (ok && (imports.size() > 0) &&
        (parser.errors_seen_mask() == Css::Parser::kNoError)) {
      for (int i = 0, n = imports.size(); i < n; ++i) {
        Css::Import* import = imports[i];
        StringPiece url(import->link().utf8_data(),
                        import->link().utf8_length());
        // Create new link element to replace the @import.
        HtmlElement* link_element =
            driver_->NewElement(style_element_->parent(), HtmlName::kLink);
        if (driver_->MimeTypeXhtmlStatus() != RewriteDriver::kIsNotXhtml) {
          link_element->set_style(HtmlElement::BRIEF_CLOSE);
        }
        driver_->AddAttribute(link_element, HtmlName::kRel,
                              CssTagScanner::kStylesheet);
        driver_->AddAttribute(link_element, HtmlName::kHref, url);
        // Add all of the style attributes to the link.
        const HtmlElement::AttributeList& attrs(style_element_->attributes());
        for (HtmlElement::AttributeConstIterator j(attrs.begin());
             j != attrs.end(); ++j) {
          const HtmlElement::Attribute& attr = *j;
          // If there's a media attribute, forget our remembered one so that we
          // copy over the import's rather than the style's; although they're
          // equivalent it's best to keep the "original".
          if (attr.name().keyword() == HtmlName::kMedia) {
            media[i].clear();
          }
          link_element->AddAttribute(attr);
        }
        if (!media[i].empty()) {
          driver_->AddAttribute(link_element, HtmlName::kMedia, media[i]);
        }
        // Add the link to the DOM.
        driver_->InsertNodeBeforeNode(style_element_, link_element);
      }

      if (parser.Done()) {
        // <style> contained only @imports, so remove it now.
        if (!driver_->DeleteNode(style_element_)) {
          driver_->ErrorHere("Failed to delete inline style element");
        }
      } else {
        // Erase parsed @imports from contents, but leave rest of CSS.
        int parser_offset = parser.CurrentOffset();
        style_characters_->mutable_contents()->erase(0, parser_offset);
        // Note: parser cannot be used after this point. It contains invalid
        // pointers into the old style_characters_->contents().
      }

      counter_->Add(1);
    }
  }
}

}  // namespace net_instaweb
