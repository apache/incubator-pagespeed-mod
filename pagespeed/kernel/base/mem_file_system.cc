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


#include "pagespeed/kernel/base/mem_file_system.h"

#include <cstddef>
#include <map>
#include <utility>

#include "base/logging.h"
#include "pagespeed/kernel/base/abstract_mutex.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/mock_timer.h"

namespace net_instaweb {

class MemInputFile : public FileSystem::InputFile {
 public:
  MemInputFile(const StringPiece& filename, const GoogleString& contents)
      : contents_(contents),
        filename_(filename.data(), filename.size()),
        offset_(0) {
  }

  bool Close(MessageHandler* message_handler) override {
    offset_ = contents_.length();
    return true;
  }

  const char* filename() override { return filename_.c_str(); }

  int Read(char* buf, int size, MessageHandler* message_handler) override {
    if (size + offset_ > static_cast<int>(contents_.length())) {
      size = contents_.length() - offset_;
    }
    memcpy(buf, contents_.c_str() + offset_, size);
    offset_ += size;
    return size;
  }

  bool ReadFile(GoogleString* buf, int64 max_file_size,
                MessageHandler* message_handler) override {
    if (max_file_size != FileSystem::kUnlimitedSize &&
        contents_.length() > static_cast<size_t>(max_file_size)) {
      return false;
    }
    *buf = contents_;
    return true;
  }

 private:
  const GoogleString contents_;
  const GoogleString filename_;
  int offset_;

  DISALLOW_COPY_AND_ASSIGN(MemInputFile);
};


class MemOutputFile : public FileSystem::OutputFile {
 public:
  MemOutputFile(
      const StringPiece& filename, GoogleString* contents, bool append)
      : contents_(contents), filename_(filename.data(), filename.size()) {
    if (!append) {
      contents_->clear();
    }
  }

  virtual bool Close(MessageHandler* message_handler) {
    Flush(message_handler);
    return true;
  }

  virtual const char* filename() { return filename_.c_str(); }

  virtual bool Flush(MessageHandler* message_handler) {
    contents_->append(written_);
    written_.clear();
    return true;
  }

  virtual bool SetWorldReadable(MessageHandler* message_handler) {
    return true;
  }

  virtual bool Write(const StringPiece& buf, MessageHandler* handler) {
    buf.AppendToString(&written_);
    return true;
  }

 private:
  GoogleString* contents_;
  const GoogleString filename_;
  GoogleString written_;

