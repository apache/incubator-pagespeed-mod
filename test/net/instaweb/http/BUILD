load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "http_test_base",
    srcs = glob(
        ["*.cc"],
        exclude = ["*_test.cc"],
    ),
    hdrs = glob(["*.h"]),
    deps = [
        "//net/instaweb/http",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/sharedmem:test_base",
        "//test/pagespeed/kernel/thread:thread_test_base",
    ],
)

pagespeed_cc_test(
    name = "http",
    srcs = glob(["*_test.cc"]),
    data = glob([
        "testdata/*",
        "testdata/www.google.com/*",
    ]),
    deps = [
        "//test/net/instaweb/http:http_test_base",
    ],
)
