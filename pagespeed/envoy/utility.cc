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

#include "utility.h"

#include "exception.h"

#include "external/envoy/source/common/http/utility.h"
#include "external/envoy/source/common/network/utility.h"

#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace net_instaweb {

std::map<std::string, uint64_t>
Utility::mapCountersFromStore(const Envoy::Stats::Store& store,
                              const StoreCounterFilter& filter) const {
  std::map<std::string, uint64_t> results;

  for (const auto& stat : store.counters()) {
    if (filter(stat->name(), stat->value())) {
      // We strip off the "cluster." prefix. Any stats that do not start with
      // "client." after that will be omitted.
      // TODO(oschaaf): we can expose those after amending some tests to expect them.
      std::string stripped_name = std::string(absl::StripPrefix(stat->name(), "cluster."));
      if (!absl::StartsWith(stripped_name, "client.") ||
          stripped_name == "client.membership_change") {
        continue;
      }
      results[stripped_name] = stat->value();
    }
  }

  return results;
}

size_t Utility::findPortSeparator(absl::string_view hostname) {
  if (hostname.size() > 0 && hostname[0] == '[') {
    return hostname.find(":", hostname.find(']'));
  }
  return hostname.rfind(":");
}

Envoy::Network::DnsLookupFamily
Utility::translateFamilyOptionString(AddressFamilyOption value) {
  switch (value) {
  case V4:
    return Envoy::Network::DnsLookupFamily::V4Only;
  case V6:
    return Envoy::Network::DnsLookupFamily::V6Only;
  case AUTO:
    return Envoy::Network::DnsLookupFamily::Auto;
  default:
    NOT_REACHED_GCOVR_EXCL_LINE;
  }
}

void Utility::parseCommand(TCLAP::CmdLine& cmd, const int argc, const char* const* argv) {
  cmd.setExceptionHandling(false);
  try {
    cmd.parse(argc, argv);
  } catch (TCLAP::ArgException& e) {
    try {
      cmd.getOutput()->failure(cmd, e);
    } catch (const TCLAP::ExitException&) {
      // failure() has already written an informative message to stderr, so all that's left to do
      // is throw our own exception with the original message.
      throw Client::MalformedArgvException(e.what());
    }
  } catch (const TCLAP::ExitException& e) {
    // parse() throws an ExitException with status 0 after printing the output for --help and
    // --version.
    throw Client::NoServingException();
  }
}

} // namespace net_instaweb