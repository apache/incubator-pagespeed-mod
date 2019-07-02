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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"

namespace net_instaweb {
class GoogleUrl;
class MessageHandler;
class RewriteDriver;
class Statistics;
class Variable;

// Filter that trims redundant information from the left end of each url.
//
// For example: if the page's base URL is http://www.example.com/foo/bar.html
// then the following URLs can be trimmed:
//   http://www.example.com/foo/bar/other.html -> bar/other.html
//   http://www.example.com/another.html -> /another.html
//   http://www.example.org/index.html -> //www.example.org/index.html
//
// TODO(jmaessen): Do we care to introduce ../ in order to relativize more urls?
// For example, if base URL is http://www.example.com/foo/bar/index.html
// we could convert: http://www.example.com/foo/other.html -> ../other.html
// rather than -> /foo/other.html.
class UrlLeftTrimFilter : public CommonFilter {
 public:
  UrlLeftTrimFilter(RewriteDriver* rewrite_driver, Statistics* stats);
  virtual ~UrlLeftTrimFilter();

  static void InitStats(Statistics* statistics);
  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element) {}

  virtual const char* Name() const { return "UrlLeftTrim"; }

  // Trim 'url_to_trim' relative to 'base_url' returning the result in
  // 'trimmed_url'. Returns true if we succeeded at trimming the URL.
  //
  // This is static and requires the base_url explicitly, so that it can be
  // called from other places (like the CSS filter).
  static bool Trim(const GoogleUrl& base_url, const StringPiece& url_to_trim,
                   GoogleString* trimmed_url, MessageHandler* handler);

 private:
  void TrimAttribute(HtmlElement::Attribute* attr);
  void ClearBaseUrl();

  friend class UrlLeftTrimFilterTest;

  // Stats on how much trimming we've done.
  Variable* trim_count_;
  Variable* trim_saved_bytes_;

  DISALLOW_COPY_AND_ASSIGN(UrlLeftTrimFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_URL_LEFT_TRIM_FILTER_H_
