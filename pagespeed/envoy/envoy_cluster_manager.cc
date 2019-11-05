#include "envoy_cluster_manager.h"

#include <sys/file.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

#include "envoy/stats/store.h"

#include "external/envoy/include/envoy/event/dispatcher.h"
#include "external/envoy/source/common/api/api_impl.h"
#include "external/envoy/source/common/config/remote_data_fetcher.h"
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
      local_info_(new Envoy::LocalInfo::LocalInfoImpl(
          {}, Envoy::Network::Utility::getLocalAddress(Envoy::Network::Address::IpVersion::v4),
          "envoyfetcher_service_zone", "envoyfetcher_service_cluster",
          "envoyfetcher_service_node")),
      stats_allocator_(symbol_table_), store_root_(stats_allocator_),
      http_context_(store_root_.symbolTable()) {
  initClusterManager();
}

void configureComponentLogLevels(spdlog::level::level_enum level) {
  // TODO(oschaaf): Add options to tweak the log level of the various log tags
  // that are available.
  Envoy::Logger::Registry::setLogLevel(level);
  Envoy::Logger::Logger* logger_to_change = Envoy::Logger::Registry::logger("main");
  logger_to_change->setLevel(level);
}

void EnvoyClusterManager::initClusterManager() {
  auto logging_context = std::make_unique<Envoy::Logger::Context>(spdlog::level::from_str("trace"),
                                                                  "[%T.%f][%t][%L] %v", log_lock);
  configureComponentLogLevels(spdlog::level::from_str("trace"));

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

  Envoy::MessageUtil::loadFromFile(
      "pagespeed/envoy/cluster.yaml",
      bootstrap, Envoy::ProtobufMessage::getStrictValidationVisitor(), *api_);

  cluster_manager_ = cluster_manager_factory_->clusterManagerFromProto(bootstrap);
  cluster_manager_->setInitializedCb([this]() -> void { init_manager_.initialize(init_watcher_); });
}

} // namespace net_instaweb