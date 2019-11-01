#include "external/envoy/source/common/config/remote_data_fetcher.h"

#include "external/envoy/source/common/event/real_time_system.h"
#include "external/envoy/source/common/protobuf/message_validator_impl.h"
#include "external/envoy/source/common/stats/allocator_impl.h"
#include "external/envoy/source/common/stats/thread_local_store.h"
#include "external/envoy/source/common/thread_local/thread_local_impl.h"
#include "external/envoy/source/common/upstream/cluster_manager_impl.h"
#include "external/envoy/source/exe/platform_impl.h"
#include "external/envoy/source/exe/process_wide.h"
#include "external/envoy/source/extensions/transport_sockets/tls/context_manager_impl.h"
#include "external/envoy/source/server/config_validation/admin.h"

#include "external/envoy/source/common/runtime/runtime_impl.h"

namespace net_instaweb {
class EnvoyClusterManager {
public:

  EnvoyClusterManager();
  void initClusterManager();
  Envoy::Upstream::ClusterManagerPtr& getClusterManager() { return cluster_manager_;}
  Envoy::Event::DispatcherPtr& getDispatcher() { return dispatcher_;}


private:
  Envoy::Server::ValidationAdmin admin_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;

  Envoy::Event::RealTimeSystem time_system_;
  Envoy::PlatformImpl platform_impl_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> runtime_singleton_;
  Envoy::Thread::MutexBasicLockable log_lock;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_ {};
  Envoy::Event::DispatcherPtr dispatcher_;
  Envoy::ProcessWide process_wide_;
  std::unique_ptr<Envoy::Extensions::TransportSockets::Tls::ContextManagerImpl> ssl_context_manager_;
  //std::vector<Envoy::Upstream::ClusterManagerPtr> clusters_;
};

} // namespace net_instaweb