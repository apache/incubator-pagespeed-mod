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
   * @param response remote data
   */
  virtual void onSuccess(Envoy::Http::ResponseMessagePtr& response) PURE;

  /**
   * This function is called when error happens during fetching data.
   * @param reason failure reason.
   */
  virtual void onFailure(FailureReason reason) PURE;
};

/**
 * Remote data fetcher.
 */
class PagespeedRemoteDataFetcher
    : public Envoy::Logger::Loggable<Envoy::Logger::Id::config>,
      public Envoy::Http::AsyncClient::Callbacks {
 public:
  PagespeedRemoteDataFetcher(Envoy::Upstream::ClusterManager& cm,
                             const ::envoy::config::core::v3::HttpUri& uri,
                             PagespeedRemoteDataFetcherCallback& callback);

  ~PagespeedRemoteDataFetcher() override;

  // Http::AsyncClient::Callbacks
  void onSuccess(const Envoy::Http::AsyncClient::Request&,
                 Envoy::Http::ResponseMessagePtr&& response) override;
  void onFailure(const Envoy::Http::AsyncClient::Request&,
                 Envoy::Http::AsyncClient::FailureReason reason) override;
  void onBeforeFinalizeUpstreamSpan(
      Envoy::Tracing::Span& span,
      const Envoy::Http::ResponseHeaderMap* response_headers) override{};

  /**
   * Fetch data from remote.
   */
  void fetch();

  /**
   * Cancel the fetch.
   */
  void cancel();

 private:
  Envoy::Upstream::ClusterManager& cm_;
  const envoy::config::core::v3::HttpUri& uri_;
  PagespeedRemoteDataFetcherCallback& callback_;

  Envoy::Http::AsyncClient::Request* request_{};
};

using pagespeed_remote_data_fetch_ptr =
    std::unique_ptr<PagespeedRemoteDataFetcher>;

}  // namespace net_instaweb
