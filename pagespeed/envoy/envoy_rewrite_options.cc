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

#include "pagespeed/envoy/envoy_rewrite_options.h"

//#include "envoy_pagespeed.h"
#include "envoy_rewrite_driver_factory.h"
#include "net/instaweb/public/version.h"
#include "net/instaweb/rewriter/public/file_load_policy.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/system/system_caches.h"

namespace net_instaweb {

namespace {

const char kStatisticsPath[] = "StatisticsPath";
const char kGlobalStatisticsPath[] = "GlobalStatisticsPath";
const char kConsolePath[] = "ConsolePath";
const char kMessagesPath[] = "MessagesPath";
const char kAdminPath[] = "AdminPath";
const char kGlobalAdminPath[] = "GlobalAdminPath";

// These options are copied from mod_instaweb.cc, where APACHE_CONFIG_OPTIONX
// indicates that they can not be set at the directory/location level. They set
// options in the RewriteDriverFactory, so they're entirely global and do not
// appear in RewriteOptions.  They are not alphabetized on purpose, but rather
// left in the same order as in mod_instaweb.cc in case we end up needing to
// compare.
// TODO(oschaaf): this duplication is a short term solution.
const char* const server_only_options[] = {
    "FetcherTimeoutMs", "FetchProxy", "ForceCaching", "GeneratedFilePrefix",
    "ImgMaxRewritesAtOnce", "InheritVHostConfig", "InstallCrashHandler",
    "MessageBufferSize", "NumRewriteThreads", "NumExpensiveRewriteThreads",
    "StaticAssetPrefix", "TrackOriginalContentLength",
    "UsePerVHostStatistics",  // TODO(anupama): What to do about "No longer
                              // used"
    "BlockingRewriteRefererUrls", "CreateSharedMemoryMetadataCache",
    "LoadFromFile", "LoadFromFileMatch", "LoadFromFileRule",
    "LoadFromFileRuleMatch", "UseNativeFetcher",
    "NativeFetcherMaxKeepaliveRequests"};

// Options that can only be used in the main (http) option scope.
const char* const main_only_options[] = {"UseNativeFetcher",
                                         "NativeFetcherMaxKeepaliveRequests"};

}  // namespace

RewriteOptions::Properties* EnvoyRewriteOptions::envoy_properties_ = nullptr;

EnvoyRewriteOptions::EnvoyRewriteOptions(const StringPiece& description,
                                         ThreadSystem* thread_system)
    : SystemRewriteOptions(description, thread_system) {
  Init();
}

EnvoyRewriteOptions::EnvoyRewriteOptions(ThreadSystem* thread_system)
    : SystemRewriteOptions(thread_system) {
  Init();
}

void EnvoyRewriteOptions::Init() {
  DCHECK(envoy_properties_ != nullptr)
      << "Call EnvoyRewriteOptions::Initialize() before construction";
  InitializeOptions(envoy_properties_);
}

void EnvoyRewriteOptions::AddProperties() {
  // Envoy-specific options.
  add_envoy_option("", &EnvoyRewriteOptions::statistics_path_, "nsp",
                   kStatisticsPath, kServerScope,
                   "Set the statistics path. Ex: /envoy_pagespeed_statistics",
                   false);
  add_envoy_option(
      "", &EnvoyRewriteOptions::global_statistics_path_, "ngsp",
      kGlobalStatisticsPath, kProcessScopeStrict,
      "Set the global statistics path. Ex: /envoy_pagespeed_global_statistics",
      false);
  add_envoy_option("", &EnvoyRewriteOptions::console_path_, "ncp", kConsolePath,
                   kServerScope, "Set the console path. Ex: /pagespeed_console",
                   false);
  add_envoy_option("", &EnvoyRewriteOptions::messages_path_, "nmp",
                   kMessagesPath, kServerScope,
                   "Set the messages path.  Ex: /envoy_pagespeed_message",
                   false);
  add_envoy_option("", &EnvoyRewriteOptions::admin_path_, "nap", kAdminPath,
                   kServerScope, "Set the admin path.  Ex: /pagespeed_admin",
                   false);
  add_envoy_option("", &EnvoyRewriteOptions::global_admin_path_, "ngap",
                   kGlobalAdminPath, kProcessScopeStrict,
                   "Set the global admin path.  Ex: /pagespeed_global_admin",
                   false);

  MergeSubclassProperties(envoy_properties_);

  // Default properties are global but to set them the current API requires
  // a RewriteOptions instance and we're in a static method.
  EnvoyRewriteOptions dummy_config(nullptr);
  dummy_config.set_default_x_header_value(kModPagespeedVersion);
}

void EnvoyRewriteOptions::Initialize() {
  if (Properties::Initialize(&envoy_properties_)) {
    SystemRewriteOptions::Initialize();
    AddProperties();
  }
}

void EnvoyRewriteOptions::Terminate() {
  if (Properties::Terminate(&envoy_properties_)) {
    SystemRewriteOptions::Terminate();
  }
}

bool EnvoyRewriteOptions::IsDirective(StringPiece config_directive,
                                      StringPiece compare_directive) {
  return StringCaseEqual(config_directive, compare_directive);
}

RewriteOptions::OptionScope EnvoyRewriteOptions::GetOptionScope(
    StringPiece option_name) {
  uint32_t i;
  uint32_t size = sizeof(main_only_options) / sizeof(char*);
  for (i = 0; i < size; i++) {
    if (StringCaseEqual(main_only_options[i], option_name)) {
      return kProcessScopeStrict;
    }
  }

  size = sizeof(server_only_options) / sizeof(char*);
  for (i = 0; i < size; i++) {
    if (StringCaseEqual(server_only_options[i], option_name)) {
      return kServerScope;
    }
  }

  // This could be made more efficient if RewriteOptions provided a map allowing
  // access of options by their name. It's not too much of a worry at present
  // since this is just during initialization.
  for (OptionBaseVector::const_iterator it = all_options().begin();
       it != all_options().end(); ++it) {
    RewriteOptions::OptionBase* option = *it;
    if (option->option_name() == option_name) {
      // We treat kLegacyProcessScope as kProcessScopeStrict, failing to start
      // if an option is out of place.
      return option->scope() == kLegacyProcessScope ? kProcessScopeStrict
                                                    : option->scope();
    }
  }
  return kDirectoryScope;
}

EnvoyRewriteOptions* EnvoyRewriteOptions::Clone() const {
  EnvoyRewriteOptions* options = new EnvoyRewriteOptions(
      StrCat("cloned from ", description()), thread_system());
  options->Merge(*this);
  return options;
}

const EnvoyRewriteOptions* EnvoyRewriteOptions::DynamicCast(
    const RewriteOptions* instance) {
  return dynamic_cast<const EnvoyRewriteOptions*>(instance);
}

EnvoyRewriteOptions* EnvoyRewriteOptions::DynamicCast(
    RewriteOptions* instance) {
  return dynamic_cast<EnvoyRewriteOptions*>(instance);
}

}  // namespace net_instaweb
