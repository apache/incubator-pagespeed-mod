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


#include "net/instaweb/rewriter/public/url_namer.h"

#include "base/logging.h"               // for COMPACT_GOOGLE_LOG_FATAL, etc
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/output_resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_hash.h"
#include "pagespeed/kernel/base/string_util.h"  // for StrCat, etc
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

UrlNamer::UrlNamer()
    : proxy_domain_("") {
}

UrlNamer::~UrlNamer() {
}

// Moved from OutputResource::url()
GoogleString UrlNamer::Encode(const RewriteOptions* rewrite_options,
                              const OutputResource& output_resource,
                              EncodeOption encode_option) const {
  GoogleString encoded_leaf(output_resource.full_name().Encode());
  GoogleString encoded_path;
  if (rewrite_options == NULL) {
    encoded_path = output_resource.resolved_base();
  } else {
    StringPiece hash = output_resource.full_name().hash();
    DCHECK(!hash.empty());
    uint32 int_hash = HashString<CasePreserve, uint32>(hash.data(),
                                                       hash.size());
    const DomainLawyer* domain_lawyer = rewrite_options->domain_lawyer();
    GoogleUrl gurl(output_resource.resolved_base());
    GoogleString domain = StrCat(gurl.Origin(), "/");
    GoogleString sharded_domain;
    if ((encode_option == kSharded) &&
        domain_lawyer->ShardDomain(domain, int_hash, &sharded_domain)) {
      // The Path has a leading "/", and sharded_domain has a trailing "/".
      // So we need to perform some StringPiece substring arithmetic to
      // make them all fit together.  Note that we could have used
      // string's substr method but that would have made another temp
      // copy, which seems like a waste.
      encoded_path = StrCat(sharded_domain, gurl.PathAndLeaf().substr(1));
    } else {
      encoded_path = output_resource.resolved_base();
    }
  }
  return StrCat(encoded_path, encoded_leaf);
}

bool UrlNamer::Decode(const GoogleUrl& request_url,
                      const RewriteOptions* rewrite_options,
                      GoogleString* decoded) const {
  return false;
}

bool UrlNamer::IsAuthorized(const GoogleUrl& request_url,
                            const RewriteOptions& options) const {
  GoogleUrl invalid_request;
  const DomainLawyer* lawyer = options.domain_lawyer();
  return lawyer->IsDomainAuthorized(invalid_request, request_url);
}

}  // namespace net_instaweb
