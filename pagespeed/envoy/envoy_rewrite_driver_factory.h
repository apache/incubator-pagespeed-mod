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

#include <set>

#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"

namespace net_instaweb {

class EnvoyMessageHandler;
class EnvoyRewriteOptions;
class EnvoyServerContext;
class SharedCircularBuffer;
class SharedMemRefererStatistics;
class SlowWorker;
class Statistics;
class SystemThreadSystem;

class EnvoyRewriteDriverFactory : public SystemRewriteDriverFactory {
 public:
  // We take ownership of the thread system.
  explicit EnvoyRewriteDriverFactory(const ProcessContext& process_context,
                                     SystemThreadSystem* system_thread_system,
                                     StringPiece hostname, int port);
  ~EnvoyRewriteDriverFactory() override;
  Hasher* NewHasher() override;
  UrlAsyncFetcher* AllocateFetcher(SystemRewriteOptions* config) override;
  MessageHandler* DefaultHtmlParseMessageHandler() override;
  MessageHandler* DefaultMessageHandler() override;
  FileSystem* DefaultFileSystem() override;
  Timer* DefaultTimer() override;
  NamedLockManager* DefaultLockManager() override;
  // Create a new RewriteOptions.  In this implementation it will be an
  // EnvoyRewriteOptions, and it will have CoreFilters explicitly set.
  RewriteOptions* NewRewriteOptions() override;
  RewriteOptions* NewRewriteOptionsForQuery() override;
  ServerContext* NewDecodingServerContext() override;

  // Initializes all the statistics objects created transitively by
  // EnvoyRewriteDriverFactory, including envoy-specific and
  // platform-independent statistics.
  static void InitStats(Statistics* statistics);
  EnvoyServerContext* MakeEnvoyServerContext(StringPiece hostname, int port);
  ServerContext* NewServerContext() override;
  void ShutDown() override;

  // Starts pagespeed threads if they've not been started already.  Must be
  // called after the caller has finished any forking it intends to do.
  void StartThreads();

  EnvoyMessageHandler* envoy_message_handler() {
    return envoy_message_handler_;
  }

  void NonStaticInitStats(Statistics* statistics) override {
    InitStats(statistics);
  }

  void SetMainConf(EnvoyRewriteOptions* main_conf);

  void LoggingInit(bool may_install_crash_handler);

  void SetServerContextMessageHandler(ServerContext* server_context);

  void ShutDownMessageHandlers() override;

  void SetCircularBuffer(SharedCircularBuffer* buffer) override;

  void PrepareForkedProcess(const char* name) override;

  void NameProcess(const char* name) override;

 private:
  // Timer *timer_;

  bool threads_started_;
  EnvoyMessageHandler* envoy_message_handler_;
  EnvoyMessageHandler* envoy_html_parse_message_handler_;
  typedef std::set<EnvoyMessageHandler*> EnvoyMessageHandlerSet;
  EnvoyMessageHandlerSet server_context_message_handlers_;
  SharedCircularBuffer* envoy_shared_circular_buffer_;
  GoogleString hostname_;
  int port_;
  bool shut_down_;
  DISALLOW_COPY_AND_ASSIGN(EnvoyRewriteDriverFactory);
};

}  // namespace net_instaweb
