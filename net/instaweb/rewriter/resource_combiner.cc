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
// Implementation of ResourceCombiner, a helper for filters that combine
// multiple resources. Also contains CombinerCallback, which is used to collect
// input resources when doing a ResourceCombiner::Fetch.

#include "net/instaweb/rewriter/public/resource_combiner.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_filter.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_partnership.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/url_escaper.h"
#include "pagespeed/kernel/util/url_multipart_encoder.h"

namespace net_instaweb {

ResourceCombiner::ResourceCombiner(RewriteDriver* driver,
                                   const StringPiece& extension,
                                   RewriteFilter* filter)
    : server_context_(driver->server_context()),
      rewrite_driver_(driver),
      partnership_(driver),
      prev_num_components_(0),
      accumulated_leaf_size_(0),
      // TODO(jmarantz): The URL overhead computation is arguably fragile.
      // Another approach is to put a CHECK that the final URL with the
      // resource naming does not exceed the limit.
      //
      // Another option too is to just instantiate a ResourceNamer and a
      // hasher put in the correct ID and EXT and leave the name blank and
      // take size of that.
      url_overhead_(strlen(filter->id()) + ResourceNamer::kOverhead +
                    extension.size()),
      filter_(filter) {
  // This CHECK is here because RewriteDriver is constructed with its
  // server_context_ == NULL.
  // TODO(sligocki): Construct RewriteDriver with a ServerContext, to avoid
  // worrying about it not getting initialized.
  CHECK(server_context_ != NULL);
}

ResourceCombiner::~ResourceCombiner() {
  Clear();
}

TimedBool ResourceCombiner::AddResourceNoFetch(const ResourcePtr& resource,
                                               MessageHandler* handler) {
  TimedBool ret = {0, false};

  // Assert the sanity of three parallel vectors.
  CHECK_EQ(num_urls(), static_cast<int>(resources_.size()));
  CHECK_EQ(num_urls(), static_cast<int>(multipart_encoder_urls_.size()));
  if (num_urls() == 0) {
    // Make sure to initialize the base URL.
    Reset();
  }

  // From here on out, the answer will not change until the resource itself
  // does.
  ret.expiration_ms = resource->CacheExpirationTimeMs();

  // Make sure the specific filter is OK with the data --- it may be
  // unable to combine it safely
  GoogleString failure_reason;
  if (!ResourceCombinable(resource.get(), &failure_reason, handler)) {
    handler->Message(
        kInfo, "Cannot combine %s: resource not combinable, reason: %s",
        resource->url().c_str(), failure_reason.c_str());
    return ret;
  }

  // Now manage the URL and policy.
  bool added = partnership_.AddUrl(resource->url(), handler);

  if (added) {
    int index = num_urls() - 1;

    if (partnership_.NumCommonComponents() != prev_num_components_) {
      UpdateResolvedBase();
    }
    const GoogleString relative_path = partnership_.RelativePath(index);
    multipart_encoder_urls_.push_back(relative_path);

    if (accumulated_leaf_size_ == 0) {
      ComputeLeafSize();
    } else {
      AccumulateLeafSize(relative_path);
    }

    AccumulateCombinedSize(resource);

    resources_.push_back(resource);
    const char* failure_reason = NULL;
    if (ContentSizeTooBig()) {
      failure_reason = "combined contents too big.";
    } else if (UrlTooBig()) {
      failure_reason = "combined url too long.";
    }
    if (failure_reason != NULL) {
      handler->Message(
          kInfo, "Cannot combine %s: %s",
          resource->url().c_str(), failure_reason);
      RemoveLastResource();
      added = false;
    }
  } else {
    handler->MessageS(kInfo, "Cannot combine: partnership forbids");
  }
  ret.value = added;
  return ret;
}

void ResourceCombiner::RemoveLastResource() {
  partnership_.RemoveLast();
  resources_.pop_back();
  multipart_encoder_urls_.pop_back();
  if (partnership_.NumCommonComponents() != prev_num_components_) {
    UpdateResolvedBase();
  }
}

GoogleString ResourceCombiner::UrlSafeId() const {
  GoogleString segment;
  UrlMultipartEncoder encoder;
  encoder.Encode(multipart_encoder_urls_, NULL, &segment);
  return segment;
}

void ResourceCombiner::ComputeLeafSize() {
  GoogleString segment = UrlSafeId();
  accumulated_leaf_size_ = segment.size() + url_overhead_
      + server_context_->hasher()->HashSizeInChars();
}

void ResourceCombiner::AccumulateLeafSize(const StringPiece& url) {
  GoogleString segment;
  UrlEscaper::EncodeToUrlSegment(url, &segment);
  const int kMultipartOverhead = 1;  // for the '+'
  accumulated_leaf_size_ += segment.size() + kMultipartOverhead;
}

bool ResourceCombiner::UrlTooBig() {
  // Note: We include kUrlSlack in our computations so that other filters,
  // which might add to URL length, can run after ours
  int expanded_size = accumulated_leaf_size_ + ResourceCombiner::kUrlSlack;

  if (expanded_size > rewrite_driver_->options()->max_url_segment_size()) {
    return true;
  }

  if ((expanded_size + static_cast<int>(resolved_base_.size())) >
      rewrite_driver_->options()->max_url_size()) {
    return true;
  }
  return false;
}

bool ResourceCombiner::ResourceCombinable(
    Resource* /*resource*/,
    GoogleString* /*failure_reason*/,
    MessageHandler* /*handler*/) {
  return true;
}

void ResourceCombiner::UpdateResolvedBase() {
  // If the addition of this URL changes the base path,
  // then we will have to recompute the multi-part encoding.
  // This is n^2 in the pathological case and if this code
  // gets used for image spriting then this
  // algorithm should be revisited.  For CSS and JS we expect N to
  // be relatively small.
  prev_num_components_ = partnership_.NumCommonComponents();
  resolved_base_ = ResolvedBase();
  multipart_encoder_urls_.clear();
  for (size_t i = 0; i < resources_.size(); ++i) {
    multipart_encoder_urls_.push_back(partnership_.RelativePath(i));
  }

  accumulated_leaf_size_ = 0;
}

OutputResourcePtr ResourceCombiner::Combine(MessageHandler* handler) {
  OutputResourcePtr combination;
  if (resources_.size() <= 1) {
    // No point in combining.
    return combination;
  }
  // First, compute the name of the new resource based on the names of
  // the old resources.
  GoogleString url_safe_id = UrlSafeId();
  // Start building up the combination.  At this point we are still
  // not committed to the combination, because the 'write' can fail.
  // TODO(jmaessen, jmarantz): encode based on partnership
  GoogleString resolved_base = ResolvedBase();
  GoogleString failure_reason;
  combination.reset(rewrite_driver_->CreateOutputResourceWithMappedPath(
      resolved_base, resolved_base, filter_->id(), url_safe_id,
      kRewrittenResource, &failure_reason));
  if (combination.get() == NULL) {
    // TODO(sligocki): Note failure_reason somewhere.
  } else {
    if (combination->cached_result() != NULL &&
        combination->cached_result()->optimizable()) {
      // If the combination has a Url set on it we have cached information
      // on what the output would be, so we'll just use that.
      return combination;
    }
    if (WriteCombination(resources_, combination, handler)
        && combination->IsWritten()) {
      // Otherwise, we have to compute it.
      return combination;
    }
    // No dice.
    combination.clear();
  }
  return combination;
}

bool ResourceCombiner::WriteCombination(
    const ResourceVector& combine_resources,
    const OutputResourcePtr& combination,
    MessageHandler* handler) {
  bool written = true;
  // TODO(sligocki): Write directly to a temp file rather than doing the extra
  // string copy.
  GoogleString combined_contents;
  StringWriter writer(&combined_contents);
  for (int i = 0, n = combine_resources.size(); written && (i < n); ++i) {
    ResourcePtr input(combine_resources[i]);
    written = WritePiece(i, n, input.get(),
                         combination.get(), &writer, handler);
  }
  if (written) {
    // Intersect the response headers from each input.
    ResponseHeaders* output_headers = combination->response_headers();
    DCHECK_EQ(0, output_headers->NumAttributes());

    // We don't copy over all the resources from [0] because we don't
    // want the input cache-control.  The output cache-control is set via
    // RewriteDriver::Write when it calls
    // RewriteDriver::SetDefaultLongCacheHeaders.
    server_context_->MergeNonCachingResponseHeaders(
        *combine_resources[0]->response_headers(), output_headers);
    for (int i = 1, n = combine_resources.size(); i < n; ++i) {
      output_headers->RemoveIfNotIn(*combine_resources[i]->response_headers());
    }

    // TODO(morlovich): Fix combiners to deal with charsets.
    written =
        rewrite_driver_->Write(
            combine_resources, combined_contents, CombinationContentType(),
            StringPiece() /* not computing charset for now */,
            combination.get());
  }
  return written;
}

bool ResourceCombiner::WritePiece(int index,
                                  int num_pieces,
                                  const Resource* input,
                                  OutputResource* /*combination*/,
                                  Writer* writer,
                                  MessageHandler* handler) {
  return writer->Write(input->ExtractUncompressedContents(), handler);
}

void ResourceCombiner::Clear() {
  resources_.clear();
  multipart_encoder_urls_.clear();
}

void ResourceCombiner::Reset() {
  Clear();
  partnership_.Reset(rewrite_driver_->base_url());
  prev_num_components_ = 0;
  accumulated_leaf_size_ = 0;
  resolved_base_.clear();
}

}  // namespace net_instaweb
