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

//
// Implementation of JsCombineFilter class which combines multiple external JS
// scripts into a single one. JsCombineFilter contains logic to decide when to
// combine based on the HTML event stream, while the actual combining and
// content-based vetoing is delegated to the JsCombineFilter::JsCombiner helper.
// That in turn largely relies on the common logic in its parent classes to
// deal with resource management.

#include "net/instaweb/rewriter/public/js_combine_filter.h"

#include <map>
#include <vector>
#include <utility>

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/input_info.pb.h"
#include "net/instaweb/rewriter/public/javascript_code_block.h"
#include "net/instaweb/rewriter/public/javascript_filter.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_combiner.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_context.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_result.h"
#include "net/instaweb/rewriter/public/script_tag_scanner.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/html/html_node.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/js/js_keywords.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

using pagespeed::JsKeywords;

namespace net_instaweb {

class MessageHandler;

const char JsCombineFilter::kJsFileCountReduction[] = "js_file_count_reduction";

// See file comment and ResourceCombiner docs for this class's role.
class JsCombineFilter::JsCombiner : public ResourceCombiner {
 public:
  JsCombiner(JsCombineFilter* filter, RewriteDriver* driver)
      : ResourceCombiner(driver, kContentTypeJavascript.file_extension() + 1,
                         filter),
        filter_(filter),
        combined_js_size_(0) {
    Statistics* stats = server_context_->statistics();
    js_file_count_reduction_ = stats->GetVariable(kJsFileCountReduction);
  }

  virtual ~JsCombiner() {
    STLDeleteValues(&code_blocks_);
  }

  virtual bool ResourceCombinable(
      Resource* resource, GoogleString* failure_reason,
      MessageHandler* handler) {
    // Get the charset for the given resource.
    StringPiece this_charset = RewriteFilter::GetCharsetForScript(
        resource, attribute_charset_, rewrite_driver_->containing_charset());

    // This resource's charset must match that of the combination so far.
    // TODO(matterbury): Correctly handle UTF-16 and UTF-32 without the BE/LE
    // suffixes, which are legal if we can determine endianness some other way.
    if (num_urls() == 0) {
      combined_charset_ = this_charset;
    } else if (!StringCaseEqual(combined_charset_, this_charset)) {
      *failure_reason = StrCat("Charset mismatch; combination thus far is ",
                               combined_charset_, " file is ", this_charset);
      return false;
    }

    // In strict mode of ES262-5 eval runs in a private variable scope,
    // (see 10.4.2 step 3 and 10.4.2.1), so our transformation is not safe.
    if (IsLikelyStrictMode(filter_->server_context()->js_tokenizer_patterns(),
                           resource->ExtractUncompressedContents())) {
      *failure_reason = "Combining strict mode files unsupported";
      return false;
    }
    const RewriteOptions* options = rewrite_driver_->options();
    if (options->avoid_renaming_introspective_javascript() &&
        JavascriptCodeBlock::UnsafeToRename(
            resource->ExtractUncompressedContents())) {
      *failure_reason = "File seems to look for its URL";
      return false;
    }

    if (options->Enabled(
            RewriteOptions::kCanonicalizeJavascriptLibraries)) {
      JavascriptCodeBlock* code_block = BlockForResource(resource);
      if (!code_block->ComputeJavascriptLibrary().empty()) {
        // TODO(morlovich): We may be double-counting some stats here.
        *failure_reason = "Will be handled as standard library";
        return false;
      }
    }

    // TODO(morlovich): define a pragma that javascript authors can
    // include in their source to prevent inclusion in a js combination
    return true;
  }

  virtual bool ContentSizeTooBig() const {
    int64 combined_js_max_size =
        rewrite_driver_->options()->max_combined_js_bytes();

    if (combined_js_max_size >= 0 &&
        combined_js_size_ > combined_js_max_size) {
      return true;
    }
    return false;
  }

  virtual void AccumulateCombinedSize(const ResourcePtr& resource) {
    combined_js_size_ += resource->UncompressedContentsSize();
  }

  virtual void Clear() {
    ResourceCombiner::Clear();
    STLDeleteValues(&code_blocks_);
    combined_js_size_ = 0;
  }

