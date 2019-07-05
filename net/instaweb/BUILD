licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "net_instaweb_lib",
    srcs = [
        "foo.cc", # for test
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
)
