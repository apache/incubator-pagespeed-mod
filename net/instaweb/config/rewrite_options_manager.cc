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


#include "net/instaweb/config/rewrite_options_manager.h"

#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"

namespace net_instaweb {

void RewriteOptionsManager::GetRewriteOptions(
    const GoogleUrl& url,
    const RequestHeaders& headers,
    OptionsCallback* done) {
  done->Run(NULL);
}

void RewriteOptionsManager::PrepareRequest(
    const RewriteOptions* rewrite_options,
    const RequestContextPtr& request_context,
    GoogleString* url,
    RequestHeaders* request_headers,
    BoolCallback* callback) {
  if (rewrite_options == NULL) {
    callback->Run(true);
    return;
  }

  GoogleUrl gurl(*url);
  if (!gurl.IsWebValid()) {
    callback->Run(false);
    return;
  }

  const DomainLawyer* domain_lawyer = rewrite_options->domain_lawyer();
  bool is_proxy = false;
  GoogleString host_header;
  if (domain_lawyer->StripProxySuffix(gurl, url, &host_header)) {
    request_context->AddSessionAuthorizedFetchOrigin(
        StrCat(gurl.Scheme(), "://", host_header));
  } else if (!domain_lawyer->MapOriginUrl(gurl, url, &host_header, &is_proxy)) {
    callback->Run(false);
    return;
  }

  if (!is_proxy) {
    request_headers->Replace(HttpAttributes::kHost, host_header);
  }

  callback->Run(true);
}

}  // namespace net_instaweb
