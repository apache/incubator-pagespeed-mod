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

#pragma once

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/envoy/envoy_server_context.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/headers.h"

namespace Envoy {
namespace Http {
class HttpPageSpeedDecoderFilter;
}
}  // namespace Envoy

namespace net_instaweb {

enum PreserveCachingHeaders {
  kPreserveAllCachingHeaders,  // Cache-Control, ETag, Last-Modified, etc
  kPreserveOnlyCacheControl,   // Only Cache-Control.
  kDontPreserveHeaders,
};

class EnvoyBaseFetch : public AsyncFetch {
 public:
  EnvoyBaseFetch(StringPiece url, EnvoyServerContext* server_context,
                 const RequestContextPtr& request_ctx,
                 PreserveCachingHeaders preserve_caching_headers,
                 const RewriteOptions* options,
                 Envoy::Http::HttpPageSpeedDecoderFilter* decoder);

  // Called by Envoy to decrement the refcount.
  int DecrementRefCount();
  // Called by pagespeed to increment the refcount.
  int IncrementRefCount();
  bool IsCachedResultValid(const ResponseHeaders& headers) override;

 private:
  bool HandleWrite(const StringPiece& sp, MessageHandler* handler) override;
  bool HandleFlush(MessageHandler* handler) override;
  void HandleHeadersComplete() override;
  void HandleDone(bool success) override;

  int DecrefAndDeleteIfUnreferenced();

  GoogleString url_;
  GoogleString buffer_;
  EnvoyServerContext* server_context_{nullptr};
  const RewriteOptions* options_{nullptr};
  uint32_t references_{2};
  PreserveCachingHeaders preserve_caching_headers_;
  // Set to true just before the Envoy side releases its reference
  bool have_ipro_response_{false};
  Envoy::Http::HttpPageSpeedDecoderFilter* decoder_{nullptr};

  DISALLOW_COPY_AND_ASSIGN(EnvoyBaseFetch);
};

}  // namespace net_instaweb
