// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sequence_checker.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/sequence_token.h"
#include "base/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Runs a callback on another thread.
class RunCallbackThread : public SimpleThread {
 public:
  explicit RunCallbackThread(OnceClosure callback)
      : SimpleThread("RunCallbackThread"), callback_(std::move(callback)) {
    Start();
    Join();
  }

 private:
  // SimpleThread:
  void Run() override { std::move(callback_).Run(); }

  OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(RunCallbackThread);
};

void ExpectCalledOnValidSequence(SequenceCheckerImpl* sequence_checker) {
  ASSERT_TRUE(sequence_checker);

  // This should bind |sequence_checker| to the current sequence if it wasn't
  // already bound to a sequence.
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());

  // Since |sequence_checker| is now bound to the current sequence, another call
  // to CalledOnValidSequence() should return true.
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
}

void ExpectCalledOnValidSequenceWithSequenceToken(
    SequenceCheckerImpl* sequence_checker,
    SequenceToken sequence_token) {
  ScopedSetSequenceTokenForCurrentThread
      scoped_set_sequence_token_for_current_thread(sequence_token);
  ExpectCalledOnValidSequence(sequence_checker);
}

void ExpectNotCalledOnValidSequence(SequenceCheckerImpl* sequence_checker) {
  ASSERT_TRUE(sequence_checker);
  EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
}

}  // namespace

TEST(SequenceCheckerTest, CallsAllowedOnSameThreadNoSequenceToken) {
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, CallsAllowedOnSameThreadSameSequenceToken) {
  ScopedSetSequenceTokenForCurrentThread
      scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerTest, CallsDisallowedOnDifferentThreadsNoSequenceToken) {
  SequenceCheckerImpl sequence_checker;
  RunCallbackThread thread(
      BindOnce(&ExpectNotCalledOnValidSequence, Unretained(&sequence_checker)));
}

TEST(SequenceCheckerTest, CallsAllowedOnDifferentThreadsSameSequenceToken) {
  const SequenceToken sequence_token(SequenceToken::Create());

  ScopedSetSequenceTokenForCurrentThread
      scoped_set_sequence_token_for_current_thread(sequence_token);
  SequenceCheckerImpl sequence_checker;
  EXPECT_TRUE(sequence_checker.CalledOnValidSequence());

  RunCallbackThread thread(
      BindOnce(&ExpectCalledOnValidSequenceWithSequenceToken,
               Unretained(&sequence_checker), sequence_token));
}

TEST(SequenceCheckerTest, CallsDisallowedOnSameThreadDifferentSequenceToken) {
  std::unique_ptr<SequenceCheckerImpl> sequence_checker;

  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    sequence_checker.reset(new SequenceCheckerImpl);
  }

  {
    // Different SequenceToken.
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
  }

  // No SequenceToken.
  EXPECT_FALSE(sequence_checker->CalledOnValidSequence());
}

TEST(SequenceCheckerTest, DetachFromSequence) {
  std::unique_ptr<SequenceCheckerImpl> sequence_checker;

  {
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    sequence_checker.reset(new SequenceCheckerImpl);
  }

  sequence_checker->DetachFromSequence();

  {
    // Verify that CalledOnValidSequence() returns true when called with
    // a different sequence token after a call to DetachFromSequence().
    ScopedSetSequenceTokenForCurrentThread
        scoped_set_sequence_token_for_current_thread(SequenceToken::Create());
    EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  }
}

TEST(SequenceCheckerTest, DetachFromSequenceNoSequenceToken) {
  SequenceCheckerImpl sequence_checker;
  sequence_checker.DetachFromSequence();

  // Verify that CalledOnValidSequence() returns true when called on a
  // different thread after a call to DetachFromSequence().
  RunCallbackThread thread(
      BindOnce(&ExpectCalledOnValidSequence, Unretained(&sequence_checker)));

  EXPECT_FALSE(sequence_checker.CalledOnValidSequence());
}

TEST(SequenceCheckerMacroTest, Macros) {
  auto scope = std::make_unique<ScopedSetSequenceTokenForCurrentThread>(
      SequenceToken::Create());
  SEQUENCE_CHECKER(my_sequence_checker);

  // Don't expect a DCHECK death when a SequenceChecker is used on the right
  // sequence.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker) << "Error message.";

  scope.reset();

#if DCHECK_IS_ON()
  // Expect DCHECK death when used on a different sequence.
  EXPECT_DCHECK_DEATH({
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker) << "Error message.";
  });
#else
    // Happily no-ops on non-dcheck builds.
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker) << "Error message.";
#endif

  DETACH_FROM_SEQUENCE(my_sequence_checker);

  // Don't expect a DCHECK death when a SequenceChecker is used for the first
  // time after having been detached.
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker) << "Error message.";
}

}  // namespace base
