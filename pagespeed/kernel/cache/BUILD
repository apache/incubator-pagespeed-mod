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
    name = "cache",
    srcs = [
        #"file_cache.cc",
    ],
    hdrs = [
        "file_cache.h",
        "cache_interface.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "@glog",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/thread:thread",
        "//pagespeed/kernel/util:util",
    ],
)

envoy_cc_test_library (
    name = "cache_test_base",
    srcs = [
        "cache_spammer.cc",
    ],
    hdrs = [
        "cache_spammer.h",
        "cache_test_base.h",
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":cache",
        "//pagespeed/kernel/base:pagespeed_gtest",        
    ]
)

envoy_cc_test(
    name = "cache_test",
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter"],
    deps = [
        ":cache",
        ":cache_test_base"
    ],
)