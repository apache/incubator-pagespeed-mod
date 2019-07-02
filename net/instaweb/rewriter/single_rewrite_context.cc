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


#include "net/instaweb/rewriter/public/single_rewrite_context.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_slot.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"

namespace net_instaweb {

SingleRewriteContext::SingleRewriteContext(RewriteDriver* driver,
                                           RewriteContext* parent,
                                           ResourceContext* resource_context)
    : RewriteContext(driver, parent, resource_context) {
}

SingleRewriteContext::~SingleRewriteContext() {
}

bool SingleRewriteContext::Partition(OutputPartitions* partitions,
                                     OutputResourceVector* outputs) {
  bool ret = false;
  if (num_slots() == 1) {
    ret = true;
    ResourcePtr resource(slot(0)->resource());
    GoogleString unsafe_reason;
    if (resource->IsSafeToRewrite(rewrite_uncacheable(), &unsafe_reason)) {
      GoogleString failure_reason;
      OutputResourcePtr output_resource(
          Driver()->CreateOutputResourceFromResource(
              id(), encoder(), resource_context(),
              resource, kind(), &failure_reason));
      if (output_resource.get() == NULL) {
        partitions->add_debug_message(failure_reason);
      } else {
        CachedResult* partition = partitions->add_partition();
        resource->AddInputInfoToPartition(Resource::kIncludeInputHash, 0,
                                          partition);
        output_resource->set_cached_result(partition);
        outputs->push_back(output_resource);
      }
    } else {
      partitions->add_debug_message(unsafe_reason);
    }
  }
  return ret;
}

void SingleRewriteContext::Rewrite(int partition_index,
                                   CachedResult* partition,
                                   const OutputResourcePtr& output_resource) {
  CHECK_EQ(0, partition_index);
  ResourcePtr resource(slot(0)->resource());
  CHECK(resource.get() != NULL);
  CHECK(resource->loaded());
  CHECK(resource->HttpStatusOk());
  if (output_resource.get() != NULL) {
    DCHECK_EQ(output_resource->cached_result(), partition);
  }
  RewriteSingle(resource, output_resource);
}

void SingleRewriteContext::AddLinkRelCanonical(
    const ResourcePtr& input, ResponseHeaders* output) {
  if (output->HasLinkRelCanonical() ||
      input->response_headers()->HasLinkRelCanonical()) {
    return;
  }

  // It's unclear what we should do in case of complex domain mapping
  // configurations, so we simply avoid adding a header in that case.
  //
  // Also note that we may see both the original and rewritten URLs,
  // depending on whether we're handling the HTML or the resource fetch.
  const DomainLawyer* domain_lawyer = Options()->domain_lawyer();
  GoogleUrl input_gurl(input->url());
  if (domain_lawyer->WillDomainChange(input_gurl)) {
    return;
  }

  ConstStringStarVector rewritten_to;
  domain_lawyer->FindDomainsRewrittenTo(input_gurl, &rewritten_to);
  if (!rewritten_to.empty()) {
    return;
  }

  output->Add(HttpAttributes::kLink,
              ResponseHeaders::RelCanonicalHeaderValue(input->url()));
  output->ComputeCaching();
}

void SingleRewriteContext::AddLinkRelCanonicalForFallbackHeaders(
    ResponseHeaders* output) {
  if (num_slots() != 1) {
    LOG(DFATAL) << "Weird number of slots:" << num_slots();
    return;
  }
  ResourcePtr resource(slot(0)->resource());
  if (resource.get() == NULL || !resource->loaded()) {
    return;
  }

  AddLinkRelCanonical(resource, output);
}

}  // namespace net_instaweb
