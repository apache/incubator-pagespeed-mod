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

#include "external/envoy/include/envoy/event/dispatcher.h"
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
#include "external/envoy/source/extensions/transport_sockets/well_known_names.h"
#include "external/envoy/source/server/config_validation/admin.h"
#include "external/envoy/source/server/options_impl_platform.h"

namespace net_instaweb {

EnvoyClusterManager::EnvoyClusterManager()
    : init_watcher_("envoyfetcher", []() {}), secret_manager_(config_tracker_),
      validation_context_(false, false), init_manager_("init_manager"),
      stats_allocator_(symbol_table_), store_root_(stats_allocator_),
      http_context_(store_root_.symbolTable()) {
  initClusterManager();
}

EnvoyClusterManager::~EnvoyClusterManager() {
  tls_.shutdownGlobalThreading();
  store_root_.shutdownThreading();
  if (cluster_manager_ != nullptr) {
    cluster_manager_->shutdown();
  }
  tls_.shutdownThread();
}

void configureComponentLogLevels(spdlog::level::level_enum level) {
  Envoy::Logger::Registry::setLogLevel(level);
  Envoy::Logger::Logger* logger_to_change = Envoy::Logger::Registry::logger(logger_str);
  logger_to_change->setLevel(level);
}

void EnvoyClusterManager::initClusterManager() {

  configureComponentLogLevels(spdlog::level::from_str("error"));

  local_info_ = std::make_unique<Envoy::LocalInfo::LocalInfoImpl>(
      envoy_node_, Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
      "envoyfetcher_service_zone", "envoyfetcher_service_cluster", "envoyfetcher_service_node");

  api_ = std::make_unique<Envoy::Api::Impl>(platform_impl_.threadFactory(), store_root_,
                                            time_system_, platform_impl_.fileSystem());
  dispatcher_ = api_->allocateDispatcher();

  tls_.registerThread(*dispatcher_, true);
  store_root_.initializeThreading(*dispatcher_, tls_);

  access_log_manager_ = new Envoy::AccessLog::AccessLogManagerImpl(
      std::chrono::milliseconds(1000), *api_, *dispatcher_, access_log_lock_, store_root_);

  runtime_singleton_ = std::make_unique<Envoy::Runtime::ScopedLoaderSingleton>(
      Envoy::Runtime::LoaderPtr{new Envoy::Runtime::LoaderImpl(
          *dispatcher_, tls_, {}, *local_info_, init_manager_, store_root_, generator_,
          Envoy::ProtobufMessage::getStrictValidationVisitor(), *api_)});

  singleton_manager_ = std::make_unique<Envoy::Singleton::ManagerImpl>(api_->threadFactory());
  Envoy::Runtime::LoaderSingleton::get();

  ssl_context_manager_ =
      std::make_unique<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl>(time_system_);

  cluster_manager_factory_ = std::make_unique<Envoy::Upstream::ProdClusterManagerFactory>(
      admin_, Envoy::Runtime::LoaderSingleton::get(), store_root_, tls_, generator_,
      dispatcher_->createDnsResolver({}), *ssl_context_manager_, *dispatcher_, *local_info_,
      secret_manager_, validation_context_, *api_, http_context_, *access_log_manager_,
      *singleton_manager_);
}

Envoy::Upstream::ClusterManager&
EnvoyClusterManager::getClusterManager(const GoogleString str_url_) {
  const std::string host_name = "127.0.0.1";
  const std::string scheme = "http";
  auto port = 80;

  cluster_manager_ = cluster_manager_factory_->clusterManagerFromProto(
      createBootstrapConfiguration(scheme, host_name, port));
  cluster_manager_->setInitializedCb([this]() -> void { init_manager_.initialize(init_watcher_); });
  return *cluster_manager_;
}

const envoy::config::bootstrap::v2::Bootstrap
EnvoyClusterManager::createBootstrapConfiguration(const std::string scheme, const std::string host_name,
                                                  const int port) const {
  envoy::config::bootstrap::v2::Bootstrap bootstrap;
  auto* cluster = bootstrap.mutable_static_resources()->add_clusters();
  cluster->set_name(getClusterName());
  cluster->mutable_connect_timeout()->set_seconds(15);
  cluster->set_type(envoy::api::v2::Cluster::DiscoveryType::Cluster_DiscoveryType_STATIC);
  auto* host = cluster->add_hosts();
  auto* socket_address = host->mutable_socket_address();

  socket_address->set_address(host_name);
  socket_address->set_port_value(port);

  return bootstrap;
}

} // namespace net_instaweb