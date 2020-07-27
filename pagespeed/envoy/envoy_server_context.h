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

// Manage pagespeed state across requests.  Compare to ApacheResourceManager.

#pragma once

#include "pagespeed/envoy/envoy_message_handler.h"
#include "pagespeed/system/system_server_context.h"

namespace net_instaweb {

class EnvoyRewriteDriverFactory;
class EnvoyRewriteOptions;
class SystemRequestContext;

class EnvoyServerContext : public SystemServerContext {
 public:
  EnvoyServerContext(EnvoyRewriteDriverFactory* factory, StringPiece hostname,
                     int port);

  // We don't allow ProxyFetch to fetch HTML via MapProxyDomain. We will call
  // set_trusted_input() on any ProxyFetches we use to transform internal HTML.
  bool ProxiesHtml() const override { return false; }

  // Call only when you need an EnvoyRewriteOptions.  If you don't need
  // Envoy-specific behavior, call global_options() instead which doesn't
  // downcast.
  EnvoyRewriteOptions* config();

  EnvoyRewriteDriverFactory* envoy_rewrite_driver_factory() {
    return envoy_factory_;
  }
  SystemRequestContext* NewRequestContext();

  EnvoyMessageHandler* envoy_message_handler() {
    return dynamic_cast<EnvoyMessageHandler*>(message_handler());
  }

  GoogleString FormatOption(StringPiece option_name, StringPiece args) override;

 private:
  EnvoyRewriteDriverFactory* envoy_factory_;
  DISALLOW_COPY_AND_ASSIGN(EnvoyServerContext);
};

}  // namespace net_instaweb