  // This eventually calls WritePiece().
  bool Write(const ResourceVector& in, const OutputResourcePtr& out) {
    return WriteCombination(in, out, rewrite_driver_->message_handler());
  }

  // Create the output resource for this combination.
  OutputResourcePtr MakeOutput() {
    return Combine(rewrite_driver_->message_handler());
  }

  // Stats.
  void AddFileCountReduction(int files) {
    js_file_count_reduction_->Add(files);
    if (files >= 1) {
      filter_->LogFilterModifiedContent();
    }
  }

  // Set the attribute charset of the resource being combined. This is the
  // charset taken from the resource's element's charset= attribute, if any.
  void set_resources_attribute_charset(StringPiece charset) {
    attribute_charset_ = charset;
  }

 private:
  typedef std::map<const Resource*, JavascriptCodeBlock*> CodeBlockMap;

  virtual const ContentType* CombinationContentType() {
    return &kContentTypeJavascript;
  }

  virtual bool WritePiece(int index, int num_pieces, const Resource* input,
                          OutputResource* combination, Writer* writer,
                          MessageHandler* handler);

  JavascriptCodeBlock* BlockForResource(const Resource* input);

  JsCombineFilter* filter_;
  int64 combined_js_size_;
  Variable* js_file_count_reduction_;
  // The charset from the resource's element, set by our owning Context's
  // Partition() method each time it checks if a resource can be added to the
  // current combination. The value is only safe to use in ResourceCombinable()
  // since it's set just before that's called and its life past that is not
  // guaranteed.
  StringPiece attribute_charset_;
  // The charset of the combination so far.
  StringPiece combined_charset_;

  scoped_ptr<JavascriptRewriteConfig> config_;
  CodeBlockMap code_blocks_;

  DISALLOW_COPY_AND_ASSIGN(JsCombiner);
};

class JsCombineFilter::Context : public RewriteContext {
 public:
  Context(RewriteDriver* driver, JsCombineFilter* filter)
      : RewriteContext(driver, NULL, NULL),
        combiner_(filter, driver),
        filter_(filter),
        fresh_combination_(true) {
  }

  // Create and add the slot that corresponds to this element.
  bool AddElement(HtmlElement* element, HtmlElement::Attribute* href) {
    ResourcePtr resource(filter_->CreateInputResourceOrInsertDebugComment(
        href->DecodedValueOrNull(), RewriteDriver::InputRole::kScript,
        element));
    if (resource.get() == NULL) {
      return false;
    }
    ResourceSlotPtr slot(Driver()->GetSlot(resource, element, href));
    AddSlot(slot);
    fresh_combination_ = false;
    elements_.push_back(element);
    // Extract the charset, if any, from the element while it's valid.
    StringPiece elements_charset(element->AttributeValue(HtmlName::kCharset));
    elements_charset.CopyToString(StringVectorAdd(&elements_charsets_));
    return true;
  }

  // If we get a flush in the middle of things, we may have put a
  // script tag on that now can't be re-written and should be removed
  // from the combination.  Remove the corresponding slot as well,
  // because we are no longer handling the resource associated with it.
  void RemoveLastElement() {
    RemoveLastSlot();
    elements_.pop_back();
    elements_charsets_.pop_back();
  }

  bool HasElementLast(HtmlElement* element) {
    return !empty() && elements_.back() == element;
  }

  JsCombiner* combiner() { return &combiner_; }
  bool empty() const { return elements_.empty(); }
  bool fresh_combination() { return fresh_combination_; }

  void Reset() {
    fresh_combination_ = true;
    combiner_.Reset();
  }

 protected:
  virtual void PartitionAsync(OutputPartitions* partitions,
                              OutputResourceVector* outputs) {
    // Partitioning here requires JS minification, so we want to
    // move it to a different thread.
    Driver()->AddLowPriorityRewriteTask(MakeFunction(
        this, &Context::PartitionImpl, &Context::PartitionCancel,
        partitions, outputs));
  }

  void PartitionCancel(OutputPartitions* partitions,
                       OutputResourceVector* outputs) {
    CrossThreadPartitionDone(kTooBusy);
  }

