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


#include "pagespeed/kernel/cache/file_cache.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/function.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/shared_string.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/thread/slow_worker.h"
#include "pagespeed/kernel/util/url_to_filename_encoder.h"

namespace net_instaweb {

namespace {  // For structs used only in Clean().

struct CompareByAtime {
 public:
  // Sort by ascending atime.
  bool operator()(const FileSystem::FileInfo& one,
                  const FileSystem::FileInfo& two) const {
    return one.atime_sec < two.atime_sec;
  }
};

}  // namespace

class FileCache::CacheCleanFunction : public Function {
 public:
  CacheCleanFunction(FileCache* cache, int64 next_clean_time_ms)
      : cache_(cache),
        next_clean_time_ms_(next_clean_time_ms) {}
  virtual ~CacheCleanFunction() {}
  virtual void Run() { cache_->CleanWithLocking(next_clean_time_ms_); }

 private:
  FileCache* cache_;
  int64 next_clean_time_ms_;
  DISALLOW_COPY_AND_ASSIGN(CacheCleanFunction);
};

const char FileCache::kBytesFreedInCleanup[] =
    "file_cache_bytes_freed_in_cleanup";
const char FileCache::kCleanups[] = "file_cache_cleanups";
const char FileCache::kDiskChecks[] = "file_cache_disk_checks";
const char FileCache::kEvictions[] = "file_cache_evictions";
const char FileCache::kSkippedCleanups[] = "file_cache_skipped_cleanups";
const char FileCache::kStartedCleanups[] = "file_cache_started_cleanups";
const char FileCache::kWriteErrors[] = "file_cache_write_errors";

// Filenames for the next scheduled clean time and the lockfile.  In
// order to prevent these from colliding with actual cachefiles, they
// contain characters that our filename encoder would escape.
const char FileCache::kCleanTimeName[] = "!clean!time!";
const char FileCache::kCleanLockName[] = "!clean!lock!";

// Be willing to wait for a cache cleaner that hasn't bumped it's lock file in
// the last 5min.  A successful cache cleaner should be hitting it far more
// often than every 5min, this leaves plenty of leeway to make sure we don't
// start running the cache cleaner twice at the same time.
const int FileCache::kLockTimeoutMs = Timer::kMinuteMs * 5;

namespace {

// Bump the lock once out of this many calls to Notify().
const int kLockBumpIntervalCycles = 1000;

class LockBumpingProgressNotifier : public FileSystem::ProgressNotifier {
 public:
  // Takes ownership of nothing.
  LockBumpingProgressNotifier(FileSystem* file_system,
                              GoogleString* clean_lock_path,
                              MessageHandler* handler) :
      file_system_(file_system),
      clean_lock_path_(clean_lock_path),
      handler_(handler),
      count_(0) {}

  void Notify() override {
    if (++count_ % kLockBumpIntervalCycles == 0) {
      // BumpLockTimeout will log errors if it fails.
      file_system_->BumpLockTimeout(clean_lock_path_->c_str(), handler_);
    }
    // TODO(jefftk): Consider using this callback to throttle cache cleaning
    // iops as well.
  }

 private:
  FileSystem* file_system_;
  GoogleString* clean_lock_path_;
  MessageHandler* handler_;

