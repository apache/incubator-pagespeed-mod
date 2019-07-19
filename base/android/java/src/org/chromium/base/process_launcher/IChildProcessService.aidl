// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.os.Bundle;

import org.chromium.base.process_launcher.IParentProcess;

interface IChildProcessService {
  // On the first call to this method, the service will record the calling PID
  // and return true. Subsequent calls will only return true if the calling PID
  // is the same as the recorded one.
  boolean bindToCaller();

  // Sets up the initial IPC channel.
  oneway void setupConnection(in Bundle args, IParentProcess parentProcess,
          in List<IBinder> clientInterfaces);

  // Forcefully kills the child process.
  oneway void forceKill();

  // Notifies about memory pressure. The argument is MemoryPressureLevel enum.
  oneway void onMemoryPressure(int pressure);

  // Dumps the stack for the child process without crashing it.
  oneway void dumpProcessStack();
}
