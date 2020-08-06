load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "cache_test_base",
    srcs = glob(
        ["*.cc"],
        exclude = glob(["*_test.cc"]),
    ),
    hdrs = glob(["*.h"]),
    deps = [
        "//pagespeed/kernel/cache",
        "//test/pagespeed/kernel/thread:thread_test_base",
        "//test/pagespeed/kernel/base:pagespeed_gtest",
    ],
)

pagespeed_cc_test(
    name = "cache",
    srcs = glob(["*_test.cc"]),
    deps = ["//test/pagespeed/kernel/cache:cache_test_base"],
)
