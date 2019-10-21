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

  class EnvoyRemoteDataFetcher : public Envoy::Config::DataFetcher::RemoteDataFetcher {
  public:
    EnvoyRemoteDataFetcher(Envoy::Upstream::ClusterManager& cm,
                           const ::envoy::api::v2::core::HttpUri& uri,
                           const std::string& content_hash,
                           Envoy::Config::DataFetcher::RemoteDataFetcherCallback& callback)
        : Envoy::Config::DataFetcher::RemoteDataFetcher(cm, uri, content_hash, callback) {}

    void onSuccess(Envoy::Http::MessagePtr&& response) override;
    void onFailure(Envoy::Http::AsyncClient::FailureReason reason) override;
  };

  class EnvoyRemoteDataCallback : public Envoy::Config::DataFetcher::RemoteDataFetcherCallback {
  public:
    // Config::DataFetcher::RemoteDataFetcherCallback
    void onSuccess(const std::string& data) override {}

    // Config::DataFetcher::RemoteDataFetcherCallback
    void onFailure(Envoy::Config::DataFetcher::FailureReason failure) override {}
  };

  void initClusterManager();
  Envoy::Upstream::ClusterManagerPtr& getClusterManager() { return cluster_manager_;}

private:
  Envoy::Server::ValidationAdmin admin_;
  Envoy::Stats::SymbolTableImpl symbol_table_;
  Envoy::Runtime::RandomGeneratorImpl generator_;
  Envoy::Server::ConfigTrackerImpl config_tracker_;
  Envoy::Thread::MutexBasicLockable access_log_lock_;
  std::unique_ptr<EnvoyRemoteDataFetcher> EnvoyRemoteDataFetcherPtr;

  Envoy::Event::RealTimeSystem time_system_;
  Envoy::PlatformImpl platform_impl_;
  Envoy::ThreadLocal::InstanceImpl tls_;
  std::unique_ptr<Envoy::Runtime::ScopedLoaderSingleton> runtime_singleton_;
  Envoy::Thread::MutexBasicLockable log_lock;
  Envoy::Upstream::ClusterManagerPtr cluster_manager_ {};
  //std::vector<Envoy::Upstream::ClusterManagerPtr> clusters_;
};

} // namespace net_instaweb