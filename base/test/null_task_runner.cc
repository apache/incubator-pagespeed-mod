// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/null_task_runner.h"

namespace base {

NullTaskRunner::NullTaskRunner() = default;

NullTaskRunner::~NullTaskRunner() = default;

bool NullTaskRunner::PostDelayedTask(const Location& from_here,
                                     OnceClosure task,
                                     base::TimeDelta delay) {
  return false;
}

bool NullTaskRunner::PostNonNestableDelayedTask(const Location& from_here,
                                                OnceClosure task,
                                                base::TimeDelta delay) {
  return false;
}

bool NullTaskRunner::RunsTasksInCurrentSequence() const {
  return true;
}

}  // namespace base
