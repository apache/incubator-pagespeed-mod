load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = ["proxy_interface_test_base.cc"],
    hdrs = ["proxy_interface_test_base.h"],
    deps = [
        "//pagespeed/automatic",
        "//test/net/instaweb/rewriter:test_base",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "automatic",
    srcs = glob(["*_test.cc"]),
    data = ["//test/net/instaweb/rewriter:testdata"],
    deps = [
        "//test/pagespeed/automatic:test_base",
    ],
)
