licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

genrule(
    name = "html_name_gperf",
    srcs = ["html_name.gperf"],
    outs = ["html_name.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

envoy_cc_library(
    name = "html",
    srcs = [
        "html_keywords.cc",
        ":html_name_gperf",
    ],
    hdrs = [
        "html_keywords.h",
        "html_name.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter"],
    deps = [
        "@glog",
        "@mod_pagespeed//base:log_shim",
        "//pagespeed/kernel/base:pagespeed_base",
#        "//net/instaweb/genfiles:htmlparse",
    ],
)

envoy_cc_test_library (
    name = "html_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":html",
    ]
)

envoy_cc_test(
    name = "html_test",
    repository = "@envoy",
    copts = ["-Wno-error=unused-private-field", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":html",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":html_test_base"
    ],
)