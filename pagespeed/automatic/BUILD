licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
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
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
#        "//gurl:url",
        "//pagespeed/kernel/base:pagespeed_base",
        "//net/instaweb/http:http",
        #"//net/instaweb/util:util",
        "//pagespeed/kernel/thread:thread",
#        "//pagespeed/kernel/util:util",
#        "//third_party/rdestl:rdestl",
    ],
)

envoy_cc_test_library (
    name = "automatic_test_base",
    srcs = [
        "proxy_interface_test_base.cc",
    ],
    hdrs = [
        "proxy_interface_test_base.h",
    ],
    repository = "@envoy",
    copts = [
        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":automatic",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
        "//pagespeed/kernel/base:benchmark",
    ]
)

envoy_cc_test(
    name = "automatic_test",
    repository = "@envoy",
    srcs = [
        "html_detector_test.cc",
        "proxy_interface_test.cc",
    ],
    copts = [
#        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
#        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":automatic_test_base",
    ],
)