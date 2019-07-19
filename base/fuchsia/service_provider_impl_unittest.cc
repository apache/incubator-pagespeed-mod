// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_provider_impl.h"

#include <lib/zx/channel.h>
#include <utility>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/fuchsia/test_interface_impl.h"
#include "base/fuchsia/testfidl/cpp/fidl.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace fuchsia {

class ServiceProviderImplTest : public testing::Test {
 public:
  ServiceProviderImplTest() = default;
  ~ServiceProviderImplTest() override = default;

  void VerifyTestInterface(fidl::InterfacePtr<testfidl::TestInterface>* stub,
                           zx_status_t expected_error) {
    // Call the service and wait for response.
    base::RunLoop run_loop;
    zx_status_t actual_error = ZX_OK;

    stub->set_error_handler([&run_loop, &actual_error](zx_status_t status) {
      actual_error = status;
      run_loop.Quit();
    });

    (*stub)->Add(2, 2, [&run_loop](int32_t result) {
      EXPECT_EQ(result, 4);
      run_loop.Quit();
    });

    run_loop.Run();

    EXPECT_EQ(expected_error, actual_error);

    // Reset error handler because the current one captures |run_loop| and
    // |error| references which are about to be destroyed.
    stub->set_error_handler(nullptr);
  }

 protected:
  MessageLoopForIO message_loop_;
  TestInterfaceImpl test_service_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProviderImplTest);
};

// Verifies that we can connect to the service service more than once.
TEST_F(ServiceProviderImplTest, ConnectMulti) {
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory_channel;
  ServiceDirectory service_directory(directory_channel.NewRequest());
  ServiceProviderImpl provider_impl(std::move(directory_channel));
  ScopedServiceBinding<testfidl::TestInterface> service_binding(
      &service_directory, &test_service_);

  ::fuchsia::sys::ServiceProviderPtr provider_client;
  provider_impl.AddBinding(provider_client.NewRequest());

  testfidl::TestInterfacePtr stub;
  provider_client->ConnectToService(testfidl::TestInterface::Name_,
                                    stub.NewRequest().TakeChannel());

  testfidl::TestInterfacePtr stub2;
  provider_client->ConnectToService(testfidl::TestInterface::Name_,
                                    stub2.NewRequest().TakeChannel());

  VerifyTestInterface(&stub, ZX_OK);
  VerifyTestInterface(&stub2, ZX_OK);
}

// Verify that the case when the service doesn't exist is handled properly.
TEST_F(ServiceProviderImplTest, NoService) {
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory_channel;
  ServiceDirectory service_directory(directory_channel.NewRequest());
  ServiceProviderImpl provider_impl(std::move(directory_channel));

  ::fuchsia::sys::ServiceProviderPtr provider_client;
  provider_impl.AddBinding(provider_client.NewRequest());

  testfidl::TestInterfacePtr stub;
  provider_client->ConnectToService(testfidl::TestInterface::Name_,
                                    stub.NewRequest().TakeChannel());

  VerifyTestInterface(&stub, ZX_ERR_PEER_CLOSED);
}

}  // namespace fuchsia
}  // namespace base
