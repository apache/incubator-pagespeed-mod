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
    linkstatic = True,
    deps = [
        "//net/instaweb/http",
        "//net/instaweb/rewriter",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/thread",
    ],
)

pagespeed_cc_test_library(
    name = "automatic_test_base",
    srcs = [
        "proxy_interface_test_base.cc",
    ],
    hdrs = [
        "proxy_interface_test_base.h",
    ],
    deps = [
        ":automatic",
        "//net/instaweb/rewriter:rewriter_test_base",
        "//pagespeed/kernel/base:benchmark",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "automatic_test",
    srcs = [
        "html_detector_test.cc",
        "proxy_fetch_test.cc",
        "proxy_interface_test.cc",
    ],
    data = ["//net/instaweb/rewriter:testdata"],
    deps = [
        ":automatic_test_base",
    ],
)