  // Divide the slots into partitions according to which js files can
  // be combined together.
  void PartitionImpl(OutputPartitions* partitions,
                     OutputResourceVector* outputs) {
    MessageHandler* handler = Driver()->message_handler();
    CachedResult* partition = NULL;
    CHECK_EQ(static_cast<int>(elements_.size()), num_slots());
    CHECK_EQ(static_cast<int>(elements_charsets_.size()), num_slots());

    // For each slot, try to add its resource to the current partition.
    // If we can't, then finalize the last combination, and then
    // move on to the next slot.
    for (int i = 0, n = num_slots(); i < n; ++i) {
      bool add_input = false;
      ResourcePtr resource(slot(i)->resource());
      if (resource->IsSafeToRewrite(rewrite_uncacheable())) {
        combiner_.set_resources_attribute_charset(elements_charsets_[i]);
        if (combiner_.AddResourceNoFetch(resource, handler).value) {
          add_input = true;
        } else if (partition != NULL) {
          FinalizePartition(partitions, partition, outputs);
          partition = NULL;
          if (combiner_.AddResourceNoFetch(resource, handler).value) {
            add_input = true;
          }
        }
      } else {
        FinalizePartition(partitions, partition, outputs);
        partition = NULL;
      }
      if (add_input) {
        if (partition == NULL) {
          partition = partitions->add_partition();
        }
        resource->AddInputInfoToPartition(
            Resource::kIncludeInputHash, i, partition);
      }
    }
    FinalizePartition(partitions, partition, outputs);
    CrossThreadPartitionDone(partitions->partition_size() != 0 ?
                                 kRewriteOk : kRewriteFailed);
  }

  // Actually write the new resource.
  virtual void Rewrite(int partition_index,
                       CachedResult* partition,
                       const OutputResourcePtr& output) {
    RewriteResult result = kRewriteOk;
    if (!output->IsWritten()) {
      ResourceVector resources;
      for (int i = 0, n = num_slots(); i < n; ++i) {
        ResourcePtr resource(slot(i)->resource());
        resources.push_back(resource);
      }
      if (!combiner_.Write(resources, output)) {
        result = kRewriteFailed;
      }
    }
    RewriteDone(result, partition_index);
  }

  bool PolicyPermitsRendering() const {
    return AreOutputsAllowedByCsp(CspDirective::kScriptSrc);
  }

  // For every partition, write a new script tag that points to the
  // combined resource.  Then create new script tags for each slot
  // in the partition that evaluate the variable that refers to the
  // original script for that tag.
  virtual void Render() {
    for (int p = 0, np = num_output_partitions(); p < np; ++p) {
      const CachedResult* partition = output_partition(p);
      int partition_size = partition->input_size();
      if (partition_size > 1) {
        // Make sure we can edit every element here.
        bool can_rewrite = true;
        for (int i = 0; i < partition_size; ++i) {
          int slot_index = partition->input(i).index();
          HtmlResourceSlot* html_slot =
              static_cast<HtmlResourceSlot*>(slot(slot_index).get());
          if (!Driver()->IsRewritable(html_slot->element())) {
            can_rewrite = false;
          }
        }

        if (can_rewrite) {
          MakeCombinedElement(partition);
          // we still need to add eval() in place of the
          // other slots.
          for (int i = 0; i < partition_size; ++i) {
            int slot_index = partition->input(i).index();
            MakeScriptElement(slot_index);
          }
          combiner_.AddFileCountReduction(partition_size - 1);
        } else {
          // Disable slot rendering, because we're doing all the rendering here.
          for (int i = 0; i < partition_size; ++i) {
            slot(partition->input(i).index())->set_disable_rendering(true);
          }
        }  // if (can_rewrite)
      }  // if (partition_size > 1)
    }
  }

  virtual const UrlSegmentEncoder* encoder() const {
    return filter_->encoder();
  }
  virtual const char* id() const { return filter_->id(); }
  virtual OutputResourceKind kind() const { return kRewrittenResource; }

  virtual GoogleString CacheKeySuffix() const {
    // Updated to make sure certain bugfixes actually deploy, and we don't
    // end up using old broken cached version.
    return "v4";
  }

