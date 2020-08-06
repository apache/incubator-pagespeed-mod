load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = [
        "log_record_test_helper.cc",
    ],
    hdrs = [
        "log_record_test_helper.h",
    ],
    deps = [
        "//pagespeed/opt/logging",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "logging",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":test_base",
    ],
)
