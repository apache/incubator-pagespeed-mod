load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

cc_library(
    name = "ads",
    srcs = [
        "ads_attribute.cc",
        "ads_util.cc",
        "show_ads_snippet_parser.cc",
    ],
    hdrs = [
        "ads_attribute.h",
        "ads_util.h",
        "show_ads_snippet_parser.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html:html",
        "//pagespeed/kernel/js:js",
    ],
)

pagespeed_cc_test_library(
    name = "ads_test_base",
    deps = [
        ":ads",
    ],
)

pagespeed_cc_test(
    name = "ads_test",
)
