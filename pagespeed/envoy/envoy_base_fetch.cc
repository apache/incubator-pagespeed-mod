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

#include "pagespeed/envoy/envoy_base_fetch.h"

#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/envoy/http_filter.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

EnvoyBaseFetch::EnvoyBaseFetch(StringPiece url,
                               EnvoyServerContext* server_context,
                               const RequestContextPtr& request_ctx,
                               PreserveCachingHeaders preserve_caching_headers,
                               const RewriteOptions* options,
                               Envoy::Http::HttpPageSpeedDecoderFilter* decoder)
    : AsyncFetch(request_ctx),
      url_(url.data(), url.size()),
      server_context_(server_context),
      options_(options),
      preserve_caching_headers_(preserve_caching_headers),
      decoder_(decoder) {}

bool EnvoyBaseFetch::HandleWrite(const StringPiece& sp, MessageHandler*) {
  buffer_.append(sp.data(), sp.size());
  return true;
}

void EnvoyBaseFetch::HandleHeadersComplete() {
  int status_code = response_headers()->status_code();
  bool continue_decoding = false;

  if (status_code == CacheUrlAsyncFetcher::kNotInCacheStatus) {
    decoder_->prepareForIproRecording();
    continue_decoding = true;
  } else {
    have_ipro_response_ = !(status_code < 0 || status_code >= 400);
    continue_decoding = !have_ipro_response_;
  }

  if (continue_decoding) {
    decoder_->decoderCallbacks()->dispatcher().post(
        [this]() { decoder_->decoderCallbacks()->continueDecoding(); });
  }
}

bool EnvoyBaseFetch::HandleFlush(MessageHandler*) { return true; }

int EnvoyBaseFetch::DecrementRefCount() {
  return DecrefAndDeleteIfUnreferenced();
}

int EnvoyBaseFetch::IncrementRefCount() {
  return __sync_add_and_fetch(&references_, 1);
}

int EnvoyBaseFetch::DecrefAndDeleteIfUnreferenced() {
  // Creates a full memory barrier.
  int r = __sync_add_and_fetch(&references_, -1);
  if (r == 0) {
    delete this;
  }
  return r;
}

void EnvoyBaseFetch::HandleDone(bool success) {
  if (have_ipro_response_) {
    if (!success) {
      decoder_->decoderCallbacks()->dispatcher().post(
          [this]() { decoder_->decoderCallbacks()->continueDecoding(); });
    } else {
      decoder_->decoderCallbacks()->dispatcher().post(
          [this]() { decoder_->sendReply(response_headers(), buffer_); });
    }
  }

  DecrefAndDeleteIfUnreferenced();
}

bool EnvoyBaseFetch::IsCachedResultValid(const ResponseHeaders& headers) {
  return OptionsAwareHTTPCacheCallback::IsCacheValid(
      url_, *options_, request_context(), headers);
}

}  // namespace net_instaweb
