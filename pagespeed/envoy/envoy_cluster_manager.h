
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

namespace net_instaweb {

class EnvoyClusterManager {
public:
  EnvoyClusterManager();
  void initClusterManager();
  Envoy::Upstream::ClusterManagerPtr& getClusterManager() { return cluster_manager_; }
  Envoy::Event::DispatcherPtr& getDispatcher() { return dispatcher_; }

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