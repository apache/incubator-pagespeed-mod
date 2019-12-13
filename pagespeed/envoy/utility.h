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

#include "external/envoy/source/common/network/dns_impl.h"

// #include "api/client/options.pb.h"

#include "absl/strings/string_view.h"
#include "tclap/CmdLine.h"

namespace net_instaweb {

using StoreCounterFilter = std::function<bool(absl::string_view, const uint64_t)>;

class Utility {
public:
  /**
   * Gets a map of tracked counter values, keyed by name.
   * @param filter function that returns true iff a counter should be included in the map,
   * based on the named and value it gets passed. The default filter returns all counters.
   * @return std::map<std::string, uint64_t> containing zero or more entries.
   */
  std::map<std::string, uint64_t> mapCountersFromStore(const Envoy::Stats::Store& store,
                                                       const StoreCounterFilter& filter =
                                                           [](absl::string_view, const uint64_t) {
                                                             return true;
                                                           }) const;
  /**
   * Finds the position of the port separator in the host:port fragment.
   *
   * @param hostname valid "host[:port]" string.
   * @return size_t the position of the port separator, or absl::string_view::npos if none was
   * found.
   */
  static size_t findPortSeparator(absl::string_view hostname);

  /**
   * @param family Address family as a string. Allowed values are "v6", "v4", and "auto" (case
   * insensitive). Any other values will throw a NighthawkException.
   * @return Envoy::Network::DnsLookupFamily the equivalent DnsLookupFamily value
   */
  enum AddressFamilyOption {
    AUTO = 0,
    V4 = 1,
    V6 = 2,
  };
  static Envoy::Network::DnsLookupFamily
  translateFamilyOptionString(AddressFamilyOption value);

  /**
   * Executes TCLAP command line parsing
   * @param cmd TCLAP command line specification.
   * @param argc forwarded argc argument of the main entry point.
   * @param argv forwarded argv argument of the main entry point.
   */
  static void parseCommand(TCLAP::CmdLine& cmd, const int argc, const char* const* argv);
};

} // namespace net_instaweb