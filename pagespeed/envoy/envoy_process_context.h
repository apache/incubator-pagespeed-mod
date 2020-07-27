/** @file

    A brief file description

    @section license License

    Licensed to the Apache Software Foundation (ASF) under one
    or more contributor license agreements.  See the NOTICE file
    distributed with this work for additional information
    regarding copyright ownership.  The ASF licenses this file
    to you under the Apache License, Version 2.0 (the
    "License"); you may not use this file except in compliance
    with the License.  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include "net/instaweb/rewriter/public/process_context.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace net_instaweb {

class EnvoyRewriteDriverFactory;
class ProxyFetchFactory;
class EnvoyServerContext;

class EnvoyProcessContext : public ProcessContext {
 public:
  explicit EnvoyProcessContext();
  ~EnvoyProcessContext() override{};

  MessageHandler* message_handler() { return message_handler_.get(); }
  EnvoyRewriteDriverFactory* driver_factory() { return driver_factory_.get(); }
  ProxyFetchFactory* proxy_fetch_factory() {
    return proxy_fetch_factory_.get();
  }
  EnvoyServerContext* server_context() { return server_context_; }

 private:
  std::unique_ptr<GoogleMessageHandler> message_handler_;
  std::unique_ptr<EnvoyRewriteDriverFactory> driver_factory_;
  std::unique_ptr<ProxyFetchFactory> proxy_fetch_factory_;
  EnvoyServerContext* server_context_;
};

}  // namespace net_instaweb
