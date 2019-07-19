// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_tracing.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <errno.h>
#endif

namespace base {

File::Info::Info()
    : size(0),
      is_directory(false),
      is_symbolic_link(false) {
}

File::Info::~Info() = default;

File::File()
    : error_details_(FILE_ERROR_FAILED),
      created_(false),
      async_(false) {
}

#if !defined(OS_NACL)
File::File(const FilePath& path, uint32_t flags)
    : error_details_(FILE_OK), created_(false), async_(false) {
  Initialize(path, flags);
}
#endif

File::File(PlatformFile platform_file) : File(platform_file, false) {}

File::File(PlatformFile platform_file, bool async)
    : file_(platform_file),
      error_details_(FILE_OK),
      created_(false),
      async_(async) {
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  DCHECK_GE(platform_file, -1);
#endif
}

File::File(Error error_details)
    : error_details_(error_details),
      created_(false),
      async_(false) {
}

File::File(File&& other)
    : file_(other.TakePlatformFile()),
      tracing_path_(other.tracing_path_),
      error_details_(other.error_details()),
      created_(other.created()),
      async_(other.async_) {}

File::~File() {
  // Go through the AssertIOAllowed logic.
  Close();
}

File& File::operator=(File&& other) {
  Close();
  SetPlatformFile(other.TakePlatformFile());
  tracing_path_ = other.tracing_path_;
  error_details_ = other.error_details();
  created_ = other.created();
  async_ = other.async_;
  return *this;
}

#if !defined(OS_NACL)
void File::Initialize(const FilePath& path, uint32_t flags) {
  if (path.ReferencesParent()) {
#if defined(OS_WIN)
    ::SetLastError(ERROR_ACCESS_DENIED);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
    errno = EACCES;
#else
#error Unsupported platform
#endif
    error_details_ = FILE_ERROR_ACCESS_DENIED;
    return;
  }
  if (FileTracing::IsCategoryEnabled())
    tracing_path_ = path;
  SCOPED_FILE_TRACE("Initialize");
  DoInitialize(path, flags);
}
#endif

std::string File::ErrorToString(Error error) {
  switch (error) {
    case FILE_OK:
      return "FILE_OK";
    case FILE_ERROR_FAILED:
      return "FILE_ERROR_FAILED";
    case FILE_ERROR_IN_USE:
      return "FILE_ERROR_IN_USE";
    case FILE_ERROR_EXISTS:
      return "FILE_ERROR_EXISTS";
    case FILE_ERROR_NOT_FOUND:
      return "FILE_ERROR_NOT_FOUND";
    case FILE_ERROR_ACCESS_DENIED:
      return "FILE_ERROR_ACCESS_DENIED";
    case FILE_ERROR_TOO_MANY_OPENED:
      return "FILE_ERROR_TOO_MANY_OPENED";
    case FILE_ERROR_NO_MEMORY:
      return "FILE_ERROR_NO_MEMORY";
    case FILE_ERROR_NO_SPACE:
      return "FILE_ERROR_NO_SPACE";
    case FILE_ERROR_NOT_A_DIRECTORY:
      return "FILE_ERROR_NOT_A_DIRECTORY";
    case FILE_ERROR_INVALID_OPERATION:
      return "FILE_ERROR_INVALID_OPERATION";
    case FILE_ERROR_SECURITY:
      return "FILE_ERROR_SECURITY";
    case FILE_ERROR_ABORT:
      return "FILE_ERROR_ABORT";
    case FILE_ERROR_NOT_A_FILE:
      return "FILE_ERROR_NOT_A_FILE";
    case FILE_ERROR_NOT_EMPTY:
      return "FILE_ERROR_NOT_EMPTY";
    case FILE_ERROR_INVALID_URL:
      return "FILE_ERROR_INVALID_URL";
    case FILE_ERROR_IO:
      return "FILE_ERROR_IO";
    case FILE_ERROR_MAX:
      break;
  }

  NOTREACHED();
  return "";
}

}  // namespace base
