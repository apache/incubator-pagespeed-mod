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

#include "envoy_cluster_manager.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/stats/store.h"
#include "external/envoy/envoy/event/dispatcher.h"
#include "external/envoy/source/common/api/api_impl.h"
#include "external/envoy/source/common/config/utility.h"
#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/init/manager_impl.h"
#include "external/envoy/source/common/local_info/local_info_impl.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/runtime/runtime_impl.h"
#include "external/envoy/source/common/singleton/manager_impl.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/common/upstream/cluster_manager_impl.h"
#include "external/envoy/source/exe/platform_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/source/extensions/transport_sockets/tls/context_manager_impl.h"
#include "external/envoy/source/server/config_validation/admin.h"
#include "external/envoy/source/server/options_impl.h"
#include "external/envoy/source/server/options_impl_platform.h"
#include "external/envoy_api/envoy/config/core/v3/resolver.pb.h"

namespace {
// Implementation of Envoy::Server::Instance used as a placeholder. None of its methods
// should be called because Nighthawk is not a real Envoy that performs xDS config validation.
class NullServerInstance : public Envoy::Server::Instance {
  Envoy::Server::Admin& admin() override { PANIC("not implemented"); }
  Envoy::Api::Api& api() override { PANIC("not implemented"); }
  Envoy::Upstream::ClusterManager& clusterManager() override { PANIC("not implemented"); }
  const Envoy::Upstream::ClusterManager& clusterManager() const override {
    PANIC("not implemented");
  }
  Envoy::Ssl::ContextManager& sslContextManager() override { PANIC("not implemented"); }
  Envoy::Event::Dispatcher& dispatcher() override { PANIC("not implemented"); }
  Envoy::Network::DnsResolverSharedPtr dnsResolver() override { PANIC("not implemented"); }
  void drainListeners() override { PANIC("not implemented"); }
  Envoy::Server::DrainManager& drainManager() override { PANIC("not implemented"); }
  Envoy::AccessLog::AccessLogManager& accessLogManager() override { PANIC("not implemented"); }
  void failHealthcheck(bool) override { PANIC("not implemented"); }
  bool healthCheckFailed() override { PANIC("not implemented"); }
  Envoy::Server::HotRestart& hotRestart() override { PANIC("not implemented"); }
  Envoy::Init::Manager& initManager() override { PANIC("not implemented"); }
  Envoy::Server::ListenerManager& listenerManager() override { PANIC("not implemented"); }
  Envoy::MutexTracer* mutexTracer() override { PANIC("not implemented"); }
  Envoy::Server::OverloadManager& overloadManager() override { PANIC("not implemented"); }
  Envoy::Secret::SecretManager& secretManager() override { PANIC("not implemented"); }
  const Envoy::Server::Options& options() override { PANIC("not implemented"); }
  Envoy::Runtime::Loader& runtime() override { PANIC("not implemented"); }
  Envoy::Server::ServerLifecycleNotifier& lifecycleNotifier() override { PANIC("not implemented"); }
  void shutdown() override { PANIC("not implemented"); }
  bool isShutdown() override { PANIC("not implemented"); }
  void shutdownAdmin() override { PANIC("not implemented"); }
  Envoy::Singleton::Manager& singletonManager() override { PANIC("not implemented"); }
  time_t startTimeCurrentEpoch() override { PANIC("not implemented"); }
  time_t startTimeFirstEpoch() override { PANIC("not implemented"); }
  Envoy::Stats::Store& stats() override { PANIC("not implemented"); }
  Envoy::Grpc::Context& grpcContext() override { PANIC("not implemented"); }
  Envoy::Http::Context& httpContext() override { PANIC("not implemented"); }
  Envoy::Router::Context& routerContext() override { PANIC("not implemented"); }
  Envoy::ProcessContextOptRef processContext() override { PANIC("not implemented"); }
  Envoy::ThreadLocal::Instance& threadLocal() override { PANIC("not implemented"); }
  Envoy::LocalInfo::LocalInfo& localInfo() const override { PANIC("not implemented"); }
  Envoy::TimeSource& timeSource() override { PANIC("not implemented"); }
  void flushStats() override { PANIC("not implemented"); }
  Envoy::ProtobufMessage::ValidationContext& messageValidationContext() override {
    PANIC("not implemented");
  }
  Envoy::Server::Configuration::StatsConfig& statsConfig() override { PANIC("not implemented"); }
  envoy::config::bootstrap::v3::Bootstrap& bootstrap() override { PANIC("not implemented"); }
  Envoy::Server::Configuration::ServerFactoryContext& serverFactoryContext() override {
    PANIC("not implemented");
  }
  Envoy::Server::Configuration::TransportSocketFactoryContext&
  transportSocketFactoryContext() override {
    PANIC("not implemented");
  }
  void setDefaultTracingConfig(const envoy::config::trace::v3::Tracing&) override {
    PANIC("not implemented");
  }
  bool enableReusePortDefault() override { PANIC("not implemented"); }
  void setSinkPredicates(std::unique_ptr<Envoy::Stats::SinkPredicates>&&) override {
    PANIC("not implemented");
  }
};

// Implementation of Envoy::Server::Configuration::ServerFactoryContext used as a placeholder. None
// of its methods should be called because Nighthawk is not a real Envoy that performs xDS config
// validation.
class NullServerFactoryContext : public Envoy::Server::Configuration::ServerFactoryContext {
  const Envoy::Server::Options& options() override { PANIC("not implemented"); };

