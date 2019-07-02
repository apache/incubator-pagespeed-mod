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


#include "net/instaweb/rewriter/public/simple_text_filter.h"

#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

SimpleTextFilter::Rewriter::~Rewriter() {
}

SimpleTextFilter::SimpleTextFilter(Rewriter* rewriter, RewriteDriver* driver)
    : RewriteFilter(driver),
      rewriter_(rewriter) {
}

SimpleTextFilter::~SimpleTextFilter() {
}

SimpleTextFilter::Context::Context(const RewriterPtr& rewriter,
                                   RewriteDriver* driver,
                                   RewriteContext* parent)
    : SingleRewriteContext(driver, parent, NULL),
      rewriter_(rewriter) {
}

SimpleTextFilter::Context::~Context() {
}

void SimpleTextFilter::Context::RewriteSingle(const ResourcePtr& input,
                                              const OutputResourcePtr& output) {
  RewriteResult result = kRewriteFailed;
  GoogleString rewritten;
  ServerContext* server_context = FindServerContext();
  if (rewriter_->RewriteText(input->url(), input->ExtractUncompressedContents(),
                             &rewritten, server_context)) {
    const ContentType* output_type = input->type();
    if (output_type == NULL) {
      output_type = &kContentTypeText;
    }
    if (Driver()->Write(
            ResourceVector(1, input), rewritten, output_type, input->charset(),
            output.get())) {
      result = kRewriteOk;
    }
  }
  RewriteDone(result, 0);
}

void SimpleTextFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* attr = rewriter_->FindResourceAttribute(element);
  if (attr == NULL) {
    return;
  }
  ResourcePtr resource(CreateInputResourceOrInsertDebugComment(
      attr->DecodedValueOrNull(), RewriteDriver::InputRole::kUnknown, element));
  if (resource.get() == NULL) {
    return;
  }

  ResourceSlotPtr slot(driver()->GetSlot(resource, element, attr));
  // This 'new' is paired with a delete in RewriteContext::FinishFetch()
  Context* context = new Context(rewriter_, driver(), NULL);
  context->AddSlot(slot);
  driver()->InitiateRewrite(context);
}

RewriteContext* SimpleTextFilter::MakeRewriteContext() {
  return new Context(rewriter_, driver(), NULL);
}

RewriteContext* SimpleTextFilter::MakeNestedRewriteContext(
    RewriteContext* parent, const ResourceSlotPtr& slot) {
  RewriteContext* context = new Context(rewriter_, NULL, parent);
  context->AddSlot(slot);
  return context;
}

}  // namespace net_instaweb
