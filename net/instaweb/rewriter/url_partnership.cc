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


#include "net/instaweb/rewriter/public/url_partnership.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

UrlPartnership::UrlPartnership(const RewriteDriver* driver)
    : rewrite_options_(driver->options()),
      url_namer_(driver->server_context()->url_namer()) {
}

UrlPartnership::~UrlPartnership() {
  STLDeleteElements(&url_vector_);
}

// Adds a URL to a combination.  If it can be legally added, consulting
// the DomainLawyer, then true is returned.  AddUrl cannot be called
// after Resolve (CHECK failure).
bool UrlPartnership::AddUrl(const StringPiece& untrimmed_resource_url,
                            MessageHandler* handler) {
  GoogleString resource_url, mapped_domain_name;
  bool ret = false;
  TrimWhitespace(untrimmed_resource_url, &resource_url);

  if (resource_url.empty()) {
    handler->Message(
        kInfo, "Cannot rewrite empty URL relative to %s",
        original_origin_and_path_.spec_c_str());
  } else if (!original_origin_and_path_.IsWebValid()) {
    handler->Message(
        kInfo, "Cannot rewrite %s relative to invalid url %s",
        resource_url.c_str(),
        original_origin_and_path_.spec_c_str());
  } else {
    // First resolve the original request to ensure that it is allowed by the
    // options.
    scoped_ptr<GoogleUrl> resolved_request(
        new GoogleUrl(original_origin_and_path_, resource_url));
    if (!resolved_request->IsWebValid()) {
      handler->Message(
          kInfo, "URL %s cannot be resolved relative to base URL %s",
          resource_url.c_str(),
          original_origin_and_path_.spec_c_str());
    } else if (!rewrite_options_->IsAllowed(resolved_request->Spec())) {
      handler->Message(kInfo,
                       "Rewriting URL %s is disallowed via configuration",
                       resolved_request->spec_c_str());
    } else if (FindResourceDomain(original_origin_and_path_,
                                  url_namer_,
                                  rewrite_options_,
                                  resolved_request.get(),
                                  &mapped_domain_name,
                                  handler)) {
      if (url_vector_.empty()) {
        domain_and_path_prefix_.swap(mapped_domain_name);
        ret = true;
      } else {
        GoogleUrl domain_url(domain_and_path_prefix_);
        GoogleUrl mapped_url(mapped_domain_name);
        ret = (domain_url.Origin() == mapped_url.Origin());
        if (ret && !rewrite_options_->combine_across_paths()) {
          ret = (ResolvedBase() == resolved_request->AllExceptLeaf());
        }
      }

      if (ret) {
        url_vector_.push_back(resolved_request.release());
        int index = url_vector_.size() - 1;
        IncrementalResolve(index);
      }
    }
  }
  return ret;
}

bool UrlPartnership::FindResourceDomain(const GoogleUrl& base_url,
                                        const UrlNamer* url_namer,
                                        const RewriteOptions* rewrite_options,
                                        GoogleUrl* resource,
                                        GoogleString* domain,
                                        MessageHandler* handler) {
  bool ret = false;
  GoogleString resource_url;
  if (url_namer->Decode(*resource, rewrite_options, &resource_url)) {
    resource->Reset(resource_url);
    ret = resource->IsWebValid();
    resource->Origin().CopyToString(domain);
  } else {
    ret = rewrite_options->domain_lawyer()->MapRequestToDomain(
        base_url, resource->Spec(), domain,
        resource, handler);
  }
  return ret;
}

void UrlPartnership::RemoveLast() {
  CHECK(!url_vector_.empty());
  int last = url_vector_.size() - 1;
  delete url_vector_[last];
  url_vector_.resize(last);

  // Re-resolve the entire partnership in the absense of the influence of the
  // ex-partner, by re-adding the GURLs one at a time.
  common_components_.clear();
  for (int i = 0, n = url_vector_.size(); i < n; ++i) {
    IncrementalResolve(i);
  }
}

void UrlPartnership::Reset(const GoogleUrl& original_request) {
  STLDeleteElements(&url_vector_);
  url_vector_.clear();
  common_components_.clear();
  if (original_request.IsWebValid()) {
    original_origin_and_path_.Reset(original_request.AllExceptLeaf());
  }
}

void UrlPartnership::IncrementalResolve(int index) {
  CHECK_LE(0, index);
  CHECK_LT(index, static_cast<int>(url_vector_.size()));

  // When tokenizing a URL, we don't want to omit empty segments
  // because we need to avoid aliasing "http://x" with "/http:/x".
  bool omit_empty = false;
  StringPieceVector components;

  if (index == 0) {
    StringPiece base = url_vector_[0]->AllExceptLeaf();
    SplitStringPieceToVector(base, "/", &components, omit_empty);
    components.pop_back();            // base ends with "/"
    CHECK_LE(3U, components.size());  // expect {"http:", "", "x"...}
    for (size_t i = 0; i < components.size(); ++i) {
      const StringPiece& sp = components[i];
      common_components_.push_back(GoogleString(sp.data(), sp.size()));
    }
  } else {
    // Split each string on / boundaries, then compare these path elements
    // until one doesn't match, then shortening common_components.
    StringPiece all_but_leaf = url_vector_[index]->AllExceptLeaf();
    SplitStringPieceToVector(all_but_leaf, "/", &components, omit_empty);
    components.pop_back();            // base ends with "/"
    CHECK_LE(3U, components.size());  // expect {"http:", "", "x"...}

    if (components.size() < common_components_.size()) {
      common_components_.resize(components.size());
    }
    for (size_t c = 0; c < common_components_.size(); ++c) {
      if (common_components_[c] != components[c]) {
        common_components_.resize(c);
        break;
      }
    }
  }
}

GoogleString UrlPartnership::ResolvedBase() const {
  GoogleString ret;
  if (!common_components_.empty()) {
    for (size_t c = 0; c < common_components_.size(); ++c) {
      const GoogleString& component = common_components_[c];
      ret += component;
      ret += "/";  // initial segment is "http" with no leading /
    }
  }
  return ret;
}

// Returns the relative path of a particular URL that was added into
// the partnership.  This requires that Resolve() be called first.
GoogleString UrlPartnership::RelativePath(int index) const {
  GoogleString resolved_base = ResolvedBase();
  StringPiece spec = url_vector_[index]->Spec();
  CHECK_GE(spec.size(), resolved_base.size());
  CHECK_EQ(StringPiece(spec.data(), resolved_base.size()),
           StringPiece(resolved_base));
  return GoogleString(spec.data() + resolved_base.size(),
                      spec.size() - resolved_base.size());
}

}  // namespace net_instaweb