 private:
  // If we can combine, put the result into outputs and then reset
  // the context (and the combiner) so we start with a fresh slate
  // for any new slots.
  void FinalizePartition(OutputPartitions* partitions,
                         CachedResult* partition,
                         OutputResourceVector* outputs) {
    if (partition != NULL) {
      OutputResourcePtr combination_output(combiner_.MakeOutput());
      if (combination_output.get() == NULL) {
        partitions->mutable_partition()->RemoveLast();
      } else {
        combination_output->UpdateCachedResultPreservingInputInfo(partition);
        outputs->push_back(combination_output);
      }
      Reset();
    }
  }

  // Create an element for the combination of all the elements in the
  // partition. Insert it before first one.
  void MakeCombinedElement(const CachedResult* partition) {
    int first_index = partition->input(0).index();
    HtmlResourceSlot* first_slot =
        static_cast<HtmlResourceSlot*>(slot(first_index).get());
    HtmlElement* combine_element =
        Driver()->NewElement(NULL,  // no parent yet.
                             HtmlName::kScript);
    Driver()->InsertNodeBeforeNode(first_slot->element(), combine_element);
    Driver()->AddAttribute(combine_element, HtmlName::kSrc,
                           ResourceSlot::RelativizeOrPassthrough(
                               Driver()->options(), partition->url(),
                               first_slot->url_relativity(),
                               Driver()->base_url()));
  }

  // Make a script element with eval(<variable name>), and replace
  // the existing element with it.
  void MakeScriptElement(int slot_index) {
    HtmlResourceSlot* html_slot = static_cast<HtmlResourceSlot*>(
        slot(slot_index).get());
    // Create a new element that doesn't have any children the
    // original element had.
    HtmlElement* original = html_slot->element();
    HtmlElement* element = Driver()->NewElement(NULL, HtmlName::kScript);
    Driver()->InsertNodeBeforeNode(original, element);
    GoogleString var_name = filter_->VarName(Driver(),
                                             html_slot->resource()->url());
    HtmlNode* script_code = Driver()->NewCharactersNode(
        element, StrCat("eval(", var_name, ");"));
    Driver()->AppendChild(element, script_code);
    html_slot->RequestDeleteElement();
  }

