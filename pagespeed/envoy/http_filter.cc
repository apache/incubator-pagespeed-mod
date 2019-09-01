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
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/query_params.h"
#include "pagespeed/kernel/html/html_keywords.h"
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

HttpPageSpeedDecoderFilter::HttpPageSpeedDecoderFilter(HttpPageSpeedDecoderFilterConfigSharedPtr config)
    : config_(config) {}

HttpPageSpeedDecoderFilter::~HttpPageSpeedDecoderFilter() {}

void HttpPageSpeedDecoderFilter::onDestroy() {}

const LowerCaseString HttpPageSpeedDecoderFilter::headerKey() const {
  return LowerCaseString(config_->key());
}

const std::string HttpPageSpeedDecoderFilter::headerValue() const {
  return config_->val();
}

FilterHeadersStatus HttpPageSpeedDecoderFilter::decodeHeaders(HeaderMap& headers, bool) {
  std::cerr << "@@@@@ yeah " << std::endl;
  headers.dumpState(std::cerr, 2);
  // add a header
  headers.addCopy(headerKey(), headerValue());
  headers.addCopy(LowerCaseString("x-page-speed"), "yeah");

  net_instaweb::RequestHeaders request_headers;

  return FilterHeadersStatus::Continue;
}

FilterDataStatus HttpPageSpeedDecoderFilter::decodeData(Buffer::Instance&, bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus HttpPageSpeedDecoderFilter::decodeTrailers(HeaderMap&) {
  return FilterTrailersStatus::Continue;
}

void HttpPageSpeedDecoderFilter::setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

} // namespace Http
} // namespace Envoy