  Envoy::Event::Dispatcher& mainThreadDispatcher() override { PANIC("not implemented"); };

  Envoy::Api::Api& api() override { PANIC("not implemented"); };

  Envoy::LocalInfo::LocalInfo& localInfo() const override { PANIC("not implemented"); };

  Envoy::Server::Admin& admin() override { PANIC("not implemented"); };

  Envoy::Runtime::Loader& runtime() override { PANIC("not implemented"); };

  Envoy::Singleton::Manager& singletonManager() override { PANIC("not implemented"); };

  Envoy::ProtobufMessage::ValidationVisitor& messageValidationVisitor() override {
    PANIC("not implemented");
  };

  Envoy::Stats::Scope& scope() override { PANIC("not implemented"); };

  Envoy::Stats::Scope& serverScope() override { PANIC("not implemented"); };

  Envoy::ThreadLocal::SlotAllocator& threadLocal() override { PANIC("not implemented"); };

  Envoy::Upstream::ClusterManager& clusterManager() override { PANIC("not implemented"); };

  Envoy::ProtobufMessage::ValidationContext& messageValidationContext() override {
    PANIC("not implemented");
  };

  Envoy::TimeSource& timeSource() override { PANIC("not implemented"); };

  Envoy::AccessLog::AccessLogManager& accessLogManager() override { PANIC("not implemented"); };

  Envoy::Server::ServerLifecycleNotifier& lifecycleNotifier() override {
    PANIC("not implemented");
  };

  Envoy::Init::Manager& initManager() override { PANIC("not implemented"); };

  Envoy::Grpc::Context& grpcContext() override { PANIC("not implemented"); };

  Envoy::Router::Context& routerContext() override { PANIC("not implemented"); };

  Envoy::Server::DrainManager& drainManager() override { PANIC("not implemented"); };

  Envoy::Server::Configuration::StatsConfig& statsConfig() override { PANIC("not implemented"); }

  envoy::config::bootstrap::v3::Bootstrap& bootstrap() override { PANIC("not implemented"); }
};

} // namespace

