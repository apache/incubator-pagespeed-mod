#include "pagespeed_remote_data_fetcher.h"

namespace net_instaweb {

PagespeedRemoteDataFetcher::PagespeedRemoteDataFetcher(Envoy::Upstream::ClusterManager& cm,
                                     const ::envoy::api::v2::core::HttpUri& uri,
                                     const std::string& content_hash,
                                     PagespeedRemoteDataFetcherCallback& callback)
    : cm_(cm), uri_(uri), content_hash_(content_hash), callback_(callback) {}

PagespeedRemoteDataFetcher::~PagespeedRemoteDataFetcher() { cancel(); }

void PagespeedRemoteDataFetcher::cancel() {
  if (request_) {
    request_->cancel();
    ENVOY_LOG(debug, "fetch remote data [uri = {}]: canceled", uri_.uri());
  }

  request_ = nullptr;
}

void PagespeedRemoteDataFetcher::fetch() {
  Envoy::Http::MessagePtr message = Envoy::Http::Utility::prepareHeaders(uri_);
  message->headers().insertMethod().value().setReference(Envoy::Http::Headers::get().MethodValues.Get);
  ENVOY_LOG(debug, "fetch remote data from [uri = {}]: start", uri_.uri());
  request_ = cm_.httpAsyncClientForCluster(uri_.cluster())
                 .send(std::move(message), *this,
                       Envoy::Http::AsyncClient::RequestOptions().setTimeout(std::chrono::milliseconds(
                           Envoy::DurationUtil::durationToMilliseconds(uri_.timeout()))));
}

void PagespeedRemoteDataFetcher::onSuccess(Envoy::Http::MessagePtr&& response) {
  callback_.onSuccess(response->body()->toString());
  request_ = nullptr;
}

void PagespeedRemoteDataFetcher::onFailure(Envoy::Http::AsyncClient::FailureReason reason) {
  // ENVOY_LOG(debug, "fetch remote data [uri = {}]: network error {}", uri_.uri(), enumToInt(reason));
  request_ = nullptr;
  callback_.onFailure(FailureReason::Network);
}

} // namespace net_instaweb
