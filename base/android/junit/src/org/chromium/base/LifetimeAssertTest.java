// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * junit tests for {@link LifetimeAssert}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LifetimeAssertTest {
    private static class TestClass {
        // Put assert inside of a test class to mirror typical api usage.
        final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    }

    private final Object mLock = new Object();
    private TestClass mTestClass;
    private LifetimeAssert.WrappedReference mTargetRef;
    private boolean mFound;
    private String mHookMessage;

    @Before
    public void setUp() {
        if (!BuildConfig.DCHECK_IS_ON) {
            return;
        }
        mTestClass = new TestClass();
        mTargetRef = mTestClass.mLifetimeAssert.mWrapper;
        mFound = false;
        mHookMessage = null;
        LifetimeAssert.sTestHook = (ref, msg) -> {
            if (ref == mTargetRef) {
                synchronized (mLock) {
                    mFound = true;
                    mHookMessage = msg;
                    mLock.notify();
                }
            }
        };
    }

    @After
    public void tearDown() {
        if (!BuildConfig.DCHECK_IS_ON) {
            return;
        }
        LifetimeAssert.sTestHook = null;
    }

    private void runTest(boolean setSafe) {
        if (!BuildConfig.DCHECK_IS_ON) {
            return;
        }

        synchronized (mLock) {
            if (setSafe) {
                LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
            }
            // Null out field to make reference GC'able.
            mTestClass = null;
            // Call System.gc() until the background thread notices the reference.
            for (int i = 0; i < 10 && !mFound; ++i) {
                System.gc();
                try {
                    mLock.wait(200);
                } catch (InterruptedException e) {
                }
            }
            Assert.assertTrue(mFound);
            if (setSafe) {
                Assert.assertNull(mHookMessage);
            } else {
                Assert.assertNotNull(mHookMessage);
            }
        }
    }

    @Test
    public void testSafeGc() {
        runTest(true);
    }

    @Test
    public void testUnsafeGc() {
        runTest(false);
    }

    @Test
    public void testAssertAllInstancesDestroyedForTesting() {
        if (!BuildConfig.DCHECK_IS_ON) {
            return;
        }
        try {
            LifetimeAssert.assertAllInstancesDestroyedForTesting();
            Assert.fail();
        } catch (LifetimeAssert.LifetimeAssertException e) {
            // Expected.
        }
        LifetimeAssert.setSafeToGc(mTestClass.mLifetimeAssert, true);
        // Should no longer throw.
        LifetimeAssert.assertAllInstancesDestroyedForTesting();
    }
}
