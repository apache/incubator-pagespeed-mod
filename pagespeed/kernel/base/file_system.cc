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


#include "pagespeed/kernel/base/file_system.h"

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"

namespace net_instaweb {

FileSystem::~FileSystem() {
}

FileSystem::File::~File() {
}

FileSystem::InputFile::~InputFile() {
}

FileSystem::OutputFile::~OutputFile() {
}

int FileSystem::MaxPathLength(const StringPiece& base) const {
  return 8192;
}

bool FileSystem::ReadFile(const char* filename, GoogleString* buffer,
                          MessageHandler* message_handler) {
  return ReadFile(filename, kUnlimitedSize, buffer, message_handler);
}

bool FileSystem::ReadFile(const char* filename, Writer* writer,
                          MessageHandler* message_handler) {
  return ReadFile(filename, kUnlimitedSize, writer, message_handler);
}

bool FileSystem::ReadFile(const char* filename, int64 max_file_size,
                          GoogleString* buffer,
                          MessageHandler* message_handler) {
  InputFile* input_file = OpenInputFile(filename, message_handler);
  return ReadFile(input_file, max_file_size, buffer, message_handler);
}

bool FileSystem::ReadFile(const char* filename, int64 max_file_size,
                          Writer* writer, MessageHandler* message_handler) {
  InputFile* input_file = OpenInputFile(filename, message_handler);
  return ReadFile(input_file, max_file_size, writer, message_handler);
}

bool FileSystem::ReadFile(InputFile* input_file, GoogleString* buffer,
                          MessageHandler* message_handler) {
  return ReadFile(input_file, kUnlimitedSize, buffer, message_handler);
}

bool FileSystem::ReadFile(InputFile* input_file, Writer* writer,
                          MessageHandler* message_handler) {
  return ReadFile(input_file, kUnlimitedSize, writer, message_handler);
}

bool FileSystem::ReadFile(
    InputFile* input_file, int64 max_file_size, GoogleString* buffer,
    MessageHandler* message_handler) {
  bool ret = false;
  if (input_file != nullptr) {
    ret = input_file->ReadFile(buffer, max_file_size, message_handler);
    ret &= Close(input_file, message_handler);
  }
  return ret;
}

bool FileSystem::ReadFile(InputFile* input_file, int64 max_file_size,
                          Writer* writer, MessageHandler* message_handler) {
  bool ret = false;
  if (input_file != nullptr) {
    char buf[kStackBufferSize];
    int nread;
    ret = true;
    int64 total_size = 0;
    while (ret && ((nread = input_file->Read(
               buf, sizeof(buf), message_handler)) > 0)) {
      if (max_file_size != kUnlimitedSize) {
        total_size += nread;
        if (total_size > max_file_size) {
          ret = false;
          break;
        }
      }
      ret = writer->Write(StringPiece(buf, nread), message_handler);
    }
    ret &= (nread == 0);
    ret &= Close(input_file, message_handler);
  }
  return ret;
}

bool FileSystem::WriteFile(const char* filename, const StringPiece& buffer,
                           MessageHandler* message_handler) {
  OutputFile* output_file = OpenOutputFile(filename, message_handler);
  bool ret = false;
  if (output_file != nullptr) {
    ret = output_file->Write(buffer, message_handler);
    ret &= output_file->SetWorldReadable(message_handler);
    ret &= Close(output_file, message_handler);
  }
  return ret;
}

bool FileSystem::WriteTempFile(const StringPiece& prefix_name,
                               const StringPiece& buffer,
                               GoogleString* filename,
                               MessageHandler* message_handler) {
  OutputFile* output_file = OpenTempFile(prefix_name, message_handler);
  bool ok = (output_file != nullptr);
  if (ok) {
    // Store filename early, since it's invalidated by Close.
    *filename = output_file->filename();
    ok = output_file->Write(buffer, message_handler);
    // attempt Close even if write fails.
    ok &= Close(output_file, message_handler);
    if (!ok) {
      NullMessageHandler null_message_handler;
      RemoveFile(filename->c_str(), &null_message_handler);
    }
  }
  if (!ok) {
    // Clear filename so we end in a consistent state.
    filename->clear();
  }
  return ok;
}

bool FileSystem::WriteFileAtomic(const StringPiece& filename_sp,
                                 const StringPiece& buffer,
                                 MessageHandler* message_handler) {
  const GoogleString filename(filename_sp.as_string());
  GoogleString tempfilename;
  bool ok = false;

  if (WriteTempFile(StrCat(filename, ".temp"), buffer, &tempfilename,
                    message_handler)) {
    ok = RenameFile(tempfilename.c_str(), filename.c_str(), message_handler);
    if (!ok) {
      // Delete any temp file as it's probably incomplete.
      NullMessageHandler null_message_handler;
      RemoveFile(tempfilename.c_str(), &null_message_handler);
    }
  }
  return ok;
}

bool FileSystem::Close(File* file, MessageHandler* message_handler) {
  bool ret = file->Close(message_handler);
  delete file;
  return ret;
}


bool FileSystem::RecursivelyMakeDir(const StringPiece& full_path_const,
                                    MessageHandler* handler) {
  bool ret = true;
  GoogleString full_path = full_path_const.as_string();
  EnsureEndsInSlash(&full_path);
  GoogleString subpath;
  subpath.reserve(full_path.size());
  size_t old_pos = 0, new_pos;
  // Note that we intentionally start searching at pos = 1 to avoid having
  // subpath be "" on absolute paths.
  while ((new_pos = full_path.find('/', old_pos + 1)) != GoogleString::npos) {
    // Build up path, one segment at a time.
    subpath.append(full_path.data() + old_pos, new_pos - old_pos);
    if (Exists(subpath.c_str(), handler).is_false()) {
      if (!MakeDir(subpath.c_str(), handler)) {
        ret = false;
        break;
      }
    } else if (IsDir(subpath.c_str(), handler).is_false()) {
      handler->Message(kError, "Subpath '%s' of '%s' is a non-directory file.",
                       subpath.c_str(), full_path.c_str());
      ret = false;
      break;
    }
    old_pos = new_pos;
  }
  return ret;
}

void FileSystem::GetDirInfo(const StringPiece& path, DirInfo* dirinfo,
                            MessageHandler* handler) {
  NullProgressNotifier notifier;
  GetDirInfoWithProgress(path, dirinfo, &notifier, handler);
}

void FileSystem::GetDirInfoWithProgress(
    const StringPiece& path, DirInfo* dirinfo, ProgressNotifier* notifier,
    MessageHandler* handler) {
  // This function is not guaranteed to produce correct results if files or
  // directories are modified while this function is executing.

  // Reset dirinfo
  dirinfo->files.clear();
  dirinfo->empty_dirs.clear();
  dirinfo->size_bytes = 0;
  dirinfo->inode_count = 0;

  StringVector dirs_to_traverse;
  dirs_to_traverse.push_back(path.as_string());
  while (!dirs_to_traverse.empty()) {
    notifier->Notify();
    GoogleString dir = dirs_to_traverse.back();
    dirs_to_traverse.pop_back();
    StringVector dir_contents;
    bool is_ok = ListContents(dir, &dir_contents, handler);
    if (!is_ok) {
      continue;
    }

    // Save empty directories to remove if we have to clean.
    if (dir_contents.empty()) {
      dirinfo->empty_dirs.push_back(dir);
      continue;
    }

    // Add files in directory to our vector of files and subdirs to our vector
    // of directories to traverse.
    dirinfo->inode_count += dir_contents.size();
    StringVector::iterator it;
    for (it = dir_contents.begin(); it != dir_contents.end(); ++it) {
      notifier->Notify();
      GoogleString file_name = *it;
      // Add size for both files and directories
      int64 file_size;
      Size(file_name, &file_size, handler);
      dirinfo->size_bytes += file_size;
      BoolOrError is_dir = IsDir(file_name.c_str(), handler);
      if (is_dir.is_false()) {
        int64 file_atime;
        Atime(file_name, &file_atime, handler);
        dirinfo->files.push_back(FileInfo(file_size, file_atime, file_name));
      } else if (is_dir.is_true()) {
        dirs_to_traverse.push_back(file_name);
      }
    }
  }
}

// Try to make directories to store file.
void FileSystem::SetupFileDir(const StringPiece& filename,
                              MessageHandler* handler) {
  size_t last_slash = filename.rfind('/');
  if (last_slash != StringPiece::npos) {
    StringPiece directory_name = filename.substr(0, last_slash);
    if (!RecursivelyMakeDir(directory_name, handler)) {
      // TODO(sligocki): Specify where dir creation failed?
      handler->Message(kError, "Could not create directories for file %s",
                       filename.as_string().c_str());
    }
  }
}

}  // namespace net_instaweb
