#include <string>

#include "http_filter.h"

#include "envoy/server/filter_config.h"

#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/cache_url_async_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/public/global_constants.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/experiment_matcher.h"
#include "net/instaweb/rewriter/public/experiment_util.h"
#include "net/instaweb/rewriter/public/process_context.h"
#include "net/instaweb/rewriter/public/resource_fetch.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_query.h"
#include "net/instaweb/rewriter/public/rewrite_stats.h"
#include "net/instaweb/rewriter/public/static_asset_manager.h"
#include "net/instaweb/util/public/fallback_property_page.h"
#include "pagespeed/automatic/proxy_fetch.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"
#include "pagespeed/kernel/util/gzip_inflater.h"
#include "pagespeed/kernel/util/statistics_logger.h"
#include "pagespeed/system/in_place_resource_recorder.h"
#include "pagespeed/system/system_caches.h"
#include "pagespeed/system/system_request_context.h"
#include "pagespeed/system/system_rewrite_options.h"
#include "pagespeed/system/system_server_context.h"
#include "pagespeed/system/system_thread_system.h"

namespace Envoy {
namespace Http {

HttpPageSpeedDecoderFilterConfig::HttpPageSpeedDecoderFilterConfig(
    const pagespeed::Decoder& proto_config)
    : key_(proto_config.key()), val_(proto_config.val()) {}

HttpPageSpeedDecoderFilter::HttpPageSpeedDecoderFilter(
    HttpPageSpeedDecoderFilterConfigSharedPtr config,
    net_instaweb::EnvoyServerContext* server_context)
    : config_(config), server_context_(server_context) {}

HttpPageSpeedDecoderFilter::~HttpPageSpeedDecoderFilter() {
  base_fetch_->DecrementRefCount();
  base_fetch_ = nullptr;
}

void HttpPageSpeedDecoderFilter::onDestroy() {}

const LowerCaseString HttpPageSpeedDecoderFilter::headerKey() const {
  return LowerCaseString(config_->key());
}

const std::string HttpPageSpeedDecoderFilter::headerValue() const { return config_->val(); }

FilterHeadersStatus HttpPageSpeedDecoderFilter::decodeHeaders(HeaderMap& headers, bool) {
  RELEASE_ASSERT(base_fetch_ == nullptr, "Base fetch not null");
  net_instaweb::GoogleUrl gurl("http://127.0.0.1/");
  net_instaweb::RequestContextPtr request_context(server_context_->NewRequestContext());
  auto* options = server_context_->global_options();
  request_context->set_options(options->ComputeHttpOptions());
  RELEASE_ASSERT(options != nullptr, "server context global options not set!");
  base_fetch_ = new net_instaweb::EnvoyBaseFetch(
      gurl.Spec(), server_context_, request_context, net_instaweb::kDontPreserveHeaders,
      net_instaweb::EnvoyBaseFetchType::kIproLookup, options, this);
  net_instaweb::RewriteDriver* rewrite_driver =
      server_context_->NewRewriteDriver(base_fetch_->request_context());
  rewrite_driver->SetRequestHeaders(*base_fetch_->request_headers());

  auto callback = [](const HeaderEntry& entry, void* base_fetch) -> HeaderMap::Iterate {
    static_cast<net_instaweb::EnvoyBaseFetch*>(base_fetch)
        ->request_headers()
        ->Add(entry.key().getStringView(), entry.value().getStringView());
    return HeaderMap::Iterate::Continue;
  };
  headers.iterate(callback, base_fetch_);

  rewrite_driver->FetchInPlaceResource(gurl, false /* proxy_mode */, base_fetch_);

  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus HttpPageSpeedDecoderFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus HttpPageSpeedDecoderFilter::decodeTrailers(HeaderMap&) {
  return FilterTrailersStatus::Continue;
}

void HttpPageSpeedDecoderFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Http
} // namespace Envoy
