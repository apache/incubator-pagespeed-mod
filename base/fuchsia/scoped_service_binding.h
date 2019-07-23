// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
#define BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_

#include <lib/fidl/cpp/binding_set.h>

#include "base/bind.h"
#include "base/fuchsia/service_directory.h"

namespace base {
namespace fuchsia {

template <typename Interface>
class ScopedServiceBinding {
 public:
  // |service_directory| and |impl| must outlive the binding.
  ScopedServiceBinding(ServiceDirectory* service_directory, Interface* impl)
      : directory_(service_directory), impl_(impl) {
    directory_->AddService(
        BindRepeating(&ScopedServiceBinding::BindClient, Unretained(this)));
  }

  ~ScopedServiceBinding() { directory_->RemoveService(Interface::Name_); }

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    bindings_.set_empty_set_handler(
        fit::bind_member(this, &ScopedServiceBinding::OnBindingSetEmpty));
  }

  bool has_clients() const { return bindings_.size() != 0; }

 private:
  void BindClient(fidl::InterfaceRequest<Interface> request) {
    bindings_.AddBinding(impl_, std::move(request));
  }

  void OnBindingSetEmpty() {
    bindings_.set_empty_set_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  ServiceDirectory* const directory_;
  Interface* const impl_;
  fidl::BindingSet<Interface> bindings_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedServiceBinding);
};

// Scoped service binding which allows only a single client to be connected
// at any time. By default a new connection will disconnect an existing client.
enum class ScopedServiceBindingPolicy { kPreferNew, kPreferExisting };

template <typename Interface,
          ScopedServiceBindingPolicy Policy =
              ScopedServiceBindingPolicy::kPreferNew>
class ScopedSingleClientServiceBinding {
 public:
  // |service_directory| and |impl| must outlive the binding.
  ScopedSingleClientServiceBinding(ServiceDirectory* service_directory,
                                   Interface* impl)
      : directory_(service_directory), binding_(impl) {
    directory_->AddService(BindRepeating(
        &ScopedSingleClientServiceBinding::BindClient, Unretained(this)));
  }

  ~ScopedSingleClientServiceBinding() {
    directory_->RemoveService(Interface::Name_);
  }

  typename Interface::EventSender_& events() { return binding_.events(); }

  void SetOnLastClientCallback(base::OnceClosure on_last_client_callback) {
    on_last_client_callback_ = std::move(on_last_client_callback);
    binding_.set_error_handler(fit::bind_member(
        this, &ScopedSingleClientServiceBinding::OnBindingEmpty));
  }

  bool has_clients() const { return binding_.is_bound(); }

 private:
  void BindClient(fidl::InterfaceRequest<Interface> request) {
    if (Policy == ScopedServiceBindingPolicy::kPreferExisting &&
        binding_.is_bound())
      return;
    binding_.Bind(std::move(request));
  }

  void OnBindingEmpty() {
    binding_.set_error_handler(nullptr);
    std::move(on_last_client_callback_).Run();
  }

  ServiceDirectory* const directory_;
  fidl::Binding<Interface> binding_;
  base::OnceClosure on_last_client_callback_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSingleClientServiceBinding);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SCOPED_SERVICE_BINDING_H_
