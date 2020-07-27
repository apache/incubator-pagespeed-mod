#pragma once

#include "absl/strings/numbers.h"
#include "envoy/http/header_map.h"
#include "pagespeed/kernel/http/headers.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

class HeaderUtils {
 public:
  static std::unique_ptr<RequestHeaders> toPageSpeedRequestHeaders(
      Envoy::Http::HeaderMap& headers) {
    std::unique_ptr<RequestHeaders> request_headers =
        std::make_unique<RequestHeaders>();
    auto callback = [&request_headers](const Envoy::Http::HeaderEntry& entry)
        -> Envoy::Http::HeaderMap::Iterate {
      request_headers->Add(entry.key().getStringView(),
                           entry.value().getStringView());
      return Envoy::Http::HeaderMap::Iterate::Continue;
    };
    headers.iterate(callback);
    return request_headers;
  }

  static std::unique_ptr<ResponseHeaders> toPageSpeedResponseHeaders(
      Envoy::Http::HeaderMap& headers) {
    std::unique_ptr<ResponseHeaders> response_headers =
        std::make_unique<ResponseHeaders>();
    // response_headers->set_major_version(r->http_version / 1000);
    // response_headers->set_minor_version(r->http_version % 1000);
    headers.iterate([&response_headers](const Envoy::Http::HeaderEntry& entry)
                        -> Envoy::Http::HeaderMap::Iterate {
      auto key = entry.key().getStringView();
      auto value = entry.value().getStringView();

      if (key == ":status") {
        int status_code;
        if (absl::SimpleAtoi(value, &status_code)) {
          // XXX(oschaaf): safety
          auto code = static_cast<net_instaweb::HttpStatus::Code>(status_code);
          response_headers->set_status_code(code);
          response_headers->set_reason_phrase(
              net_instaweb::HttpStatus::GetReasonPhrase(code));
        } else {
          // XXX(oschaaf)
        }
      } else {
        response_headers->Add(entry.key().getStringView(), value);
      }
      return Envoy::Http::HeaderMap::Iterate::Continue;
    });
    response_headers->ComputeCaching();

    return response_headers;
  }
};

}  // namespace net_instaweb