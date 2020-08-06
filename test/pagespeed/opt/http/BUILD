load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = [
        "mock_property_page.cc",
    ],
    hdrs = [
        "mock_property_page.h",
    ],
    deps = [
        "//pagespeed/opt/http",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "http",
    srcs = glob(["*_test.cc"]),
    deps = [":test_base"],
)