  // Incremented on every Notify() call so we can bump the lock only every
  // kLockBumpInterval calls.
  int64 count_;
};

}  // namespace

// TODO(abliss): remove policy from constructor; provide defaults here
// and setters below.
FileCache::FileCache(const GoogleString& path, FileSystem* file_system,
                     ThreadSystem* thread_system, SlowWorker* worker,
                     CachePolicy* policy, Statistics* stats,
                     MessageHandler* handler)
    : path_(path),
      file_system_(file_system),
      worker_(worker),
      message_handler_(handler),
      cache_policy_(policy),
      mutex_(thread_system->NewMutex()),
      next_clean_ms_(INT64_MAX),
      path_length_limit_(file_system_->MaxPathLength(path)),
      clean_time_path_(path),
      clean_lock_path_(path),
      notifier_for_tests_(nullptr),
      disk_checks_(stats->GetVariable(kDiskChecks)),
      cleanups_(stats->GetVariable(kCleanups)),
      evictions_(stats->GetVariable(kEvictions)),
      bytes_freed_in_cleanup_(stats->GetVariable(kBytesFreedInCleanup)),
      skipped_cleanups_(stats->GetVariable(kSkippedCleanups)),
      started_cleanups_(stats->GetVariable(kStartedCleanups)),
      write_errors_(stats->GetVariable(kWriteErrors)) {
  if (policy->cleaning_enabled()) {
    next_clean_ms_ = policy->timer->NowMs() + policy->clean_interval_ms / 2;
  }
  EnsureEndsInSlash(&clean_time_path_);
  StrAppend(&clean_time_path_, kCleanTimeName);
  EnsureEndsInSlash(&clean_lock_path_);
  StrAppend(&clean_lock_path_, kCleanLockName);
}

FileCache::~FileCache() {
}

void FileCache::InitStats(Statistics* statistics) {
  statistics->AddVariable(kBytesFreedInCleanup);
  statistics->AddVariable(kCleanups);
  statistics->AddVariable(kDiskChecks);
  statistics->AddVariable(kEvictions);
  statistics->AddVariable(kSkippedCleanups);
  statistics->AddVariable(kStartedCleanups);
  statistics->AddVariable(kWriteErrors);
}

void FileCache::Get(const GoogleString& key, Callback* callback) {
  GoogleString filename;
  bool ret = EncodeFilename(key, &filename);
  if (ret) {
    // Suppress read errors.  Note that we want to show Write errors,
    // as they likely indicate a permissions or disk-space problem
    // which is best not eaten.  It's cheap enough to construct
    // a NullMessageHandler on the stack when we want one.
    NullMessageHandler null_handler;
    GoogleString buf;
    ret = file_system_->ReadFile(filename.c_str(), &buf, &null_handler);
    callback->set_value(SharedString(buf));
  }
  ValidateAndReportResult(key, ret ? kAvailable : kNotFound, callback);
}

void FileCache::Put(const GoogleString& key, const SharedString& value) {
  GoogleString filename;
  if (EncodeFilename(key, &filename) &&
      !file_system_->WriteFileAtomic(filename, value.Value(),
                                     message_handler_)) {
    write_errors_->Add(1);
  }
  CleanIfNeeded();
}

void FileCache::Delete(const GoogleString& key) {
  GoogleString filename;
  if (!EncodeFilename(key, &filename)) {
    return;
  }
  NullMessageHandler null_handler;  // Do not emit messages on delete failures.
  file_system_->RemoveFile(filename.c_str(), &null_handler);
}

bool FileCache::EncodeFilename(const GoogleString& key,
                               GoogleString* filename) {
  GoogleString prefix = path_;
  // TODO(abliss): unify and make explicit everyone's assumptions
  // about trailing slashes.
  EnsureEndsInSlash(&prefix);
  UrlToFilenameEncoder::EncodeSegment(prefix, key, '/', filename);

  // Make sure the length isn't too big for filesystem to handle; if it is
  // just name the object using a hash.
  if (static_cast<int>(filename->length()) > path_length_limit_) {
    UrlToFilenameEncoder::EncodeSegment(
        prefix, cache_policy_->hasher->Hash(key), '/', filename);
  }

  return true;
}

namespace {
// The minimum age an empty directory needs to be before cache cleaning will
// delete it. This is to prevent cache cleaning from removing file lock
// directories that StdioFileSystem uses and is set to be double
// ServerContext::kBreakLockMs / kSecondMs.
const int64 kEmptyDirCleanAgeSec = 60;

}  // namespace

bool FileCache::Clean(int64 target_size_bytes, int64 target_inode_count) {
  started_cleanups_->Add(1);

  DCHECK(cache_policy_->cleaning_enabled());
  // While this function can delete .lock and .outputlock files, the use of
  // kEmptyDirCleanAgeSec should keep that from being a problem.
  message_handler_->Message(kInfo,
                            "Checking cache size against target %s and inode "
                            "count against target %s",
                            Integer64ToString(target_size_bytes).c_str(),
                            Integer64ToString(target_inode_count).c_str());
  disk_checks_->Add(1);

  bool everything_ok = true;

  LockBumpingProgressNotifier lock_bumping_notifier(
      file_system_, &clean_lock_path_, message_handler_);
  FileSystem::ProgressNotifier* notifier = &lock_bumping_notifier;
  if (notifier_for_tests_ != NULL) {
    notifier = notifier_for_tests_;
  }
  // Get the contents of the cache
  FileSystem::DirInfo dir_info;
  file_system_->GetDirInfoWithProgress(
      path_, &dir_info, notifier, message_handler_);

  // Check to see if cache size or inode count exceeds our limits.
  // target_inode_count of 0 indicates no inode limit.
  int64 cache_size = dir_info.size_bytes;
  int64 cache_inode_count = dir_info.inode_count;
  if (cache_size < target_size_bytes &&
      (target_inode_count == 0 ||
       cache_inode_count < target_inode_count)) {
    message_handler_->Message(kInfo,
                              "File cache size is %s and contains %s inodes; "
                              "no cleanup needed.",
                              Integer64ToString(cache_size).c_str(),
                              Integer64ToString(cache_inode_count).c_str());
    return true;
  }

  message_handler_->Message(kInfo,
                            "File cache size is %s and contains %s inodes; "
                            "beginning cleanup.",
                            Integer64ToString(cache_size).c_str(),
                            Integer64ToString(cache_inode_count).c_str());
  cleanups_->Add(1);

  // Remove empty directories.
  StringVector::iterator it;
  for (it = dir_info.empty_dirs.begin(); it != dir_info.empty_dirs.end();
       ++it) {
    notifier->Notify();
    // StdioFileSystem uses an empty directory as a file lock. Avoid deleting
    // these file locks by not removing the file cache clean lock file, and
    // making sure empty directories are at least n seconds old before removing
    // them, where n is double ServerContext::kBreakLockMs.
    int64 timestamp_sec;
    file_system_->Mtime(*it, &timestamp_sec, message_handler_);
    const int64 now_sec = cache_policy_->timer->NowMs() / Timer::kSecondMs;
    int64 age_sec = now_sec - timestamp_sec;
    if (age_sec > kEmptyDirCleanAgeSec &&
        clean_lock_path_.compare(it->c_str()) != 0) {
      everything_ok &= file_system_->RemoveDir(it->c_str(), message_handler_);
    }
    // Decrement cache_inode_count even if RemoveDir failed. This is likely
    // because the directory has already been removed.
    --cache_inode_count;
  }

  // Save original cache size to track how many bytes we've cleaned up.
  int64 orig_cache_size = cache_size;

  // Sort files by atime in ascending order to remove oldest files first.
  std::sort(dir_info.files.begin(), dir_info.files.end(), CompareByAtime());

  // Set the target size to clean to.
  target_size_bytes = (target_size_bytes * 3) / 4;
  target_inode_count = (target_inode_count * 3) / 4;

  // Delete files until we are under our targets.
  std::vector<FileSystem::FileInfo>::iterator file_itr = dir_info.files.begin();
  while (file_itr != dir_info.files.end() &&
         (cache_size > target_size_bytes ||
          (target_inode_count != 0 &&
           cache_inode_count > target_inode_count))) {
    notifier->Notify();
    FileSystem::FileInfo file = *file_itr;
    ++file_itr;
    // Don't clean the clean_time or clean_lock files! They ought to be the
    // newest files (and very small) so they would normally not be deleted
    // anyway. But on some systems (e.g. mounted noatime?) they were getting
    // deleted.
    if (clean_time_path_.compare(file.name) == 0 ||
        clean_lock_path_.compare(file.name) == 0) {
      continue;
    }
    cache_size -= file.size_bytes;
    // Decrement inode_count even if RemoveFile fails. This is likely because
    // the file has already been removed.
    --cache_inode_count;
    everything_ok &= file_system_->RemoveFile(file.name.c_str(),
                                              message_handler_);
    evictions_->Add(1);
  }

  int64 bytes_freed = orig_cache_size - cache_size;
  message_handler_->Message(kInfo,
                            "File cache cleanup complete; freed %s bytes",
                            Integer64ToString(bytes_freed).c_str());
  bytes_freed_in_cleanup_->Add(bytes_freed);
  return everything_ok;
}

void FileCache::CleanWithLocking(int64 next_clean_time_ms) {
  if (file_system_->TryLockWithTimeout(clean_lock_path_, kLockTimeoutMs,
                                       cache_policy_->timer,
                                       message_handler_).is_true()) {
    // Update the timestamp file.
    {
      ScopedMutex lock(mutex_.get());
      next_clean_ms_ = next_clean_time_ms;
    }
    if (!file_system_->WriteFileAtomic(clean_time_path_,
                                       Integer64ToString(next_clean_time_ms),
                                       message_handler_)) {
      write_errors_->Add(1);
    }

    // Now actually clean.
    Clean(cache_policy_->target_size_bytes,
          cache_policy_->target_inode_count);
    file_system_->Unlock(clean_lock_path_, message_handler_);
  } else {
    // The previous cache cleaning run is still active, so skip this round.
    skipped_cleanups_->Add(1);
    message_handler_->Message(
        kInfo, "Skipped file cache cleaning: previous cleanup still ongoing");
  }
}

bool FileCache::ShouldClean(int64* suggested_next_clean_time_ms) {
  if (!cache_policy_->cleaning_enabled()) {
    return false;
  }

  bool to_return = false;
  const int64 now_ms = cache_policy_->timer->NowMs();
  {
    ScopedMutex lock(mutex_.get());
    if (now_ms < next_clean_ms_) {
      *suggested_next_clean_time_ms = next_clean_ms_;  // No change yet.
      return false;
    }
  }

  GoogleString clean_time_str;
  int64 clean_time_ms = 0;
  int64 new_clean_time_ms = now_ms + cache_policy_->clean_interval_ms;
  NullMessageHandler null_handler;
  if (file_system_->ReadFile(clean_time_path_.c_str(), &clean_time_str,
                             &null_handler)) {
    StringToInt64(clean_time_str, &clean_time_ms);
  } else {
    message_handler_->Message(
        kWarning, "Failed to read cache clean timestamp %s. "
        " Doing an extra cache clean to be safe.", clean_time_path_.c_str());
  }

  // If the "clean time" written in the file is older than now, we clean.
  if (clean_time_ms < now_ms) {
    message_handler_->Message(
        kInfo, "Need to check cache size against target %s",
        Integer64ToString(cache_policy_->target_size_bytes).c_str());
    to_return = true;
  }
  // If the "clean time" is later than now plus one interval, something
  // went wrong (like the system clock moving backwards or the file
  // getting corrupt) so we clean and reset it.
  if (clean_time_ms > new_clean_time_ms) {
    message_handler_->Message(kError,
                              "Next scheduled file cache clean time %s"
                              " is implausibly remote.  Cleaning now.",
                              Integer64ToString(clean_time_ms).c_str());
    to_return = true;
  }

  *suggested_next_clean_time_ms = new_clean_time_ms;
  if (!to_return) {
    ScopedMutex lock(mutex_.get());
    next_clean_ms_ = new_clean_time_ms;
  }
  return to_return;
}

void FileCache::CleanIfNeeded() {
  DCHECK(worker_ != NULL);
  if (worker_ != NULL) {
    int64 suggested_next_clean_time_ms;
    if (ShouldClean(&suggested_next_clean_time_ms)) {
      worker_->Start();
      // TODO(jefftk): On systems with multiple filecaches that take non-trivial
      // amounts of time to clean this is probably not right.  If at least two
      // caches are getting at least 1QPS of PUTs then they'll keep their clean
      // times synchronized and each time one of them will randomly get to run
      // and the others won't.  We could fix this by having cache cleaning be
      // global, and clean all file caches together, we could have the worker
      // queue cache cleaning jobs, or we could bump next_clean_time by
      // something much less than the cache cleaning interval if the worker is
      // busy here.
      worker_->RunIfNotBusy(
          new CacheCleanFunction(this, suggested_next_clean_time_ms));
    }
  }
}

}  // namespace net_instaweb
