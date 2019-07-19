// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.base;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.SparseArray;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.process_launcher.ChildProcessServiceDelegate;
import org.chromium.native_test.MainRunner;

import java.util.List;

/** Implementation of the ChildProcessServiceDelegate used for the Multiprocess tests. */
public class MultiprocessTestClientServiceDelegate implements ChildProcessServiceDelegate {
    private static final String TAG = "MPTestCSDelegate";

    private ITestCallback mTestCallback;

    private final ITestController.Stub mTestController = new ITestController.Stub() {
        @Override
        public boolean forceStopSynchronous(int exitCode) {
            System.exit(exitCode);
            return true;
        }

        @Override
        public void forceStop(int exitCode) {
            System.exit(exitCode);
        }
    };

    @Override
    public void onServiceCreated() {
        PathUtils.setPrivateDataDirectorySuffix("chrome_multiprocess_test_client_service");
    }

    @Override
    public void onServiceBound(Intent intent) {}

    @Override
    public void onConnectionSetup(Bundle connectionBundle, List<IBinder> callbacks) {
        mTestCallback = ITestCallback.Stub.asInterface(callbacks.get(0));
    }

    @Override
    public void preloadNativeLibrary(Context hostContext) {
        LibraryLoader.getInstance().preloadNow();
    }

    @Override
    public boolean loadNativeLibrary(Context hostContext) {
        try {
            LibraryLoader.getInstance().loadNow();
            return true;
        } catch (ProcessInitException pie) {
            Log.e(TAG, "Unable to load native libraries.", pie);
            return false;
        }
    }

    @Override
    public SparseArray<String> getFileDescriptorsIdsToKeys() {
        return null;
    }

    @Override
    public void onBeforeMain() {
        try {
            mTestCallback.childConnected(mTestController);
        } catch (RemoteException re) {
            Log.e(TAG, "Failed to notify parent process of connection.");
        }
    }

    @Override
    public void runMain() {
        int result = MainRunner.runMain(CommandLine.getJavaSwitchesOrNull());
        try {
            mTestCallback.mainReturned(result);
        } catch (RemoteException re) {
            Log.e(TAG, "Failed to notify parent process of main returning.");
        }
    }
}
