// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;

import org.chromium.base.test.params.ParameterizedRunner.ParameterizedTestInstantiationException;

import java.util.List;

/**
 * Parameterized class runner delegate that extends BlockJUnit4ClassRunner
 */
public final class BlockJUnit4RunnerDelegate
        extends BlockJUnit4ClassRunner implements ParameterizedRunnerDelegate {
    private ParameterizedRunnerDelegateCommon mDelegateCommon;

    public BlockJUnit4RunnerDelegate(Class<?> klass,
            ParameterizedRunnerDelegateCommon delegateCommon) throws InitializationError {
        super(klass);
        mDelegateCommon = delegateCommon;
    }

    @Override
    public void collectInitializationErrors(List<Throwable> errors) {
        ParameterizedRunnerDelegateCommon.collectInitializationErrors(errors);
    }

    @Override
    public List<FrameworkMethod> computeTestMethods() {
        return mDelegateCommon.computeTestMethods();
    }

    @Override
    public Object createTest() throws ParameterizedTestInstantiationException {
        return mDelegateCommon.createTest();
    }
}
