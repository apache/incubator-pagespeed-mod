// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/logging.h"
//#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace base {
namespace debug {

static bool is_debug_ui_suppressed = false;

// XXX
bool WaitForDebugger(int wait_seconds, bool silent) {
  return false;
}

void SetSuppressDebugUI(bool suppress) {
  is_debug_ui_suppressed = suppress;
}

bool IsDebugUISuppressed() {
  return is_debug_ui_suppressed;
}

}  // namespace debug
}  // namespace base
