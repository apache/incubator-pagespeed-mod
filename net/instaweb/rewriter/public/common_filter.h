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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/empty_html_filter.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

// CommonFilter encapsulates useful functionality that many filters will want.
// All filters who want this functionality should inherit from CommonFilter and
// define the Helper methods rather than the main methods.
//
// Currently, it stores whether we are in a <noscript> element (in
// which case, we should be careful about moving things out of this
// element).
//
// The base-tag is maintained in the RewriteDriver, although it can be
// accessed via a convenience method here for historical reasons.
class CommonFilter : public EmptyHtmlFilter {
 public:
  // Debug message to be inserted when resource creation fails.
  static const char kCreateResourceFailedDebugMsg[];

  explicit CommonFilter(RewriteDriver* driver);
  virtual ~CommonFilter();

  // Getters

  // URL of the requested HTML or resource.
  const GoogleUrl& base_url() const;

  // For rewritten resources, decoded_base_url() is the base of the original
  // (un-rewritten) resource's URL.
  const GoogleUrl& decoded_base_url() const;

  RewriteDriver* driver() const { return driver_; }
  HtmlElement* noscript_element() const { return noscript_element_; }

  // Insert a node at the best available location in or near the closing body
  // tag during EndDocument. This is useful for filters that want to insert
  // scripts or summary data at the end of body, but need to wait until
  // EndDocument to do so.
  //
  // Tries to inject just before </body> if nothing else intervenes; otherwise
  // tries to inject before </html> or, failing that, at the end of all content.
  // This latter case still works in browsers, but breaks HTML validation (and
  // is incredibly ugly). It can be necessitated by other post-</html> content,
  // or by flushes in the body.
  //
  // Note that if a subclass overloads the Characters function, it needs to call
  // the parent implementation for this function to be correct.
  void InsertNodeAtBodyEnd(HtmlNode* data);

  // Note: Don't overload these methods, overload the implementers instead!
  virtual void StartDocument();
  virtual void StartElement(HtmlElement* element);
  virtual void EndElement(HtmlElement* element);

  // If a subclass overloads this function and wishes to use
  // InsertNodeAtBodyEnd(), it needs to make an upcall to this implementation
  // for InsertNodeAtBodyEnd() to work correctly.
  virtual void Characters(HtmlCharactersNode* characters);

  // Creates an input resource with the url evaluated based on input_url
  // which may need to be absolutified relative to base_url(). Returns NULL
  // if input resource url isn't valid, or can't legally be rewritten in the
  // context of this page. *is_authorized will be set to false if the domain
  // of input_url is not authorized, which could true of false regardless of
  // the return value: for example if we are allowing inlining of resources
  // from unauthorized domains we will return non-NULL but *is_authorized will
  // be false; converse cases are possible too (e.g. input_url is a data URI).
  ResourcePtr CreateInputResource(StringPiece input_url,
                                  RewriteDriver::InputRole role,
                                  bool* is_authorized);

  // Similar to CreateInputResource except that if the input_url is not
  // authorized we insert a debug comment after the given element if possible
  // (debug is enabled and the element is writable). The returned ResourcePtr
  // is guaranteed to be non-NULL iff the input_url is authorized.
  ResourcePtr CreateInputResourceOrInsertDebugComment(
      StringPiece input_url, RewriteDriver::InputRole role,
      HtmlElement* element);

  // Resolves input_url based on the driver's location and any base tag into
  // out_url. If resolution fails, the resulting URL may be invalid.
  void ResolveUrl(StringPiece input_url, GoogleUrl* out_url);

  bool IsRelativeUrlLoadPermittedByCsp(StringPiece url, CspDirective role);

  // Returns whether or not the base url is valid.  This value will change
  // as a filter processes the document.  E.g. If there are url refs before
  // the base tag is reached, it will return false until the filter sees the
  // base tag.  After the filter sees the base tag, it will return true.
  bool BaseUrlIsValid() const;

  // Returns whether the current options specify the "debug" filter.
  // If set, then other filters can annotate output HTML with HTML
  // comments indicating why they did or did not do an optimization,
  // using HtmlParse::InsertComment.
  bool DebugMode() const { return driver_->DebugMode(); }

  // Utility function to extract the mime type and/or charset from a meta tag,
  // either the HTML4 http-equiv form or the HTML5 charset form:
  // element is the meta tag element to process.
  // headers is optional: if provided it is checked to see if it already has
  //         a content type with the tag's value; if so, returns false.
  // content is set to the content attribute's value, http-equiv form only.
  // mime_type is set to the extracted mime type, if any.
  // charset is the set to the extracted charset, if any.
  // returns true if the details were extracted, false if not. If true is
  // returned then content will be empty for the HTML5 charset form and
  // non-empty for the HTML4 http-equiv form; also an http-equiv attribute
  // with a blank mime type returns false as it's not a valid format.
  static bool ExtractMetaTagDetails(const HtmlElement& element,
                                    const ResponseHeaders* headers,
                                    GoogleString* content,
                                    GoogleString* mime_type,
                                    GoogleString* charset);

  // Returns true if the image element is not in a <noscript> block and it has
  // a) no onload attribute or
  // b) an onload attribute exists with the value being equal to the
  //    CriticalImagesBeaconFilter::kImageOnloadCode.
  bool CanAddPagespeedOnloadToImage(const HtmlElement&);

  // Add this filter to the logged list of applied rewriters. The intended
  // semantics of this are that it should only include filters that modified the
  // content of the response to the request being processed.
  // This class logs using Name(); subclasses may do otherwise.
  virtual void LogFilterModifiedContent();

  // Returns true if this filter allows domains not authorized by any pagespeed
  // directive to be optimized. Filters that end up inlining content onto the
  // HTML are almost the only ones that can safely do this.
  virtual RewriteDriver::InlineAuthorizationPolicy AllowUnauthorizedDomain()
      const { return RewriteDriver::kInlineOnlyAuthorizedResources; }

  // Returns true if the filter intends to inline the resource it fetches.  This
  // is to support AllowWhenInlining.  Unlike AllowUnauthorizedDomain() this
  // doesn't have security implications and is just used for performance tuning.
  virtual bool IntendedForInlining() const { return false; }

  // Add JavaScript code to an HtmlElement*.  Requires MimeTypeXhtmlStatus(),
  // preventing this from going into HtmlParse.
  void AddJsToElement(StringPiece js, HtmlElement* script);

 protected:
  ServerContext* server_context() const { return server_context_; }
  const RewriteOptions* rewrite_options() { return rewrite_options_; }

  // Overload these implementer methods:
  // Intentionally left abstract so that implementers don't forget to change
  // the name from Blah to BlahImpl.
  virtual void StartDocumentImpl() = 0;
  virtual void StartElementImpl(HtmlElement* element) = 0;
  virtual void EndElementImpl(HtmlElement* element) = 0;

  // ID string used in logging. Inheritors should supply whatever short ID
  // string they use.
  virtual const char* LoggingId() { return Name(); }

 private:
  // These fields are gettable by inheritors.
  RewriteDriver* driver_;
  ServerContext* server_context_;
  const RewriteOptions* rewrite_options_;
  HtmlElement* noscript_element_;
  // These are private.
  HtmlElement* end_body_point_;
  bool seen_base_;

  DISALLOW_COPY_AND_ASSIGN(CommonFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_COMMON_FILTER_H_
