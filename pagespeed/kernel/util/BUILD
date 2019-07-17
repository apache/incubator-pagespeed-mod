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
    name = "util",
    srcs = [
        "statistics_logger.cc",
        #"url_to_filename_encoder.cc",
        "platform.cc",
    ],
    hdrs = [
        "statistics_logger.h",
        "url_to_filename_encoder.h",
        "platform.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "@glog",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html:html",
        "//pagespeed/kernel/http:http",
        "//pagespeed/kernel/thread:thread",
    ],
)

envoy_cc_test_library (
    name = "util_test_base",
    srcs = [
        "simple_stats.cc",
    ],
    hdrs = [
        "simple_stats.h",
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":util",
    ]
)

envoy_cc_test(
    name = "util_test",
    repository = "@envoy",
    copts = ["-Wno-error=unused-private-field", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":util_test_base"
    ],
)