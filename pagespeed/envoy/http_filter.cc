#include "http_filter.h"

#include <string>

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
  if (rewrite_driver_ != nullptr) {
    rewrite_driver_->Cleanup();
    rewrite_driver_ = nullptr;
  }
  if (recorder_ != nullptr) {
    recorder_->DoneAndSetHeaders(nullptr, false);
    recorder_ = nullptr;
  }
  if (base_fetch_ != nullptr) {
    base_fetch_->DecrementRefCount();
    base_fetch_ = nullptr;
  }
}

void HttpPageSpeedDecoderFilter::onDestroy() {}

const LowerCaseString HttpPageSpeedDecoderFilter::headerKey() const {
  return LowerCaseString(config_->key());
}

const std::string HttpPageSpeedDecoderFilter::headerValue() const {
  return config_->val();
}

// decode = client side request
FilterHeadersStatus HttpPageSpeedDecoderFilter::decodeHeaders(
    RequestHeaderMap& headers, bool end_response) {
  RELEASE_ASSERT(base_fetch_ == nullptr, "Base fetch not null");
  const std::string url =
      absl::StrCat("http://127.0.0.1", headers.Path()->value().getStringView());
  pristine_url_ = std::make_unique<net_instaweb::GoogleUrl>(url);
  net_instaweb::RequestContextPtr request_context(
      server_context_->NewRequestContext());
  auto* options = options_ = server_context_->global_options();
  request_context->set_options(options->ComputeHttpOptions());
  RELEASE_ASSERT(options != nullptr, "server context global options not set!");
  base_fetch_ = new net_instaweb::EnvoyBaseFetch(
      pristine_url_->Spec(), server_context_, request_context,
      net_instaweb::kDontPreserveHeaders, options, this);
  rewrite_driver_ =
      server_context_->NewRewriteDriver(base_fetch_->request_context());
  rewrite_driver_->SetRequestHeaders(*base_fetch_->request_headers());

  headers.iterate([this](const HeaderEntry& entry) -> HeaderMap::Iterate {
    base_fetch_->request_headers()->Add(entry.key().getStringView(),
                                        entry.value().getStringView());
    return HeaderMap::Iterate::Continue;
  });
  rewrite_driver_->FetchInPlaceResource(*pristine_url_, false /* proxy_mode */,
                                        base_fetch_);
  return FilterHeadersStatus::StopIteration;
}

FilterDataStatus HttpPageSpeedDecoderFilter::decodeData(Buffer::Instance&,
                                                        bool) {
  return FilterDataStatus::Continue;
}

FilterTrailersStatus HttpPageSpeedDecoderFilter::decodeTrailers(
    RequestTrailerMap&) {
  return FilterTrailersStatus::Continue;
}

void HttpPageSpeedDecoderFilter::setDecoderFilterCallbacks(
    StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;
}

void HttpPageSpeedDecoderFilter::prepareForIproRecording() {
  server_context_->rewrite_stats()->ipro_not_in_cache()->Add(1);
  server_context_->message_handler()->Message(
      net_instaweb::kInfo,
      "Could not rewrite resource in-place "
      "because URL is not in cache: %s",
      pristine_url_->spec_c_str());
  const net_instaweb::SystemRewriteOptions* options =
      net_instaweb::SystemRewriteOptions::DynamicCast(
          rewrite_driver_->options());
  net_instaweb::RequestContextPtr request_context(
      server_context_->NewRequestContext());
  request_context->set_options(options->ComputeHttpOptions());

  // This URL was not found in cache (neither the input resource nor
  // a ResourceNotCacheable entry) so we need to get it into cache
  // (or at least a note that it cannot be cached stored there).
  // We do that using an Apache output filter.
  recorder_ = new net_instaweb::InPlaceResourceRecorder(
      request_context, pristine_url_->spec_c_str(),
      rewrite_driver_->CacheFragment(),
      base_fetch_->request_headers()->GetProperties(),
      options->ipro_max_response_bytes(),
      options->ipro_max_concurrent_recordings(), server_context_->http_cache(),
      server_context_->statistics(), &message_handler_);
}

void HttpPageSpeedDecoderFilter::sendReply(
    net_instaweb::ResponseHeaders* response_headers, std::string body) {
  CHECK(response_headers != nullptr);

  std::function<void(Http::HeaderMap&)> modify_headers =
      [response_headers](Http::HeaderMap& envoy_headers) {
        for (uint32_t i = 0, n = response_headers->NumAttributes(); i < n;
             ++i) {
          const GoogleString& name = response_headers->Name(i);
          const GoogleString& value = response_headers->Value(i);
          auto lcase_key = Envoy::Http::LowerCaseString(name);
          envoy_headers.remove(lcase_key);
          envoy_headers.addCopy(lcase_key, value);
        }
      };
  // XXX(oschaaf): cast
  decoder_callbacks_->sendLocalReply(
      static_cast<Envoy::Http::Code>(response_headers->status_code()), body,
      modify_headers, absl::nullopt, "details");
}

FilterHeadersStatus HttpPageSpeedDecoderFilter::encodeHeaders(
    ResponseHeaderMap& headers, bool end_stream) {
  if (end_stream || !recorder_) {
    return FilterHeadersStatus::Continue;
  }

  if (recorder_ != nullptr) {
    response_headers_ =
        net_instaweb::HeaderUtils::toPageSpeedResponseHeaders(headers);
    // std::cerr << response_headers_->ToString() << std::endl;
    recorder_->ConsiderResponseHeaders(
        net_instaweb::InPlaceResourceRecorder::kPreliminaryHeaders,
        response_headers_.get());
  }
  return FilterHeadersStatus::Continue;
};

FilterDataStatus HttpPageSpeedDecoderFilter::encodeData(Buffer::Instance& data,
                                                        bool end_stream) {
  if (recorder_ != nullptr) {
    // XXX(oschaaf): update s-max-age
    // ResponseHeaders::ApplySMaxAge(s_maxage_sec,
    //                            existing_cache_control,
    //                            &updated_cache_control)
    // XXX(oschaaf): can we get a string view?
    recorder_->Write(data.toString(), recorder_->handler());
    if (end_stream) {
      recorder_->DoneAndSetHeaders(response_headers_.get(), true);
      recorder_ = nullptr;
    }
  }

  return FilterDataStatus::Continue;
};

}  // namespace Http
}  // namespace Envoy
