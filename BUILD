licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_package",
)

envoy_package()

envoy_cc_binary(
    name = "mod_pagespeed",
    repository = "@envoy",
    deps = [
        "//net/instaweb:net_instaweb_lib",
        "//third_party:all_third_party", # for test
    ],
)
