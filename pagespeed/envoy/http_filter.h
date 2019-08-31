#pragma once

#include <string>

#include "envoy/server/filter_config.h"

#include "pagespeed/envoy/http_filter.pb.h"

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

typedef std::shared_ptr<HttpPageSpeedDecoderFilterConfig> HttpPageSpeedDecoderFilterConfigSharedPtr;

class HttpPageSpeedDecoderFilter : public StreamDecoderFilter {
public:
  HttpPageSpeedDecoderFilter(HttpPageSpeedDecoderFilterConfigSharedPtr);
  ~HttpPageSpeedDecoderFilter();

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap&, bool) override;
  FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks&) override;

private:
  const HttpPageSpeedDecoderFilterConfigSharedPtr config_;
  StreamDecoderFilterCallbacks* decoder_callbacks_;

  const LowerCaseString headerKey() const;
  const std::string headerValue() const;
};

} // namespace Http
} // namespace Envoy
