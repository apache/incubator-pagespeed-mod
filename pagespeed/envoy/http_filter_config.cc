#include <string>

#include "http_filter.h"

#include "common/config/json_utility.h"
#include "envoy/registry/registry.h"

#include "pagespeed/envoy/http_filter.pb.h"
#include "pagespeed/envoy/http_filter.pb.validate.h"
#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"

using namespace net_instaweb;

namespace Envoy {
namespace Server {
namespace Configuration {

class HttpPageSpeedDecoderFilterConfig : public NamedHttpFilterConfigFactory {
public:
  Http::FilterFactoryCb createFilterFactory(const Json::Object& json_config, const std::string&,
                                            FactoryContext& context) override {

    pagespeed::Decoder proto_config;
    translateHttpPageSpeedDecoderFilter(json_config, proto_config);

    return createFilter(proto_config, context);
  }

  Http::FilterFactoryCb createFilterFactoryFromProto(const Protobuf::Message& proto_config,
                                                     const std::string&,
                                                     FactoryContext& context) override {

    return createFilter(
        Envoy::MessageUtil::downcastAndValidate<const pagespeed::Decoder&>(proto_config,  context.messageValidationVisitor()), context);
  }

  /**
   *  Return the Protobuf Message that represents your config incase you have config proto
   */
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new pagespeed::Decoder()};
  }

  std::string name() override { return "pagespeed"; }

private:
  Http::FilterFactoryCb createFilter(const pagespeed::Decoder& proto_config, FactoryContext&) {
    Http::HttpPageSpeedDecoderFilterConfigSharedPtr config =
        std::make_shared<Http::HttpPageSpeedDecoderFilterConfig>(
            Http::HttpPageSpeedDecoderFilterConfig(proto_config));

    return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Http::HttpPageSpeedDecoderFilter(config);
      callbacks.addStreamDecoderFilter(Http::StreamDecoderFilterSharedPtr{filter});
    };
  }

  void translateHttpPageSpeedDecoderFilter(const Json::Object& json_config,
                                        pagespeed::Decoder& proto_config) {

    // normally we want to validate the json_config againts a defined json-schema here.
    JSON_UTIL_SET_STRING(json_config, proto_config, key);
    JSON_UTIL_SET_STRING(json_config, proto_config, val);
  }

  std::shared_ptr<EnvoyRewriteDriverFactory> rewrite_driver_factory_;
};

/**
 * Static registration for this PageSpeed filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpPageSpeedDecoderFilterConfig, NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