  DISALLOW_COPY_AND_ASSIGN(MemOutputFile);
};

MemFileSystem::MemFileSystem(ThreadSystem* threads, Timer* timer)
    : lock_map_mutex_(threads->NewMutex()),
      all_else_mutex_(threads->NewMutex()),
      enabled_(true),
      timer_(timer),
      mock_timer_(NULL),
      temp_file_index_(0),
      atime_enabled_(true),
      advance_time_on_update_(false) {
  ClearStats();
}

MemFileSystem::~MemFileSystem() {
}

void MemFileSystem::UpdateAtime(const StringPiece& path) {
  if (atime_enabled_) {
    int64 now_us = timer_->NowUs();
    int64 now_s = now_us / Timer::kSecondUs;
    if (advance_time_on_update_) {
      mock_timer_->AdvanceUs(Timer::kSecondUs);
    }
    atime_map_[path.as_string()] = now_s;
  }
}

void MemFileSystem::UpdateMtime(const StringPiece& path) {
  int64 now_us = timer_->NowUs();
  int64 now_s = now_us / Timer::kSecondUs;
  mtime_map_[path.as_string()] = now_s;
}

void MemFileSystem::Clear() {
  ScopedMutex lock(all_else_mutex_.get());
  string_map_.clear();
}

BoolOrError MemFileSystem::Exists(const char* path, MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  StringStringMap::const_iterator iter = string_map_.find(path);
  return BoolOrError(iter != string_map_.end());
}

BoolOrError MemFileSystem::IsDir(const char* path, MessageHandler* handler) {
  return Exists(path, handler).is_true()
      ? BoolOrError(EndsInSlash(path)) : BoolOrError();
}

bool MemFileSystem::MakeDir(const char* path, MessageHandler* handler) {
  // We store directories as empty files with trailing slashes.
  ScopedMutex lock(all_else_mutex_.get());
  GoogleString path_string = path;
  EnsureEndsInSlash(&path_string);
  string_map_[path_string] = "";
  UpdateAtime(path_string);
  UpdateMtime(path_string);
  return true;
}

bool MemFileSystem::RemoveDir(const char* path, MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  GoogleString path_string = path;
  EnsureEndsInSlash(&path_string);

  StringStringMap::const_iterator iter = string_map_.find(path_string);

  // Verify that this directory exists
  if (iter == string_map_.end()) {
    handler->Message(kError, "Failed to remove directory %s: directory does "
                     "not exist", path);
    return false;
  }

  // Verify that no files are stored in this directory. We can do this by
  // checking to see if the next string in the map starts with this directory
  // path. Note this depends on using a data structure that keeps its elements
  // sorted by key (such as a map).
  StringStringMap::const_iterator next_iter = iter;
  ++next_iter;
  if (next_iter != string_map_.end() &&
      next_iter->first.find(iter->first) == 0) {
    handler->Message(kError, "Failed to remove directory %s: directory is not "
                     "empty", path);
    return false;
  }

  // This directory exists and is empty, so remove it
  atime_map_.erase(path_string);
  mtime_map_.erase(path_string);
  string_map_.erase(path_string);
  return true;
}

FileSystem::InputFile* MemFileSystem::OpenInputFile(
    const char* filename, MessageHandler* message_handler) {
  ScopedMutex lock(all_else_mutex_.get());

  ++num_input_file_opens_;
  if (!enabled_) {
    return NULL;
  }

  StringStringMap::const_iterator iter = string_map_.find(filename);
  if (iter == string_map_.end()) {
    message_handler->Error(filename, 0, "opening input file: %s",
                           "file not found");
    return NULL;
  } else {
    UpdateAtime(filename);
    return new MemInputFile(filename, iter->second);
  }
}

FileSystem::OutputFile* MemFileSystem::OpenOutputFileHelper(
    const char* filename, bool append, MessageHandler* message_handler) {
  ScopedMutex lock(all_else_mutex_.get());
  UpdateAtime(filename);
  UpdateMtime(filename);
  ++num_output_file_opens_;
  return new MemOutputFile(filename, &(string_map_[filename]), append);
}

FileSystem::OutputFile* MemFileSystem::OpenTempFileHelper(
    const StringPiece& prefix, MessageHandler* message_handler) {
  ScopedMutex lock(all_else_mutex_.get());
  GoogleString filename = StringPrintf("tmpfile%d", temp_file_index_++);
  UpdateAtime(filename);
  UpdateMtime(filename);
  ++num_temp_file_opens_;
  return new MemOutputFile(filename, &string_map_[filename], false);
}

bool MemFileSystem::RecursivelyMakeDir(const StringPiece& full_path_const,
                                       MessageHandler* handler) {
  // This is called to make sure that files can be written under the
  // named directory.  We don't have directories and files can be
  // written anywhere, so just return true.
  return true;
}

bool MemFileSystem::RemoveFile(const char* filename,
                               MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  atime_map_.erase(filename);
  mtime_map_.erase(filename);
  return (string_map_.erase(filename) == 1);
}

bool MemFileSystem::RenameFileHelper(const char* old_file,
                                     const char* new_file,
                                     MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());

  if (strcmp(old_file, new_file) == 0) {
    handler->Error(old_file, 0, "Cannot move a file to itself");
    return false;
  }

  StringStringMap::iterator iter = string_map_.find(old_file);
  if (iter == string_map_.end()) {
    handler->Error(old_file, 0, "File not found");
    return false;
  }

  string_map_[new_file] = iter->second;
  string_map_.erase(iter);

  UpdateAtime(new_file);
  atime_map_.erase(old_file);
  mtime_map_[new_file] = mtime_map_[old_file];
  mtime_map_.erase(old_file);

  return true;
}

