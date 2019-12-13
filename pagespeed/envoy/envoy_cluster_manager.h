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

#include "exception.h"
#include "external/envoy/source/common/access_log/access_log_manager_impl.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/http/context_impl.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/secret/secret_manager_impl.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/common/upstream/cluster_manager_impl.h"
#include "external/envoy/source/exe/platform_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/source/extensions/transport_sockets/tls/context_manager_impl.h"
#include "external/envoy/source/server/config_validation/admin.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "uri_impl.h"
#include "utility.h"

namespace net_instaweb {

class EnvoyClusterManager {
public:
  EnvoyClusterManager();
  ~EnvoyClusterManager();
  void initClusterManager();
  Envoy::Upstream::ClusterManager& getClusterManager(const GoogleString str_url_);
  Envoy::Event::DispatcherPtr& getDispatcher() { return dispatcher_; }
  const envoy::config::bootstrap::v2::Bootstrap createBootstrapConfiguration(const Uri& uri) const;

private:
  Envoy::ThreadLocal::InstanceImpl tls_;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_{};
  Envoy::Stats::SymbolTableImpl symbol_table_;
  Envoy::Api::ApiPtr api_;
  Envoy::Init::WatcherImpl init_watcher_;
  Envoy::Singleton::ManagerPtr singleton_manager_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Secret::SecretManagerImpl secret_manager_;
  Envoy::ProtobufMessage::ProdValidationContextImpl validation_context_;
  Envoy::AccessLog::AccessLogManagerImpl* access_log_manager_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  Envoy::Server::ValidationAdmin admin_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;
  Envoy::Init::ManagerImpl init_manager_;
  Envoy::Stats::AllocatorImpl stats_allocator_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  Envoy::Http::ContextImpl http_context_;
  Envoy::Event::RealTimeSystem time_system_;
  Envoy::PlatformImpl platform_impl_;
  Envoy::Thread::MutexBasicLockable log_lock;
  Envoy::LocalInfo::LocalInfoPtr local_info_ptr;
  Envoy::ProcessWide process_wide_;

  envoy::config::bootstrap::v2::Bootstrap bootstrap;

  std::unique_ptr<Envoy::Upstream::ProdClusterManagerFactory> cluster_manager_factory_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> runtime_singleton_;
  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;
};

} // namespace net_instaweb