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

#include "exception.h"

#include "external/envoy/source/common/network/dns_impl.h"
#include "external/envoy/source/common/network/utility.h"

#include "absl/strings/string_view.h"

namespace net_instaweb {

/**
 * Any exception thrown by Uri shall inherit from UriException.
 */
class UriException : public EnvoyException {
public:
  UriException(const std::string& message) : EnvoyException(message) {}
};

/**
 * Abstract Uri interface.
 */
class Uri {
public:
  virtual ~Uri() = default;

  /**
   * @return absl::string_view containing the "host:port" fragment of the parsed uri. The port
   * will be explicitly set in case it is the default for the protocol. Do not use the returned
   * value after the Uri instance is destructed.
   */
  virtual absl::string_view hostAndPort() const PURE;

  /**
   * @return absl::string_view containing the "host" fragment parsed uri. Do not use the
   * returned value after the Uri instance is destructed.
   */
  virtual absl::string_view hostWithoutPort() const PURE;

  /**
   * @return absl::string_view containing the "/path" fragment of the parsed uri.
   */
  virtual absl::string_view path() const PURE;

  /**
   * @return uint64_t returns the port of the parsed uri.
   */
  virtual uint64_t port() const PURE;

  /**
   * @return absl::string_view returns the scheme of the parsed uri. Do not use the
   * returned value after the Uri instance is destructed.
   */
  virtual absl::string_view scheme() const PURE;

  /**
   * Synchronously resolves the parsed host from the uri to an ip-address.
   * @param dispatcher Dispatcher to use for resolving.
   * @param dns_lookup_family Allows specifying Ipv4, Ipv6, or Auto as the preferred returned
   * address family.
   * @return Envoy::Network::Address::InstanceConstSharedPtr the resolved address.
   */
  virtual Envoy::Network::Address::InstanceConstSharedPtr
  resolve(Envoy::Event::Dispatcher& dispatcher,
          const Envoy::Network::DnsLookupFamily dns_lookup_family) PURE;

  /**
   * @return Envoy::Network::Address::InstanceConstSharedPtr a cached copy of an earlier call to
   * resolve(), which must have been called successfully first.
   */
  virtual Envoy::Network::Address::InstanceConstSharedPtr address() const PURE;
};

using UriPtr = std::unique_ptr<Uri>;

} // namespace net_instaweb