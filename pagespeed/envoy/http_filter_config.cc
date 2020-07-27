#include <string>

#include "envoy/registry/registry.h"
#include "http_filter.h"
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

EnvoyProcessContext& getProcessContext() {
  static EnvoyProcessContext* process_context = new EnvoyProcessContext();
  return *process_context;
}

class HttpPageSpeedDecoderFilterConfig : public NamedHttpFilterConfigFactory {
 public:
  Http::FilterFactoryCb createFilterFactoryFromProto(
      const Protobuf::Message& proto_config, const std::string&,
      FactoryContext& context) override {
    return createFilter(
        Envoy::MessageUtil::downcastAndValidate<const pagespeed::Decoder&>(
            proto_config, context.messageValidationVisitor()),
        context);
  }

  /**
   *  Return the Protobuf Message that represents your config incase you have
   * config proto
   */
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return ProtobufTypes::MessagePtr{new pagespeed::Decoder()};
  }

  std::string name() const override { return "pagespeed"; };

 private:
  Http::FilterFactoryCb createFilter(const pagespeed::Decoder& proto_config,
                                     FactoryContext&) {
    Http::HttpPageSpeedDecoderFilterConfigSharedPtr config =
        std::make_shared<Http::HttpPageSpeedDecoderFilterConfig>(
            Http::HttpPageSpeedDecoderFilterConfig(proto_config));

    return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
      auto filter = new Http::HttpPageSpeedDecoderFilter(
          config, getProcessContext().server_context());
      callbacks.addStreamFilter(Http::StreamFilterSharedPtr{filter});
    };
  }
};
/**
 * Static registration for this PageSpeed filter. @see RegisterFactory.
 */
static Registry::RegisterFactory<HttpPageSpeedDecoderFilterConfig,
                                 NamedHttpFilterConfigFactory>
    register_;

}  // namespace Configuration
}  // namespace Server
}  // namespace Envoy
