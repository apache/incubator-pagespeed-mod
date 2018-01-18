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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_

#include "net/instaweb/rewriter/public/common_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"

namespace net_instaweb {

class Statistics;
class Variable;

// Injects javascript instrumentation for monitoring page-rendering time.
class AddInstrumentationFilter : public CommonFilter {
 public:
  static const char kLoadTag[];
  static const char kUnloadTag[];
  static GoogleString* kUnloadScriptFormatXhtml;
  static GoogleString* kTailScriptFormatXhtml;

  // Counters.
  static const char kInstrumentationScriptAddedCount[];

  explicit AddInstrumentationFilter(RewriteDriver* driver);
  virtual ~AddInstrumentationFilter();

  static void InitStats(Statistics* statistics);

  virtual void StartDocumentImpl();
  virtual void EndDocument();
  virtual void StartElementImpl(HtmlElement* element);
  virtual void EndElementImpl(HtmlElement* element);
  virtual const char* Name() const { return "AddInstrumentation"; }

  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

 protected:
  virtual void DetermineEnabled(GoogleString* disabled_reason);

  // The total number of times instrumentation script is added.
  Variable* instrumentation_script_added_count_;

 private:
  // Returns JS using the specified event.
  GoogleString GetScriptJs(StringPiece event);

  // Adds the kHeadScript just before the current event only if the element is
  // not a <title> or <meta>.
  void AddHeadScript(HtmlElement* element);

  bool found_head_;
  bool added_head_script_;
  bool added_unload_script_;

  DISALLOW_COPY_AND_ASSIGN(AddInstrumentationFilter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_ADD_INSTRUMENTATION_FILTER_H_