namespace net_instaweb {

EnvoyClusterManager::EnvoyClusterManager()
    : init_watcher_("envoyfetcher", []() {}),
      secret_manager_(config_tracker_),
      validation_context_(false, false, false),
      admin_(Envoy::Network::Address::InstanceConstSharedPtr()),
      init_manager_("init_manager"),
      stats_allocator_(symbol_table_),
      store_root_(stats_allocator_),
      http_context_(store_root_.symbolTable()),
      grpc_context_(store_root_.symbolTable()),
      router_context_(store_root_.symbolTable()),
      quic_stat_names_(store_root_.symbolTable()),
      server_(std::make_unique<NullServerInstance>()),
      server_factory_context_(std::make_unique<NullServerFactoryContext>())  {
  initClusterManager();
}

EnvoyClusterManager::~EnvoyClusterManager() { CHECK(shutdown_); }

void EnvoyClusterManager::ShutDown() {
  tls_.shutdownGlobalThreading();
  store_root_.shutdownThreading();
  if (cluster_manager_ != nullptr) {
    cluster_manager_->shutdown();
  }
  tls_.shutdownThread();
  shutdown_ = true;
}

void configureComponentLogLevels(spdlog::level::level_enum level) {
  Envoy::Logger::Registry::setLogLevel(level);
  Envoy::Logger::Logger* logger_to_change =
      Envoy::Logger::Registry::logger(logger_str);
  logger_to_change->setLevel(level);
}

void EnvoyClusterManager::initClusterManager() {
  configureComponentLogLevels(spdlog::level::from_str("error"));
  const std::string host_name = "35.196.240.89";
  const std::string scheme = "http";
  auto port = 80;
  bootstrap_ = createBootstrapConfiguration(scheme, host_name, port);

  local_info_ = std::make_unique<Envoy::LocalInfo::LocalInfoImpl>(
      store_root_.symbolTable(), envoy_node_, envoy_node_context_params_,
      Envoy::Network::Utility::getLocalAddress(
          Envoy::Network::Address::IpVersion::v4),
      "envoyfetcher_service_zone", "envoyfetcher_service_cluster",
      "envoyfetcher_service_node");

  api_ = std::make_unique<Envoy::Api::Impl>(
      platform_impl_.threadFactory(), store_root_, time_system_,
      platform_impl_.fileSystem(), generator_, bootstrap_);
  dispatcher_ = api_->allocateDispatcher("pagespeed-fetcher");
  tls_.registerThread(*dispatcher_, true);
  store_root_.initializeThreading(*dispatcher_, tls_);
  access_log_manager_ =
      std::make_unique<Envoy::AccessLog::AccessLogManagerImpl>(
          std::chrono::milliseconds(1000), *api_, *dispatcher_,
          access_log_lock_, store_root_);
  runtime_singleton_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
          *dispatcher_, tls_, {}, *local_info_, store_root_, generator_,
          Envoy::ProtobufMessage::getStrictValidationVisitor(), *api_)});
  singleton_manager_ =
      std::make_unique<Envoy::Singleton::ManagerImpl>(api_->threadFactory());
  ssl_context_manager_ = std::make_unique<
      Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>(
      time_system_);
  const Envoy::OptionsImpl::HotRestartVersionCb hot_restart_version_cb =
      [](bool) { return "hot restart is disabled"; };
  const Envoy::OptionsImpl envoy_options(
      /* args = */ {"process_impl"}, hot_restart_version_cb,
      spdlog::level::info);
  cluster_manager_factory_ =
      std::make_unique<Envoy::Upstream::ProdClusterManagerFactory>(
          *server_factory_context_, admin_, Envoy::Runtime::LoaderSingleton::get(), store_root_, tls_,
          nullptr /* TODO: set up DNS */, *ssl_context_manager_, *dispatcher_, *local_info_, secret_manager_,
          validation_context_, *api_, http_context_, grpc_context_,
          router_context_, *access_log_manager_, *singleton_manager_,
          envoy_options, quic_stat_names_, *server_);
}

Envoy::Upstream::ClusterManager& EnvoyClusterManager::getClusterManager(
    const GoogleString str_url_) {
  cluster_manager_ = cluster_manager_factory_->clusterManagerFromProto(bootstrap_);
  cluster_manager_->setInitializedCb(
      [this]() -> void { init_manager_.initialize(init_watcher_); });
  return *cluster_manager_;
}

const envoy::config::bootstrap::v3::Bootstrap
EnvoyClusterManager::createBootstrapConfiguration(const std::string scheme,
                                                  const std::string host_name,
                                                  const int port) const {
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  cluster->set_name(getClusterName());
  cluster->mutable_connect_timeout()->set_seconds(15);
  cluster->set_type(envoy::config::cluster::v3::Cluster::DiscoveryType::
                        Cluster_DiscoveryType_STATIC);

  auto* load_assignment = cluster->mutable_load_assignment();
  load_assignment->set_cluster_name(cluster->name());
  auto* socket = cluster->mutable_load_assignment()
                     ->add_endpoints()
                     ->add_lb_endpoints()
                     ->mutable_endpoint()
                     ->mutable_address()
                     ->mutable_socket_address();
  socket->set_address(host_name);
  socket->set_port_value(port);
  return bootstrap;
}

}  // namespace net_instaweb