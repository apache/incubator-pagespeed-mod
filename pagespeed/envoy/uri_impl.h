/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include <string>

#include "envoy/stats/store.h"

#include "exception.h"
#include "uri.h"

#include "external/envoy/source/common/common/logger.h"
#include "external/envoy/source/common/network/dns_impl.h"
#include "external/envoy/source/common/network/utility.h"

#include "absl/strings/string_view.h"

namespace net_instaweb {

class UriImpl : public Uri, public Envoy::Logger::Loggable<Envoy::Logger::Id::main> {
public:
  UriImpl(absl::string_view uri);
  absl::string_view hostAndPort() const override { return host_and_port_; }
  absl::string_view hostWithoutPort() const override { return host_without_port_; }
  absl::string_view path() const override { return path_; }
  uint64_t port() const override { return port_; }
  absl::string_view scheme() const override { return scheme_; }
  Envoy::Network::Address::InstanceConstSharedPtr
  resolve(Envoy::Event::Dispatcher& dispatcher,
          const Envoy::Network::DnsLookupFamily dns_lookup_family) override;
  Envoy::Network::Address::InstanceConstSharedPtr address() const override {
    ASSERT(resolve_attempted_, "resolve() must be called first.");
    return address_;
  }

private:
  bool isValid() const;
  bool performDnsLookup(Envoy::Event::Dispatcher& dispatcher,
                        const Envoy::Network::DnsLookupFamily dns_lookup_family);

  // TODO(oschaaf): username, password, query etc. But we may want to look at
  // pulling in a mature uri parser.
  std::string host_and_port_;
  std::string host_without_port_;
  std::string path_;
  uint64_t port_{};
  std::string scheme_;

  Envoy::Network::Address::InstanceConstSharedPtr address_;
  bool resolve_attempted_{};
};

} // namespace net_instaweb