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

#include <pthread.h>

#include "pagespeed/envoy/envoy_server_context.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/headers.h"

namespace net_instaweb {

enum EnvoyBaseFetchType {
  kIproLookup,
  kHtmlTransform,
  kPageSpeedResource,
  kAdminPage,
  kPageSpeedProxy
};

enum PreserveCachingHeaders {
  kPreserveAllCachingHeaders,  // Cache-Control, ETag, Last-Modified, etc
  kPreserveOnlyCacheControl,   // Only Cache-Control.
  kDontPreserveHeaders,
};


class EnvoyBaseFetch : public AsyncFetch {
 public:
  EnvoyBaseFetch(StringPiece url,
               EnvoyServerContext* server_context,
               const RequestContextPtr& request_ctx,
               PreserveCachingHeaders preserve_caching_headers,
               EnvoyBaseFetchType base_fetch_type,
               const RewriteOptions* options);
  virtual ~EnvoyBaseFetch();

  // Called by Envoy to decrement the refcount.
  int DecrementRefCount();

  // Called by pagespeed to increment the refcount.
  int IncrementRefCount();

  // Detach() is called when the Envoy side releases this base fetch. It
  // sets detached_ to true and decrements the refcount. We need to know
  // this to be able to handle events which Envoy request context has been
  // released while the event was in-flight.
  void Detach() { detached_ = true; DecrementRefCount(); }

  bool detached() { return detached_; }

  EnvoyBaseFetchType base_fetch_type() { return base_fetch_type_; }

  bool IsCachedResultValid(const ResponseHeaders& headers) override;

 private:
  virtual bool HandleWrite(const StringPiece& sp, MessageHandler* handler);
  virtual bool HandleFlush(MessageHandler* handler);
  virtual void HandleHeadersComplete();
  virtual void HandleDone(bool success);

  void Lock();
  void Unlock();

  // Called by Done() and Release().  Decrements our reference count, and if
  // it's zero we delete ourself.
  int DecrefAndDeleteIfUnreferenced();

  // Live count of EnvoyBaseFetch instances that are currently in use.
  static int active_base_fetches;

  GoogleString url_;
  GoogleString buffer_;
  EnvoyServerContext* server_context_;
  const RewriteOptions* options_;
  bool need_flush_;
  bool done_called_;
  bool last_buf_sent_;
  // How many active references there are to this fetch. Starts at two,
  // decremented once when Done() is called and once when Detach() is called.
  // Incremented for each event written by pagespeed for this EnvoyBaseFetch, and
  // decremented on the Envoy side for each event read for it.
  int references_;
  pthread_mutex_t mutex_;
  EnvoyBaseFetchType base_fetch_type_;
  PreserveCachingHeaders preserve_caching_headers_;
  // Set to true just before the Envoy side releases its reference
  bool detached_;
  bool suppress_;

  DISALLOW_COPY_AND_ASSIGN(EnvoyBaseFetch);
};

}  // namespace net_instaweb
