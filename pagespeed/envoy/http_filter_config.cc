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
    SystemRewriteDriverFactory::InitApr();
    EnvoyRewriteOptions::Initialize();
    EnvoyRewriteDriverFactory::Initialize();
    // net_instaweb::log_message_handler::Install();

    process_context_ = std::make_shared<EnvoyProcessContext>();
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

  net_instaweb::ServerContext* server_context() const { return server_context_.get(); }

private:
  Http::FilterFactoryCb createFilter(const pagespeed::Decoder& proto_config, FactoryContext&) {
    Http::HttpPageSpeedDecoderFilterConfigSharedPtr config =
        std::make_shared<Http::HttpPageSpeedDecoderFilterConfig>(
            Http::HttpPageSpeedDecoderFilterConfig(proto_config));

    return [config, this](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter =
          new Http::HttpPageSpeedDecoderFilter(config, process_context_->server_context());
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
  }

  void translateHttpPageSpeedDecoderFilter(const Json::Object& json_config,
                                           pagespeed::Decoder& proto_config) {

    // normally we want to validate the json_config againts a defined json-schema here.
    JSON_UTIL_SET_STRING(json_config, proto_config, key);
    JSON_UTIL_SET_STRING(json_config, proto_config, val);
  }

  std::shared_ptr<EnvoyProcessContext> process_context_;
  std::shared_ptr<net_instaweb::ProxyFetchFactory> proxy_fetch_factory_;
  std::shared_ptr<EnvoyRewriteDriverFactory> rewrite_driver_factory_;
  std::unique_ptr<EnvoyServerContext> server_context_;
};

/**
 * Static registration for this PageSpeed filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpPageSpeedDecoderFilterConfig, NamedHttpFilterConfigFactory>
    register_;

} // namespace Configuration
} // namespace Server
} // namespace Envoy
