load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "main",
    testonly = True,
    srcs = [
        "main.cc",
    ],
    hdrs = [
        "test_runner.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//test/pagespeed/kernel/base:pagespeed_gtest",
        "@com_google_absl//absl/debugging:symbolize",
    ],
)
