load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
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
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-const-variable",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html:html",
        "//pagespeed/kernel/js:js",
        #       "//pagespeed/kernel/cache",
        #       "//pagespeed/opt/logging",
    ],
)

envoy_cc_test_library(
    name = "ads_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-const-variable",
    ],
    repository = "@envoy",
    deps = [
        ":ads",
        #        "//pagespeed/kernel/base:kernel_test_util",
        #        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)

envoy_cc_test(
    name = "ads_test",
    #    srcs = glob(["*_test.cc"]),
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-const-variable",
    ],
    repository = "@envoy",
    deps = [
        #        ":ads_test_base",
    ],
)
