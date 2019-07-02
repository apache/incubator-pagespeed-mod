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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class Statistics;

// Implements deferring of javascripts into post onload.
// JsDisableFilter moves scripts inside a noscript tag. This
// filter adds a javascript that goes through every noscript
// tag to defer them to be executed at onload of window.
class JsDeferDisabledFilter : public CommonFilter {
 public:
  explicit JsDeferDisabledFilter(RewriteDriver* driver);
  virtual ~JsDeferDisabledFilter();

  virtual void DetermineEnabled(GoogleString* disabled_reason);
  virtual const char* Name() const { return "JsDeferDisabledFilter"; }

  static void InitStats(Statistics* statistics);
  static void Terminate();

  // JsDeferDisableFilter will be no op for the request if ShouldApply returns
  // false.
  static bool ShouldApply(RewriteDriver* driver);

  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 private:
  virtual void EndDocument();

  virtual void StartDocumentImpl() {}
  virtual void StartElementImpl(HtmlElement* element) {}
  virtual void EndElementImpl(HtmlElement* element) {}

  void InsertJsDeferCode();

  DISALLOW_COPY_AND_ASSIGN(JsDeferDisabledFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JS_DEFER_DISABLED_FILTER_H_
