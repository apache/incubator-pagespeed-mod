#pragma once

#include "envoy_cluster_manager.h"


namespace net_instaweb {

/**
 * Failure reason.
 */
enum class FailureReason {
  /* A network error occurred causing remote data retrieval failure. */
  Network,
  /* A failure occurred when trying to verify remote data using sha256. */
  InvalidData,
};

/**
 * Callback used by remote data fetcher.
 */
class PagespeedRemoteDataFetcherCallback {
public:
  virtual ~PagespeedRemoteDataFetcherCallback() = default;

  /**
   * This function will be called when data is fetched successfully from remote.
   * @param data remote data
   */
  virtual void onSuccess(const std::string& data) PURE;

  /**
   * This function is called when error happens during fetching data.
   * @param reason failure reason.
   */
  virtual void onFailure(FailureReason reason) PURE;
};

/**
 * Remote data fetcher.
 */
class PagespeedRemoteDataFetcher : public Envoy::Logger::Loggable<Envoy::Logger::Id::config>,
                          public Envoy::Http::AsyncClient::Callbacks {
public:
  PagespeedRemoteDataFetcher(Envoy::Upstream::ClusterManager& cm, const ::envoy::api::v2::core::HttpUri& uri,
                    const std::string& content_hash, PagespeedRemoteDataFetcherCallback& callback);

  ~PagespeedRemoteDataFetcher() override;

  // Http::AsyncClient::Callbacks
  void onSuccess(Envoy::Http::MessagePtr&& response) override;
  void onFailure(Envoy::Http::AsyncClient::FailureReason reason) override;

  /**
   * Fetch data from remote.
   * @param uri remote URI
   * @param content_hash for verifying data integrity
   * @param callback callback when fetch is done.
   */
  void fetch();

  /**
   * Cancel the fetch.
   */
  void cancel();

private:
  Envoy::Upstream::ClusterManager& cm_;
  const envoy::api::v2::core::HttpUri& uri_;
  const std::string content_hash_;
  PagespeedRemoteDataFetcherCallback& callback_;

  Envoy::Http::AsyncClient::Request* request_{};
};

using PagespeedRemoteDataFetcherPtr = std::unique_ptr<PagespeedRemoteDataFetcher>;

} // namespace net_instaweb