  JsCombineFilter::JsCombiner combiner_;
  JsCombineFilter* filter_;
  bool fresh_combination_;
  // Each of the elements for the resources being combined are added to this
  // vector, but those elements will be free'd after the end of the document,
  // though this context might survive past that (as it's an asynchronous
  // rewriting thread). Therefore the contents of this vector are not usable
  // in any of the rewriting callbacks: Partition, Rewrite, and Render.
  std::vector<HtmlElement*> elements_;
  StringVector elements_charsets_;  // charset for each element added, if any.
};

bool JsCombineFilter::JsCombiner::WritePiece(
    int index, int num_pieces, const Resource* input,
    OutputResource* combination, Writer* writer, MessageHandler* handler) {
  // Minify if needed.
  StringPiece not_escaped = input->ExtractUncompressedContents();

  // TODO(morlovich): And now we're not updating some stats instead.
  // Factor out that bit in JsFilter.
  const RewriteOptions* options = rewrite_driver_->options();
  if (options->Enabled(RewriteOptions::kRewriteJavascriptExternal)) {
    JavascriptCodeBlock* code_block = BlockForResource(input);
    if (code_block->successfully_rewritten()) {
      not_escaped = code_block->rewritten_code();
    }
  }

  // We write out code of each script into a variable.
  writer->Write(StrCat("var ",
                       JsCombineFilter::VarName(
                           rewrite_driver_, input->url()),
                       " = "),
                handler);

  GoogleString escaped;
  JavascriptCodeBlock::ToJsStringLiteral(not_escaped, &escaped);

  writer->Write(escaped, handler);
  writer->Write(";\n", handler);
  return true;
}

JavascriptCodeBlock* JsCombineFilter::JsCombiner::BlockForResource(
    const Resource* input) {
  std::pair<CodeBlockMap::iterator, bool> insert_result =
      code_blocks_.insert(CodeBlockMap::value_type(input, NULL));

  if (insert_result.second) {
    // Actually inserted, so we need a value.
    if (config_.get() == NULL) {
      config_.reset(JavascriptFilter::InitializeConfig(rewrite_driver_));
    }

    scoped_ptr<JavascriptCodeBlock> new_block(new JavascriptCodeBlock(
        input->ExtractUncompressedContents(), config_.get(), input->url(),
        rewrite_driver_->message_handler()));
    new_block->Rewrite();
    insert_result.first->second = new_block.release();
  }
  return insert_result.first->second;
}

JsCombineFilter::JsCombineFilter(RewriteDriver* driver)
    : RewriteFilter(driver),
      script_scanner_(driver),
      script_depth_(0),
      current_js_script_(NULL),
      context_(MakeContext()) {
}

JsCombineFilter::~JsCombineFilter() {
}

void JsCombineFilter::InitStats(Statistics* statistics) {
  statistics->AddVariable(kJsFileCountReduction);
}

bool JsCombineFilter::IsLikelyStrictMode(
    const pagespeed::js::JsTokenizerPatterns* jstp, StringPiece input) {
  pagespeed::js::JsTokenizer tokenizer(jstp, input);

  // The prolog is spec'd as a sequence of expression statements
  // consisting only of string literals at beginning of a scope.
  // If one of them is 'use strict' then it indicates strict mode.
  // Rather than worry about finer points of the grammar we basically
  // accept any mixture of strings, semicolons and whitespace.
  while (true) {
    StringPiece token_text;
    JsKeywords::Type token_type = tokenizer.NextToken(&token_text);
    switch (token_type) {
      case JsKeywords::kComment:
      case JsKeywords::kWhitespace:
      case JsKeywords::kLineSeparator:
      case JsKeywords::kSemiInsert:
        // All of these can occur in prologue sections (but not quite that
        // freely).
        break;
      case JsKeywords::kOperator:
        // ; may also be OK, but other stuff isn't.
        if (token_text != ";") {
          return false;
        }
        break;
      case JsKeywords::kStringLiteral:
        if (token_text == "'use strict'" || token_text == "\"use strict\"") {
          return true;
        }
        break;
      default:
        return false;
    }
  }
}

void JsCombineFilter::StartDocumentImpl() {
}

void JsCombineFilter::StartElementImpl(HtmlElement* element) {
  HtmlElement::Attribute* src = NULL;
  ScriptTagScanner::ScriptClassification classification =
      script_scanner_.ParseScriptElement(element, &src);
  switch (classification) {
    case ScriptTagScanner::kNonScript:
      if (script_depth_ > 0) {
        // We somehow got some tag inside a script. Be conservative ---
        // it may be meaningful so we don't want to destroy it;
        // so flush the complete things before us, and call it a day.
        if (context_->HasElementLast(current_js_script_)) {
          context_->RemoveLastElement();
        }
        NextCombination();
      }
      break;

    case ScriptTagScanner::kJavaScript:
      ConsiderJsForCombination(element, src);
      ++script_depth_;
      break;

    case ScriptTagScanner::kUnknownScript:
      // We have something like vbscript. Handle this as a barrier
      NextCombination();
      ++script_depth_;
      break;
  }
}

void JsCombineFilter::EndElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kScript) {
    --script_depth_;
    if (script_depth_ == 0) {
      current_js_script_ = NULL;
    }
  }
}

void JsCombineFilter::IEDirective(HtmlIEDirectiveNode* directive) {
  NextCombination();
}

void JsCombineFilter::Characters(HtmlCharactersNode* characters) {
  // If a script has non-whitespace data inside of it, we cannot
  // replace its contents with a call to eval, as they may be needed.
  if (script_depth_ > 0 && !OnlyWhitespace(characters->contents())) {
    if (context_->HasElementLast(current_js_script_)) {
      context_->RemoveLastElement();
      NextCombination();
    }
  }
}

void JsCombineFilter::Flush() {
  // We try to combine what we have thus far the moment we see a flush.
  // This serves two purposes:
  // 1) Let's us edit elements while they are still rewritable,
  //    but as late as possible.
  // 2) Ensures we do combine eventually (as we will get a flush at the end of
  //    parsing).
  NextCombination();
}

