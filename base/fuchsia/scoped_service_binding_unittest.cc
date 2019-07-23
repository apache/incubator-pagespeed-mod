// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/scoped_service_binding.h"

#include "base/fuchsia/service_directory_test_base.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace fuchsia {

class ScopedServiceBindingTest : public ServiceDirectoryTestBase {};

// Verifies that ScopedServiceBinding allows connection more than once.
TEST_F(ScopedServiceBindingTest, ConnectTwice) {
  auto stub = public_service_directory_client_
                  ->ConnectToService<testfidl::TestInterface>();
  auto stub2 = public_service_directory_client_
                   ->ConnectToService<testfidl::TestInterface>();
  VerifyTestInterface(&stub, ZX_OK);
  VerifyTestInterface(&stub2, ZX_OK);
}

// Verify that if we connect twice to a prefer-new bound service, the existing
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferNew) {
  // Teardown the default multi-client binding and create a prefer-new one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferNew>
      binding(service_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client = public_service_directory_client_
                             ->ConnectToService<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client = public_service_directory_client_
                        ->ConnectToService<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  VerifyTestInterface(&new_client, ZX_OK);
}

// Verify that if we connect twice to a prefer-existing bound service, the new
// connection gets closed.
TEST_F(ScopedServiceBindingTest, SingleClientPreferExisting) {
  // Teardown the default multi-client binding and create a prefer-existing one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface,
                                   ScopedServiceBindingPolicy::kPreferExisting>
      binding(service_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client = public_service_directory_client_
                             ->ConnectToService<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, then verify that the it gets closed and the
  // existing one remains functional.
  auto new_client = public_service_directory_client_
                        ->ConnectToService<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(new_client);
  VerifyTestInterface(&existing_client, ZX_OK);
}

// Verify that the default single-client binding policy is prefer-new.
TEST_F(ScopedServiceBindingTest, SingleClientDefaultIsPreferNew) {
  // Teardown the default multi-client binding and create a prefer-new one.
  service_binding_ = nullptr;
  ScopedSingleClientServiceBinding<testfidl::TestInterface> binding(
      service_directory_.get(), &test_service_);

  // Connect the first client, and verify that it is functional.
  auto existing_client = public_service_directory_client_
                             ->ConnectToService<testfidl::TestInterface>();
  VerifyTestInterface(&existing_client, ZX_OK);

  // Connect the second client, so the existing one should be disconnected and
  // the new should be functional.
  auto new_client = public_service_directory_client_
                        ->ConnectToService<testfidl::TestInterface>();
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(existing_client);
  VerifyTestInterface(&new_client, ZX_OK);
}

}  // namespace fuchsia
}  // namespace base
