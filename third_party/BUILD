licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "all_third_party",
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = ["//third_party/base64:base64", "@brotli//:brotli_common"]
)
