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

#include "pagespeed/envoy/envoy_process_context.h"

#include <memory>
#include <vector>

#include "pagespeed/automatic/proxy_fetch.h"
#include "pagespeed/envoy/envoy_message_handler.h"
#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"
#include "pagespeed/envoy/envoy_rewrite_options.h"
#include "pagespeed/envoy/envoy_server_context.h"
#include "pagespeed/envoy/envoy_thread_system.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"

namespace net_instaweb {
EnvoyProcessContext::EnvoyProcessContext() : ProcessContext() {
  SystemRewriteDriverFactory::InitApr();
  EnvoyRewriteOptions::Initialize();
  EnvoyRewriteDriverFactory::Initialize();
  // net_instaweb::log_message_handler::Install();

  EnvoyThreadSystem* ts = new EnvoyThreadSystem();
  message_handler_ = std::make_unique<GoogleMessageHandler>();
  driver_factory_ = std::make_unique<EnvoyRewriteDriverFactory>(
      *this, ts, "" /*hostname, not used*/, -1 /*port, not used*/);
  driver_factory_->Init();
  server_context_ = driver_factory()->MakeEnvoyServerContext("", -1);

  EnvoyRewriteOptions* root_options_ =
      dynamic_cast<EnvoyRewriteOptions*>(driver_factory_->default_options());
  EnvoyRewriteOptions* server_options = root_options_->Clone();
  EnvoyRewriteOptions* options =
      new EnvoyRewriteOptions(driver_factory_->thread_system());
  server_options->Merge(*options);
  delete options;

  server_context_->global_options()->Merge(*server_options);
  delete server_options;

  message_handler_->Message(
      kInfo, "Process context constructed:\r\n %s",
      driver_factory_->default_options()->OptionsToString().c_str());
  message_handler_->Message(
      kInfo, "Server context global options:\r\n %s",
      server_context_->global_options()->OptionsToString().c_str());

  std::vector<SystemServerContext*> server_contexts;
  server_contexts.push_back(server_context_);

  // Statistics* statistics =
  //    driver_factory_->MakeGlobalSharedMemStatistics(*(SystemRewriteOptions*)server_context_->global_options());
  GoogleString error_message;
  int error_index = -1;
  Statistics* global_statistics = nullptr;
  driver_factory_.get()->PostConfig(server_contexts, &error_message,
                                    &error_index, &global_statistics);
  if (error_index != -1) {
    server_contexts[error_index]->message_handler()->Message(
        kError, "pagespeed is enabled. %s", error_message.c_str());
    CHECK(false);
  }

  EnvoyRewriteDriverFactory::InitStats(global_statistics);

  driver_factory()->RootInit();
  driver_factory()->ChildInit();

  proxy_fetch_factory_ = std::make_unique<ProxyFetchFactory>(server_context_);
}

}  // namespace net_instaweb