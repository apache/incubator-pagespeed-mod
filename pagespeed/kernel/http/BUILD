licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "http",
    srcs = [
        #"google_url.cc",
    ],
    hdrs = [
        #"google_url.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "@glog",
        "//pagespeed/kernel/base:pagespeed_base",
        #"@url//:url",
    ],
)

envoy_cc_test_library (
    name = "http_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":http",
    ]
)

envoy_cc_test(
    name = "http_test",
    repository = "@envoy",
    copts = ["-Wno-error=unused-private-field", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":http",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":http_test_base"
    ],
)