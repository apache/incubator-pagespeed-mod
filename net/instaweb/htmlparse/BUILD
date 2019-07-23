licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "htmlparse",
    srcs = [
        "file_driver.cc",
        "file_statistics_log.cc",
        "logging_html_filter.cc",
        "statistics_log.cc",
    ],
    hdrs = [
        "public/file_driver.h",
        "public/file_statistics_log.h",
        "public/logging_html_filter.h",
        "public/statistics_log.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=old-style-cast",
        "-Wno-error=sign-compare",
        "-Wno-error=inconsistent-missing-override",
    ],
    deps = [
        "//pagespeed/kernel/html:html",
    ],
)
