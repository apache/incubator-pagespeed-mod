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

#include "pagespeed/envoy/envoy_rewrite_driver_factory.h"

#include <cstdio>

#include "net/instaweb/http/public/rate_controller.h"
#include "net/instaweb/http/public/rate_controlling_url_async_fetcher.h"
#include "net/instaweb/http/public/wget_url_fetcher.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/util/public/property_cache.h"
#include "pagespeed/envoy/envoy_message_handler.h"
#include "pagespeed/envoy/envoy_rewrite_options.h"
#include "pagespeed/envoy/envoy_server_context.h"
#include "pagespeed/envoy/log_message_handler.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/null_shared_mem.h"
#include "pagespeed/kernel/base/posix_timer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/sharedmem/shared_circular_buffer.h"
#include "pagespeed/kernel/sharedmem/shared_mem_statistics.h"
#include "pagespeed/kernel/thread/pthread_shared_mem.h"
#include "pagespeed/kernel/thread/scheduler_thread.h"
#include "pagespeed/kernel/thread/slow_worker.h"
#include "pagespeed/system/in_place_resource_recorder.h"
#include "pagespeed/system/serf_url_async_fetcher.h"
#include "pagespeed/system/system_caches.h"
#include "pagespeed/system/system_rewrite_options.h"

namespace net_instaweb {

class FileSystem;
class Hasher;
class MessageHandler;
class Statistics;
class Timer;
class UrlAsyncFetcher;
class UrlFetcher;
class Writer;

class SharedCircularBuffer;

EnvoyRewriteDriverFactory::EnvoyRewriteDriverFactory(
    const ProcessContext& process_context,
    SystemThreadSystem* system_thread_system, StringPiece hostname, int port)
    : SystemRewriteDriverFactory(
          process_context, system_thread_system,
          new PthreadSharedMem() /* default shared memory runtime */, hostname,
          port),
      threads_started_(false),
      envoy_message_handler_(
          new EnvoyMessageHandler(timer(), thread_system()->NewMutex())),
      envoy_html_parse_message_handler_(
          new EnvoyMessageHandler(timer(), thread_system()->NewMutex())),
      envoy_shared_circular_buffer_(nullptr),
      hostname_(hostname.as_string()),
      port_(port),
      shut_down_(false) {
  InitializeDefaultOptions();
  default_options()->set_beacon_url("/envoy_pagespeed_beacon");
  default_options()->set_enabled(RewriteOptions::kEnabledOn);
  default_options()->SetRewriteLevel(RewriteOptions::kCoreFilters);

  SystemRewriteOptions* system_options =
      dynamic_cast<SystemRewriteOptions*>(default_options());
  system_options->set_log_dir("/tmp/envoy_pagespeed_log/");
  system_options->set_statistics_logging_enabled(true);
  // ExternalClusterSpec spec = {{ExternalServerSpec("127.0.0.1", 11211)}};
  // system_options->set_memcached_servers(spec);

  system_options->set_file_cache_clean_inode_limit(500000);
  system_options->set_file_cache_clean_size_kb(1024 * 10000);  // 10 GB
  system_options->set_avoid_renaming_introspective_javascript(true);
  system_options->set_file_cache_path("/tmp/envoy_pagespeed_cache/");
  system_options->set_lru_cache_byte_limit(163840);
  system_options->set_lru_cache_kb_per_process(1024 * 500);  // 500 MB

  system_options->set_flush_html(true);

  // EnvoyRewriteOptions *options = (EnvoyRewriteOptions *)system_options;
  // std::vector<std::string> args;
  // args.push_back("RateLimitBackgroundFetches");
  // args.push_back("on");
  // global_settings settings;
  // const char *msg = options->ParseAndSetOptions(args, envoy_message_handler_,
  // settings); CHECK(!msg);

  set_message_buffer_size(1024 * 128);
  set_message_handler(envoy_message_handler_);
  set_html_parse_message_handler(envoy_html_parse_message_handler_);
  StartThreads();
}

EnvoyRewriteDriverFactory::~EnvoyRewriteDriverFactory() {
  ShutDown();
  envoy_shared_circular_buffer_ = nullptr;
  // message handlers are owned by RewriteDriverFactory
  envoy_message_handler_ = nullptr;
  envoy_html_parse_message_handler_ = nullptr;
  STLDeleteElements(&uninitialized_server_contexts_);
}

Hasher* EnvoyRewriteDriverFactory::NewHasher() { return new MD5Hasher; }

UrlAsyncFetcher* EnvoyRewriteDriverFactory::AllocateFetcher(
    SystemRewriteOptions* config) {
  return SystemRewriteDriverFactory::AllocateFetcher(config);
}

MessageHandler* EnvoyRewriteDriverFactory::DefaultHtmlParseMessageHandler() {
  return envoy_html_parse_message_handler_;
}

MessageHandler* EnvoyRewriteDriverFactory::DefaultMessageHandler() {
  return envoy_message_handler_;
}

FileSystem* EnvoyRewriteDriverFactory::DefaultFileSystem() {
  return new StdioFileSystem();
}

Timer* EnvoyRewriteDriverFactory::DefaultTimer() { return new PosixTimer; }

NamedLockManager* EnvoyRewriteDriverFactory::DefaultLockManager() {
  CHECK(false);
  return nullptr;
}

RewriteOptions* EnvoyRewriteDriverFactory::NewRewriteOptions() {
  EnvoyRewriteOptions* options = new EnvoyRewriteOptions(thread_system());
  // TODO(jefftk): figure out why using SetDefaultRewriteLevel like
  // mod_pagespeed does in mod_instaweb.cc:create_dir_config() isn't enough here
  // -- if you use that instead then envoy_pagespeed doesn't actually end up
  // defaulting CoreFilters.
  // See: https://github.com/apache/incubator-pagespeed-envoy/issues/1190
  options->SetRewriteLevel(RewriteOptions::kCoreFilters);
  return options;
}

RewriteOptions* EnvoyRewriteDriverFactory::NewRewriteOptionsForQuery() {
  return new EnvoyRewriteOptions(thread_system());
}

EnvoyServerContext* EnvoyRewriteDriverFactory::MakeEnvoyServerContext(
    StringPiece hostname, int port) {
  EnvoyServerContext* server_context =
      new EnvoyServerContext(this, hostname, port);
  uninitialized_server_contexts_.insert(server_context);
  return server_context;
}

ServerContext* EnvoyRewriteDriverFactory::NewDecodingServerContext() {
  ServerContext* sc = new EnvoyServerContext(this, hostname_, port_);
  InitStubDecodingServerContext(sc);
  return sc;
}

ServerContext* EnvoyRewriteDriverFactory::NewServerContext() {
  LOG(DFATAL) << "MakeEnvoyServerContext should be used instead";
  return nullptr;
}

void EnvoyRewriteDriverFactory::ShutDown() {
  if (!shut_down_) {
    shut_down_ = true;
    SystemRewriteDriverFactory::ShutDown();
  }
}

void EnvoyRewriteDriverFactory::ShutDownMessageHandlers() {
  envoy_message_handler_->set_buffer(nullptr);
  envoy_html_parse_message_handler_->set_buffer(nullptr);
  for (EnvoyMessageHandlerSet::iterator p =
           server_context_message_handlers_.begin();
       p != server_context_message_handlers_.end(); ++p) {
    (*p)->set_buffer(nullptr);
  }
  server_context_message_handlers_.clear();
}

void EnvoyRewriteDriverFactory::StartThreads() {
  if (threads_started_) {
    return;
  }
  // TODO(oschaaf): Can we use Envoy-native scheduling?
  SchedulerThread* thread = new SchedulerThread(thread_system(), scheduler());
  bool ok = thread->Start();
  CHECK(ok) << "Unable to start scheduler thread";
  defer_cleanup(thread->MakeDeleter());
  threads_started_ = true;
}

void EnvoyRewriteDriverFactory::SetMainConf(EnvoyRewriteOptions* main_options) {
  // Propagate process-scope options from the copy we had during Envoy option
  // parsing to our own.
  if (main_options != nullptr) {
    default_options()->MergeOnlyProcessScopeOptions(*main_options);
  }
}

void EnvoyRewriteDriverFactory::LoggingInit(bool may_install_crash_handler) {
  /*
  log_ = log;
  net_instaweb::log_message_handler::Install(log);
  if (may_install_crash_handler && install_crash_handler())
  {
      EnvoyMessageHandler::InstallCrashHandler(log);
  }
  envoy_message_handler_->set_log(log);
  envoy_html_parse_message_handler_->set_log(log);
  */
}

void EnvoyRewriteDriverFactory::SetServerContextMessageHandler(
    ServerContext* server_context) {
  EnvoyMessageHandler* handler =
      new EnvoyMessageHandler(timer(), thread_system()->NewMutex());
  handler->set_buffer(envoy_shared_circular_buffer_);
  server_context_message_handlers_.insert(handler);
  defer_cleanup(new Deleter<EnvoyMessageHandler>(handler));
  server_context->set_message_handler(handler);
}

void EnvoyRewriteDriverFactory::SetCircularBuffer(
    SharedCircularBuffer* buffer) {
  envoy_shared_circular_buffer_ = buffer;
  envoy_message_handler_->set_buffer(buffer);
  envoy_html_parse_message_handler_->set_buffer(buffer);
}

void EnvoyRewriteDriverFactory::InitStats(Statistics* statistics) {
  // Init standard PSOL stats.
  SystemRewriteDriverFactory::InitStats(statistics);
  RewriteDriverFactory::InitStats(statistics);
  RateController::InitStats(statistics);

  // Init Envoy-specific stats.
  EnvoyServerContext::InitStats(statistics);
  InPlaceResourceRecorder::InitStats(statistics);
}

void EnvoyRewriteDriverFactory::PrepareForkedProcess(const char* name) {
  // envoy_pid = envoy_getpid(); // Needed for logging to have the right PIDs.
  SystemRewriteDriverFactory::PrepareForkedProcess(name);
}

void EnvoyRewriteDriverFactory::NameProcess(const char* name) {
  SystemRewriteDriverFactory::NameProcess(name);
  // char name_for_setproctitle[32];
  // snprintf(name_for_setproctitle, sizeof(name_for_setproctitle),
  //         "pagespeed %s", name);
  // envoy_setproctitle(name_for_setproctitle);
}

}  // namespace net_instaweb