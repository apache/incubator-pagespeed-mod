workspace(name = "mod_pagespeed")

load("//bazel:repositories.bzl", "mod_pagespeed_dependencies")

mod_pagespeed_dependencies()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")
load("@envoy//bazel:cc_configure.bzl", "cc_configure")

envoy_dependencies()
cc_configure()

load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

rules_foreign_cc_dependencies()

load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")

go_rules_dependencies()

go_register_toolchains()

