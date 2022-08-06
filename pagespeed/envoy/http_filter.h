#pragma once

#include <string>

#include "envoy/server/filter_config.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "pagespeed/envoy/envoy_base_fetch.h"
#include "pagespeed/envoy/envoy_server_context.h"
#include "pagespeed/envoy/header_utils.h"
#include "pagespeed/envoy/http_filter.pb.h"
#include "pagespeed/system/in_place_resource_recorder.h"
#include "pagespeed/system/system_request_context.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace Envoy {
namespace Http {

class HttpPageSpeedDecoderFilterConfig {
 public:
  HttpPageSpeedDecoderFilterConfig(const pagespeed::Decoder& proto_config);

  const std::string& key() const { return key_; }
  const std::string& val() const { return val_; }

 private:
  const std::string key_;
  const std::string val_;
};

typedef std::shared_ptr<HttpPageSpeedDecoderFilterConfig>
    HttpPageSpeedDecoderFilterConfigSharedPtr;

class HttpPageSpeedDecoderFilter : public StreamFilter {
 public:
  HttpPageSpeedDecoderFilter(HttpPageSpeedDecoderFilterConfigSharedPtr,
                             net_instaweb::EnvoyServerContext*);
  ~HttpPageSpeedDecoderFilter() override;

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(RequestHeaderMap&, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks&) override;

  // Http::StreamEncoderFilter
  FilterHeadersStatus encode1xxHeaders(
      ResponseHeaderMap& headers) override {
    return FilterHeadersStatus::Continue;
  };

  FilterHeadersStatus encodeHeaders(ResponseHeaderMap& headers,
                                    bool end_stream) override;

  FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;

  FilterTrailersStatus encodeTrailers(ResponseTrailerMap& trailers) override {
    return FilterTrailersStatus::Continue;
  };
  FilterMetadataStatus encodeMetadata(MetadataMap& metadata_map) override {
    return FilterMetadataStatus::Continue;
  };

  void setEncoderFilterCallbacks(
      StreamEncoderFilterCallbacks& callbacks) override {
    encoder_callbacks_ = &callbacks;
  };
  void encodeComplete() override {}

  // HttpPageSpeedDecoderFilter
  void prepareForIproRecording();
  void sendReply(net_instaweb::ResponseHeaders* response_headers,
                 std::string body);

  StreamDecoderFilterCallbacks* decoderCallbacks() {
    return decoder_callbacks_;
  };
  StreamEncoderFilterCallbacks* encoderCallbacks() {
    return encoder_callbacks_;
  };

 private:
  const HttpPageSpeedDecoderFilterConfigSharedPtr config_;
  net_instaweb::EnvoyServerContext* server_context_{nullptr};
  StreamDecoderFilterCallbacks* decoder_callbacks_;
  StreamEncoderFilterCallbacks* encoder_callbacks_;

  const LowerCaseString headerKey() const;
  const std::string headerValue() const;
  net_instaweb::EnvoyBaseFetch* base_fetch_{nullptr};
  net_instaweb::RewriteOptions* options_{nullptr};
  net_instaweb::RewriteDriver* rewrite_driver_{nullptr};
  net_instaweb::InPlaceResourceRecorder* recorder_{nullptr};
  net_instaweb::GoogleMessageHandler message_handler_;
  std::unique_ptr<net_instaweb::ResponseHeaders> response_headers_;
  std::unique_ptr<net_instaweb::GoogleUrl> pristine_url_;
};

}  // namespace Http
}  // namespace Envoy
