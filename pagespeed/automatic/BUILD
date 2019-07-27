licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
    name = "automatic",
    srcs = [
        "html_detector.cc",
        "proxy_fetch.cc",
        "proxy_interface.cc",
#        "rewriter_speed_test.cc",
    ],
    hdrs = [
        "html_detector.h",
        "proxy_fetch.h",
        "proxy_interface.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//net/instaweb/http:http",
        "//net/instaweb/rewriter:rewriter",
        "//pagespeed/kernel/thread:thread",
    ],
)

pagespeed_cc_test_library (
    name = "automatic_test_base",
    srcs = [
        "proxy_interface_test_base.cc",
    ],
    hdrs = [
        "proxy_interface_test_base.h",
    ],
    deps = [
        ":automatic",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
        "//pagespeed/kernel/base:benchmark",
        "//net/instaweb/rewriter:rewriter_test_base",
    ]
)

pagespeed_cc_test(
    name = "automatic_test",
    srcs = [
        "html_detector_test.cc",
        "proxy_interface_test.cc",
        "proxy_fetch_test.cc",
    ],
    deps = [
        ":automatic_test_base",
    ],
    data = ["//net/instaweb/rewriter:testdata"],
)