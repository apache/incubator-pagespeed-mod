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


#include "pagespeed/system/system_cache_path.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver_factory.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/callback.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/cache/cache_stats.h"
#include "pagespeed/kernel/cache/file_cache.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/cache/purge_context.h"
#include "pagespeed/kernel/cache/purge_set.h"
#include "pagespeed/kernel/cache/threadsafe_cache.h"
#include "pagespeed/kernel/sharedmem/shared_mem_lock_manager.h"
#include "pagespeed/kernel/util/file_system_lock_manager.h"
#include "pagespeed/system/system_rewrite_options.h"
#include "pagespeed/system/system_server_context.h"

namespace net_instaweb {

const char SystemCachePath::kFileCache[] = "file_cache";
const char SystemCachePath::kLruCache[] = "lru_cache";

// The SystemCachePath encapsulates a cache-sharing model where a user specifies
// a file-cache path per virtual-host.  With each file-cache object we keep
// a locking mechanism and an optional per-process LRUCache.
SystemCachePath::SystemCachePath(const StringPiece& path,
                                 const SystemRewriteOptions* config,
                                 RewriteDriverFactory* factory,
                                 AbstractSharedMem* shm_runtime)
    : path_(path.data(), path.size()),
      factory_(factory),
      shm_runtime_(shm_runtime),
      lock_manager_(NULL),
      file_cache_backend_(NULL),
      lru_cache_(NULL),
      file_cache_(NULL),
      cache_flush_filename_(config->cache_flush_filename()),
      unplugged_(config->unplugged()),
      enable_cache_purge_(config->enable_cache_purge()),
      clean_interval_explicitly_set_(
          config->has_file_cache_clean_interval_ms()),
      clean_size_explicitly_set_(config->has_file_cache_clean_size_kb()),
      clean_inode_limit_explicitly_set_(
          config->has_file_cache_clean_inode_limit()),
      mutex_(factory->thread_system()->NewMutex()) {
  if (cache_flush_filename_.empty()) {
    if (enable_cache_purge_) {
      cache_flush_filename_ = "cache.purge";
    } else {
      cache_flush_filename_ = "cache.flush";
    }
  }
  if (cache_flush_filename_[0] != '/') {
    // Implementations must ensure the file cache path is an absolute path.
    // mod_pagespeed checks in mod_instaweb.cc:pagespeed_post_config while
    // ngx_pagespeed checks in ngx_pagespeed.cc:ps_merge_srv_conf.
    // There is at least one example where this check is violated in
    // ngx_pagespeed. Example:
    // server {
    //   pagespeed off;
    //   pagespeed FileCachePath "/tmp";
    //   location / {
    //     pagespeed on;
    //   }
    // }
    //
    // Fixing this would require knowing if pagespeed is ever switched on within
    // a deeper level of the block. When this is parsed, we just have knowledge
    // of the higher-level server block.
    StringPiece path(config->file_cache_path());
    cache_flush_filename_ = StrCat(
        path, (path.size() > 0 && strings::EndsWith(path, "/")) ? "" : "/",
        cache_flush_filename_);
  }

  if (config->use_shared_mem_locking()) {
    shared_mem_lock_manager_.reset(new SharedMemLockManager(
        shm_runtime, LockManagerSegmentName(),
        factory->scheduler(), factory->hasher(), factory->message_handler()));
    lock_manager_ = shared_mem_lock_manager_.get();
  } else {
    FallBackToFileBasedLocking();
  }

  FileCache::CachePolicy* policy = new FileCache::CachePolicy(
      factory->timer(),
      factory->hasher(),
      config->file_cache_clean_interval_ms(),
      config->file_cache_clean_size_kb() * 1024,
      config->file_cache_clean_inode_limit());
  file_cache_backend_ =
      new FileCache(config->file_cache_path(), factory->file_system(),
                    factory->thread_system(), NULL, policy,
                    factory->statistics(), factory->message_handler());
  factory->TakeOwnership(file_cache_backend_);
  file_cache_ = new CacheStats(kFileCache, file_cache_backend_,
                               factory->timer(), factory->statistics());
  factory->TakeOwnership(file_cache_);

  if (config->lru_cache_kb_per_process() != 0) {
    LRUCache* lru_cache = new LRUCache(
        config->lru_cache_kb_per_process() * 1024);
    factory->TakeOwnership(lru_cache);

    // We only add the threadsafe-wrapper to the LRUCache.  The FileCache
    // is naturally thread-safe because it's got no writable member variables.
    // And surrounding that slower-running class with a mutex would likely
    // cause contention.
    ThreadsafeCache* ts_cache =
        new ThreadsafeCache(lru_cache, factory->thread_system()->NewMutex());
    factory->TakeOwnership(ts_cache);
    lru_cache_ = new CacheStats(kLruCache, ts_cache, factory->timer(),
                                factory->statistics());
    factory->TakeOwnership(lru_cache_);
  }
}

SystemCachePath::~SystemCachePath() {
}

// static
GoogleString SystemCachePath::CachePath(SystemRewriteOptions* config) {
  return (config->unplugged()
          ? "<unplugged>"
          : StrCat(config->file_cache_path(),
                   config->enable_cache_purge() ? " purge " : " flush ",
                   config->cache_flush_filename()));
}

void SystemCachePath::MergeConfig(const SystemRewriteOptions* config) {
  FileCache::CachePolicy* policy = file_cache_backend_->mutable_cache_policy();

  // For the interval, we take the smaller of the specified intervals, so
  // we get at least as much cache cleaning as each vhost owner wants.
  MergeEntries(config->file_cache_clean_interval_ms(),
               config->has_file_cache_clean_interval_ms(),
               false /* take_larger */,
               "IntervalMs",
               &policy->clean_interval_ms,
               &clean_interval_explicitly_set_);

  // For the sizes, we take the maximum value, so that the owner of any
  // vhost gets at least as much disk space as they asked for.  Note,
  // an argument could be made either way, but there's really no right
  // answer here, which is why MergeEntries prints a warning on a conflict.
  MergeEntries(config->file_cache_clean_size_kb() * 1024,
               config->has_file_cache_clean_size_kb(),
               true, "SizeKb",
               &policy->target_size_bytes,
               &clean_size_explicitly_set_);
  MergeEntries(config->file_cache_clean_inode_limit(),
               config->has_file_cache_clean_inode_limit(),
               true, "InodeLimit",
               &policy->target_inode_count,
               &clean_inode_limit_explicitly_set_);
}

void SystemCachePath::MergeEntries(int64 config_value, bool config_was_set,
                                   bool take_larger,
                                   const char* name,
                                   int64* policy_value,
                                   bool* policy_was_set) {
  if (config_value != *policy_value) {
    // If only one of these values was explicitly set, then just silently
    // update to the explicitly set one.
    if (config_was_set && !*policy_was_set) {
      *policy_value = config_value;
      *policy_was_set = true;
    } else if (!config_was_set && *policy_was_set) {
      // No action required; ignore default value coming from the new config.
    } else {
      DCHECK(config_was_set && *policy_was_set);
      *policy_was_set = true;
      factory_->message_handler()->Message(
          kWarning,
          "Conflicting settings %s!=%s for FileCacheClean%s for file-cache %s, "
          "keeping the %s value",
          Integer64ToString(config_value).c_str(),
          Integer64ToString(*policy_value).c_str(),
          name,
          path_.c_str(),
          take_larger ? "larger" : "smaller");
      if ((take_larger && (config_value > *policy_value)) ||
          (!take_larger && (config_value < *policy_value))) {
        *policy_value = config_value;
      }
    }
  }
}

void SystemCachePath::RootInit() {
  factory_->message_handler()->Message(
      kInfo, "Initializing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Initialize()) {
    FallBackToFileBasedLocking();
  }
}

void SystemCachePath::ChildInit(SlowWorker* cache_clean_worker) {
  if (unplugged_) {
    return;
  }
  factory_->message_handler()->Message(
      kInfo, "Reusing shared memory for path: %s.", path_.c_str());
  if ((shared_mem_lock_manager_.get() != NULL) &&
      !shared_mem_lock_manager_->Attach()) {
    FallBackToFileBasedLocking();
  }
  if (file_cache_backend_ != NULL) {
    file_cache_backend_->set_worker(cache_clean_worker);
  }

  purge_context_.reset(new PurgeContext(cache_flush_filename_,
                                        factory_->file_system(),
                                        factory_->timer(),
                                        RewriteOptions::kCachePurgeBytes,
                                        factory_->thread_system(),
                                        lock_manager_,
                                        factory_->scheduler(),
                                        factory_->statistics(),
                                        factory_->message_handler()));
  purge_context_->set_enable_purge(enable_cache_purge_);
  purge_context_->SetUpdateCallback(NewPermanentCallback(
      this, &SystemCachePath::UpdateCachePurgeSet));
}

void SystemCachePath::GlobalCleanup(MessageHandler* handler) {
  if (shared_mem_lock_manager_.get() != NULL) {
    shared_mem_lock_manager_->GlobalCleanup(
        shm_runtime_, LockManagerSegmentName(), handler);
  }
}

void SystemCachePath::FallBackToFileBasedLocking() {
  if ((shared_mem_lock_manager_.get() != NULL) || (lock_manager_ == NULL)) {
    shared_mem_lock_manager_.reset(NULL);
    file_system_lock_manager_.reset(new FileSystemLockManager(
        factory_->file_system(), path_,
        factory_->scheduler(), factory_->message_handler()));
    lock_manager_ = file_system_lock_manager_.get();
  }
}

GoogleString SystemCachePath::LockManagerSegmentName() const {
  return StrCat(path_, "/named_locks");
}

void SystemCachePath::FlushCacheIfNecessary() {
  if (!unplugged_) {
    purge_context_->PollFileSystem();
  }
}

void SystemCachePath::AddServerContext(SystemServerContext* server_context) {
  ScopedMutex lock(mutex_.get());
  server_context_set_.insert(server_context);
}

void SystemCachePath::RemoveServerContext(SystemServerContext* server_context) {
  ScopedMutex lock(mutex_.get());
  server_context_set_.erase(server_context);
}

void SystemCachePath::UpdateCachePurgeSet(
    const CopyOnWrite<PurgeSet>& purge_set) {
  ScopedMutex lock(mutex_.get());
  for (ServerContextSet::iterator p = server_context_set_.begin(),
           e = server_context_set_.end(); p != e; ++p) {
    SystemServerContext* server_context = *p;
    server_context->UpdateCachePurgeSet(purge_set);
  }
}

}  // namespace net_instaweb