// Determine if we can add this script to the combination or not.
// If not, call NextCombination() to write out what we've got and then
// reset.
void JsCombineFilter::ConsiderJsForCombination(HtmlElement* element,
                                               HtmlElement::Attribute* src) {
  if (!driver()->content_security_policy().PermitsEval()) {
    driver()->InsertDebugComment(
        "Not considering JS combining since CSP forbids eval", element);
    context_->Reset();
    return;
  }

  // Worst-case scenario is if we somehow ended up with nested scripts.
  // In this case, we just give up entirely.
  if (script_depth_ > 0) {
    driver()->WarningHere("Nested <script> elements");
    context_->Reset();
    return;
  }

  // Opening a new script normally...
  current_js_script_ = element;

  // Now we may have something that's not combinable; in those cases we would
  // like to flush as much as possible.
  // TODO(morlovich): if we stick with the current eval-based strategy, this
  // is way too conservative, as we keep multiple script elements for
  // actual execution.

  // If our current script may be inside a noscript, which means
  // we should not be making it runnable.
  if (noscript_element() != NULL) {
    NextCombination();
    return;
  }

  // An inline script.
  if (src == NULL || src->DecodedValueOrNull() == NULL) {
    NextCombination();
    return;
  }

  // Don't combine scripts with the data-pagespeed-no-defer attribute.
  if (element->FindAttribute(HtmlName::kDataPagespeedNoDefer) != NULL ||
      element->FindAttribute(HtmlName::kPagespeedNoDefer) != NULL) {
    NextCombination();
    return;
  }

  // We do not try to merge in a <script with async/defer> or for/event.
  // TODO(morlovich): is it worth combining multiple scripts with
  // async/defer if the flags are the same?
  if (script_scanner_.ExecutionMode(element) != script_scanner_.kExecuteSync) {
    NextCombination();
    return;
  }

  // Now we see if policy permits us merging this element with previous ones.
  context_->AddElement(element, src);
}

GoogleString JsCombineFilter::VarName(const RewriteDriver* driver,
                                      const GoogleString& url) {
  // We want to apply any rewrite mappings, since they can change the directory
  // and hence affect variable names.
  GoogleString output_url;

  GoogleString domain_out;  // ignored.
  GoogleUrl resource_url(url);
  // We can't generally use the preexisting UrlPartnership in the
  // ResourceCombiner since during the .pagespeed. resource fetch it's not
  // filled in.
  UrlPartnership::FindResourceDomain(driver->base_url(),
                                     driver->server_context()->url_namer(),
                                     driver->options(),
                                     &resource_url,
                                     &domain_out,
                                     driver->message_handler());
  if (resource_url.IsWebValid()) {
    resource_url.Spec().CopyToString(&output_url);
  } else {
    LOG(DFATAL) << "Somehow got invalid URL in JsCombineFilter::VarName:"
                << resource_url.UncheckedSpec() << " starting from:"
                << url;
    output_url = url;
  }

  // We hash the non-host portion of URL to keep it consistent when sharding.
  // This is safe since we never include URLs from different hosts in a single
  // combination.
  GoogleString url_hash =
      JavascriptCodeBlock::JsUrlHash(output_url,
                                     driver->server_context()->hasher());

  return StrCat("mod_pagespeed_", url_hash);
}

JsCombineFilter::Context* JsCombineFilter::MakeContext() {
  return new Context(driver(), this);
}

RewriteContext* JsCombineFilter::MakeRewriteContext() {
  return MakeContext();
}

JsCombineFilter::JsCombiner* JsCombineFilter::combiner() const {
  return context_->combiner();
}

// In async flow, tell the rewrite_driver to write out the last
// combination, and reset our context to a new one.
// In sync flow, just write out what we have so far, and then
// reset the context.
void JsCombineFilter::NextCombination() {
  if (!context_->empty() &&
      driver()->content_security_policy().PermitsEval()) {
    driver()->InitiateRewrite(context_.release());
    context_.reset(MakeContext());
  }
  context_->Reset();
}

}  // namespace net_instaweb
