load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "benchmark",
    hdrs = ["benchmark.h"],
    deps = ["@re2//:benchmark"],
)
