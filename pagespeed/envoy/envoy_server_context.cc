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

#include "pagespeed/envoy/envoy_server_context.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "pagespeed/envoy/envoy_message_handler.h"
#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"
#include "pagespeed/envoy/envoy_rewrite_options.h"
#include "pagespeed/system/add_headers_fetcher.h"
#include "pagespeed/system/loopback_route_fetcher.h"
#include "pagespeed/system/system_request_context.h"

namespace net_instaweb {

EnvoyServerContext::EnvoyServerContext(EnvoyRewriteDriverFactory* factory,
                                       StringPiece hostname, int port)
    : SystemServerContext(factory, hostname, port) {}

EnvoyRewriteOptions* EnvoyServerContext::config() {
  return EnvoyRewriteOptions::DynamicCast(global_options());
}

SystemRequestContext* EnvoyServerContext::NewRequestContext() {
  SystemRequestContext* ctx = new SystemRequestContext(
      thread_system()->NewMutex(), timer(), "foohost", 80, "127.0.0.1");
  ctx->set_using_http2(false);
  return ctx;
}

GoogleString EnvoyServerContext::FormatOption(StringPiece option_name,
                                              StringPiece args) {
  return StrCat("pagespeed ", option_name, " ", args, ";");
}

}  // namespace net_instaweb