bool MemFileSystem::ListContents(const StringPiece& dir, StringVector* files,
                                 MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  GoogleString prefix = dir.as_string();
  EnsureEndsInSlash(&prefix);
  const size_t prefix_length = prefix.size();
  // We don't have directories, so we just list everything in the
  // filesystem that matches the prefix and doesn't have another
  // internal slash.
  for (StringStringMap::iterator it = string_map_.begin(),
           end = string_map_.end(); it != end; it++) {
    const GoogleString& path = (*it).first;
    if ((0 == path.compare(0, prefix_length, prefix)) &&
        path.length() > prefix_length) {
      const size_t next_slash = path.find("/", prefix_length + 1);
      // Only want to list files without another slash, unless that
      // slash is the last char in the filename.
      if ((next_slash == GoogleString::npos)
          || (next_slash == path.length() - 1)) {
        files->push_back(path);
      }
    }
  }
  return true;
}

bool MemFileSystem::Atime(const StringPiece& path, int64* timestamp_sec,
                          MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  *timestamp_sec = atime_map_[path.as_string()];
  return true;
}

bool MemFileSystem::Mtime(const StringPiece& path, int64* timestamp_sec,
                          MessageHandler* handler) {
  ScopedMutex lock(all_else_mutex_.get());
  ++num_input_file_stats_;
  *timestamp_sec = mtime_map_[path.as_string()];
  return true;
}

bool MemFileSystem::Size(const StringPiece& path, int64* size,
                         MessageHandler* handler) const {
  ScopedMutex lock(all_else_mutex_.get());
  const GoogleString path_string = path.as_string();
  auto iter = string_map_.find(path_string);
  if (iter != string_map_.end()) {
    *size = iter->second.size();
    return true;
  } else {
    return false;
  }
}

BoolOrError MemFileSystem::TryLock(const StringPiece& lock_name,
                                   MessageHandler* handler) {
  ScopedMutex lock(lock_map_mutex_.get());

  auto ret = lock_map_.insert(
      std::make_pair(lock_name.as_string(), timer_->NowMs()));
  bool inserted = ret.second;
  return BoolOrError(inserted);
}

BoolOrError MemFileSystem::TryLockWithTimeout(const StringPiece& lock_name,
                                              int64 timeout_ms,
                                              const Timer* timer,
                                              MessageHandler* handler) {
  ScopedMutex lock(lock_map_mutex_.get());

  DCHECK_EQ(timer, timer_);
  int64 now = timer->NowMs();
  auto ret = lock_map_.insert(std::make_pair(lock_name.as_string(), now));
  auto iter = ret.first;
  bool inserted = ret.second;
  if (inserted) {
    // Lock wasn't already held, successfully issued.
    return BoolOrError(true);
  } else if (now <= iter->second + timeout_ms) {
    // Lock was held, timeout hasn't expired.
    return BoolOrError(false);
  } else {
    // Steal lock.
    iter->second = now;
    return BoolOrError(true);
  }
}

bool MemFileSystem::BumpLockTimeout(const StringPiece& lock_name,
                                    MessageHandler* handler) {
  ScopedMutex lock(lock_map_mutex_.get());

  auto iter = lock_map_.find(lock_name.as_string());
  if (iter == lock_map_.end()) {
    handler->Info(lock_name.as_string().c_str(), 0,
                  "Failed to bump lock: lock not held");
    return false;
  } else {
    iter->second = timer_->NowMs();
    return true;
  }
}

bool MemFileSystem::Unlock(const StringPiece& lock_name,
                           MessageHandler* handler) {
  ScopedMutex lock(lock_map_mutex_.get());
  return (lock_map_.erase(lock_name.as_string()) == 1);
}

bool MemFileSystem::WriteFile(const char* filename,
                              const StringPiece& buffer,
                              MessageHandler* handler) {
  bool ret = FileSystem::WriteFile(filename, buffer, handler);
  if (write_callback_.get() != NULL) {
    write_callback_.release()->Run(filename);
  }
  return ret;
}

bool MemFileSystem::WriteTempFile(const StringPiece& prefix_name,
                                  const StringPiece& buffer,
                                  GoogleString* filename,
                                  MessageHandler* handler) {
  bool ret = FileSystem::WriteTempFile(prefix_name, buffer, filename, handler);
  if (write_callback_.get() != NULL) {
    write_callback_.release()->Run(*filename);
  }
  return ret;
}

}  // namespace net_instaweb
