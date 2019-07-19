// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/mime_util_xdg.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/third_party/xdg_mime/xdgmime.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {
namespace nix {

namespace {

// None of the XDG stuff is thread-safe, so serialize all access under
// this lock.
LazyInstance<Lock>::Leaky g_mime_util_xdg_lock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

std::string GetFileMimeType(const FilePath& filepath) {
  if (filepath.empty())
    return std::string();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  AutoLock scoped_lock(g_mime_util_xdg_lock.Get());
  return xdg_mime_get_mime_type_from_file_name(filepath.value().c_str());
}

}  // namespace nix
}  // namespace base
