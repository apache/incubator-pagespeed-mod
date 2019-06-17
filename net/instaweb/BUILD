licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "net_instaweb_lib",
    hdrs = [
        "public/global_constants.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
)
