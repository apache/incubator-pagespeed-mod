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

#include <vector>

#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/ref_counted_ptr.h"
#include "pagespeed/kernel/base/stl_util.h"  // for STLDeleteElements
#include "pagespeed/system/system_rewrite_options.h"

#define ENVOY_PAGESPEED_MAX_ARGS 10

namespace net_instaweb {

class EnvoyRewriteDriverFactory;

class EnvoyRewriteOptions : public SystemRewriteOptions {
 public:
  // See rewrite_options::Initialize and ::Terminate
  static void Initialize();
  static void Terminate();

  EnvoyRewriteOptions(const StringPiece& description,
                      ThreadSystem* thread_system);
  explicit EnvoyRewriteOptions(ThreadSystem* thread_system);

  // Make an identical copy of these options and return it.
  EnvoyRewriteOptions* Clone() const override;

  // Returns a suitably down cast version of 'instance' if it is an instance
  // of this class, NULL if not.
  static const EnvoyRewriteOptions* DynamicCast(const RewriteOptions* instance);
  static EnvoyRewriteOptions* DynamicCast(RewriteOptions* instance);

  const GoogleString& statistics_path() const {
    return statistics_path_.value();
  }
  const GoogleString& global_statistics_path() const {
    return global_statistics_path_.value();
  }
  const GoogleString& console_path() const { return console_path_.value(); }
  const GoogleString& messages_path() const { return messages_path_.value(); }
  const GoogleString& admin_path() const { return admin_path_.value(); }
  const GoogleString& global_admin_path() const {
    return global_admin_path_.value();
  }

 private:
  // Keeps the properties added by this subclass.  These are merged into
  // RewriteOptions::all_properties_ during Initialize().
  //
  // RewriteOptions uses static initialization to reduce memory usage and
  // construction time.  All EnvoyRewriteOptions instances will have the same
  // Properties, so we can build the list when we initialize the first one.
  static Properties* envoy_properties_;
  static void AddProperties();
  void Init();

  // Add an option to envoy_properties_
  template <class OptionClass>
  static void add_envoy_option(typename OptionClass::ValueType default_value,
                               OptionClass EnvoyRewriteOptions::*offset,
                               const char* id, StringPiece option_name,
                               OptionScope scope, const char* help,
                               bool safe_to_print) {
    AddProperty(default_value, offset, id, option_name, scope, help,
                safe_to_print, envoy_properties_);
  }

  Option<GoogleString> statistics_path_;
  Option<GoogleString> global_statistics_path_;
  Option<GoogleString> console_path_;
  Option<GoogleString> messages_path_;
  Option<GoogleString> admin_path_;
  Option<GoogleString> global_admin_path_;

  // Helper for ParseAndSetOptions.  Returns whether the two directives equal,
  // ignoring case.
  bool IsDirective(StringPiece config_directive, StringPiece compare_directive);

  // Returns a given option's scope.
  RewriteOptions::OptionScope GetOptionScope(StringPiece option_name);

  // TODO(jefftk): support fetch proxy in server and location blocks.

  DISALLOW_COPY_AND_ASSIGN(EnvoyRewriteOptions);
};

}  // namespace net_instaweb
