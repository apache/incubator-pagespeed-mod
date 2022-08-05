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

#include "envoy_logger.h"
#include "external/envoy/source/common/access_log/access_log_manager_impl.h"
#include "external/envoy/source/common/common/random_generator.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/grpc/context_impl.h"
#include "external/envoy/source/common/http/context_impl.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/quic/quic_stat_names.h"
#include "external/envoy/source/common/router/context_impl.h"
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

namespace net_instaweb {

// Implementation to create and manage envoy cluster configuration
// Cluster manager gets created from manager factory for every url to be fetched
class EnvoyClusterManager {
 public:
  EnvoyClusterManager();
  ~EnvoyClusterManager();

  /**
   * This function creates envoy cluster manager for url to be fetched
   * @param str_url_ url to be fetched
   * @return Envoy::Upstream::ClusterManager& Cluster manager reference
   */
  Envoy::Upstream::ClusterManager& getClusterManager(
      const GoogleString str_url_);

  /**
   * This function gets envoy dispatcher which is a event dispatching loop
   * @return Envoy::Event::DispatcherPtr& Event dispatcher reference
   */
  Envoy::Event::DispatcherPtr& getDispatcher() { return dispatcher_; }

  /**
   * This function gets envoy cluster name
   * @return std::string clusterName
   */
  const std::string getClusterName() const { return "cluster1"; }

  void ShutDown();

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
  std::unique_ptr<Envoy::AccessLog::AccessLogManager> access_log_manager_;
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::LocalInfo::LocalInfoPtr local_info_;
  Envoy::Server::ValidationAdmin admin_;
  Envoy::Random::RandomGeneratorImpl generator_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;
  Envoy::Init::ManagerImpl init_manager_;
  Envoy::Stats::AllocatorImpl stats_allocator_;
  Envoy::Stats::ThreadLocalStoreImpl store_root_;
  Envoy::Http::ContextImpl http_context_;
  Envoy::Grpc::ContextImpl grpc_context_;
  Envoy::Event::RealTimeSystem time_system_;
  Envoy::PlatformImpl platform_impl_;
  Envoy::ProcessWide process_wide_;

  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  envoy::config::core::v3::Node envoy_node_{};
  const Envoy::Protobuf::RepeatedPtrField<std::string>
      envoy_node_context_params_;
  std::unique_ptr<Envoy::Upstream::ProdClusterManagerFactory>
      cluster_manager_factory_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> runtime_singleton_;
  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>
      ssl_context_manager_;
  bool shutdown_{false};
  Envoy::Router::ContextImpl router_context_;
  Envoy::Quic::QuicStatNames quic_stat_names_;

  void initClusterManager();

  const envoy::config::bootstrap::v3::Bootstrap createBootstrapConfiguration(
      const std::string scheme, const std::string host_name,
      const int port) const;
  envoy::config::bootstrap::v3::Bootstrap bootstrap_;
  // Null server implementation used as a placeholder. Its methods should never get called
  // because we're not a full Envoy server that performs xDS config validation.
  std::unique_ptr<Envoy::Server::Instance> server_;
  // Null server factory context implementation for the same reason as above.
  std::unique_ptr<Envoy::Server::Configuration::ServerFactoryContext> server_factory_context_;   
};

}  // namespace net_instaweb