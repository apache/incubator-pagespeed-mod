load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

cc_library(
    name = "automatic",
    srcs = [
        "html_detector.cc",
        "proxy_fetch.cc",
        "proxy_interface.cc",
    ],
    hdrs = [
        "html_detector.h",
        "proxy_fetch.h",
        "proxy_interface.h",
    ],
    linkstatic = True,
    visibility = ["//visibility:public"],
    deps = [
        "//net/instaweb/http",
        "//net/instaweb/util",
        "//net/instaweb/rewriter",
        "//pagespeed/kernel/base:pagespeed_base",
        #"//pagespeed/kernel/thread",
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
        "//pagespeed/kernel/util:util_test_base",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test_library(
    name = "static_rewriter",
    srcs = [
        "static_rewriter.cc",
    ],
    hdrs = [
        "static_rewriter.h",
    ],
    deps = [
        "//pagespeed/system",
    ],
)

pagespeed_cc_test(
    name = "automatic_test",
    srcs = glob(["*_test.cc"]),
    data = ["//net/instaweb/rewriter:testdata"],
    deps = [
        ":automatic_test_base",
    ],
)
