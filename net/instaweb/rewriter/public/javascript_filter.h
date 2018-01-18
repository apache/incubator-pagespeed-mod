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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_

#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_filter.h"
#include "pagespeed/kernel/html/html_node.h"

namespace net_instaweb {

/*
 * Find Javascript elements (either inline scripts or imported js files) and
 * rewrite them.  This can involve any combination of minifaction,
 * concatenation, renaming, reordering, and incrementalization that accomplishes
 * our goals.
 *
 * For the moment we keep it simple and just minify any scripts that we find.
 *
 * Challenges:
 *  * Identifying everywhere js is invoked, in particular event handlers on
 *    elements that might be found in css or in variously-randomly-named
 *    html properties.
 *  * Analysis of eval() contexts.  Actually less hard than the last, assuming
 *    constant strings.  Otherwise hard.
 *  * Figuring out where to re-inject code after analysis.
 *
 * We will probably need to do an end run around the need for js analysis by
 * instrumenting and incrementally loading code, then probably using dynamic
 * feedback to change the runtime instrumentation in future pages as we serve
 * them.
 */
class JavascriptFilter : public RewriteFilter {
 public:
  explicit JavascriptFilter(RewriteDriver* rewrite_driver);
  ~JavascriptFilter() override;
  static void InitStats(Statistics* statistics);

  void StartDocumentImpl() override { InitializeConfigIfNecessary(); }
  void StartElementImpl(HtmlElement* element) override;
  void Characters(HtmlCharactersNode* characters) override;
  void EndElementImpl(HtmlElement* element) override;
  void IEDirective(HtmlIEDirectiveNode* directive) override;

  const char* Name() const override { return "JavascriptFilter"; }
  const char* id() const override { return RewriteOptions::kJavascriptMinId; }
  RewriteContext* MakeRewriteContext() override;
  ScriptUsage GetScriptUsage() const override { return kWillInjectScripts; }

  static JavascriptRewriteConfig* InitializeConfig(RewriteDriver* driver);

 protected:
  RewriteContext* MakeNestedRewriteContext(
      RewriteContext* parent, const ResourceSlotPtr& slot) override;

 private:
  class Context;

  typedef enum {
    kNoScript,
    kExternalScript,
    kInlineScript
  } ScriptType;

  inline void RewriteInlineScript(HtmlCharactersNode* body_node);
  inline void RewriteExternalScript(
      HtmlElement* script_in_progress, HtmlElement::Attribute* script_src);

  // Set up config_ if it has not already been initialized.  We must do this
  // lazily because at filter creation time many of the options have not yet
  // been set up correctly.
  void InitializeConfigIfNecessary();

  // Used to distinguish requests for jm (Minified JavaScript) and
  // sm (JavaScript Source Map) resources.
  virtual bool output_source_map() const { return false; }

  ScriptType script_type_;
  // some_missing_scripts indicates that we stopped processing a script and
  // therefore can't assume we know all of the Javascript on a page.
  bool some_missing_scripts_;
  scoped_ptr<JavascriptRewriteConfig> config_;
  ScriptTagScanner script_tag_scanner_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptFilter);
};

class JavascriptSourceMapFilter : public JavascriptFilter {
 public:
  explicit JavascriptSourceMapFilter(RewriteDriver* rewrite_driver);
  ~JavascriptSourceMapFilter() override;

  const char* Name() const override { return "Javascript_Source_Map"; }
  const char* id() const override {
    return RewriteOptions::kJavascriptMinSourceMapId;
  }

 private:
  bool output_source_map() const override { return true; }
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_JAVASCRIPT_FILTER_H_
