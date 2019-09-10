#include <string>

#include "http_filter.h"

#include "envoy/registry/registry.h"

#include "common/config/json_utility.h"

#include "net/instaweb/rewriter/public/process_context.h"
#include "pagespeed/automatic/proxy_fetch.h"
#include "pagespeed/envoy/envoy_process_context.h"
#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"
#include "pagespeed/envoy/envoy_rewrite_options.h"
#include "pagespeed/envoy/envoy_server_context.h"
#include "pagespeed/envoy/http_filter.pb.h"
#include "pagespeed/envoy/http_filter.pb.validate.h"
#include "pagespeed/system/system_thread_system.h"

using namespace net_instaweb;

// XXX(oschaaf): use in-proc shared mem?
// #define PAGESPEED_SUPPORT_POSIX_SHARED_MEM

namespace Envoy {
namespace Server {
namespace Configuration {

// XXX(oschaaf): fix process context construction
static std::shared_ptr<EnvoyProcessContext> process_context = nullptr;

EnvoyProcessContext* getProcessContext() {
  if (process_context == nullptr) {
    process_context = std::make_shared<EnvoyProcessContext>();
  }
  return process_context.get();
}

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
    return createFilter(Envoy::MessageUtil::downcastAndValidate<const pagespeed::Decoder&>(
                            proto_config, context.messageValidationVisitor()),
                        context);
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
      auto filter = new Http::HttpPageSpeedDecoderFilter(config, getProcessContext()->server_context());
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
  }

  void translateHttpPageSpeedDecoderFilter(const Json::Object& json_config,
                                           pagespeed::Decoder& proto_config) {

    // normally we want to validate the json_config againts a defined json-schema here.
    JSON_UTIL_SET_STRING(json_config, proto_config, key);
    JSON_UTIL_SET_STRING(json_config, proto_config, val);
  }
};

/**
 * Static registration for this PageSpeed filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpPageSpeedDecoderFilterConfig, NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
