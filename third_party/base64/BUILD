licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "base64",
    srcs = [
        "base64.cc",
    ],
    hdrs = [
        "base64.h",
    ],
    copts = [
        "-Wno-error",
    ],    
    repository = "@envoy",
    visibility = ["//visibility:public"],
)
