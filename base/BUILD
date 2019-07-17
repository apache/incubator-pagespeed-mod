licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

cc_library(
    name = "log_shim",
    hdrs = [
        "logging.h",
        "component_export.h",
        "export_template.h",
        "compiler_specific.h",
        "debug/alias.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@glog",
    ],
)

cc_library(
    name = "string_util_shim",
    hdrs = [
        "strings/string_util.h",
        "strings/string16.h",
        "strings/string_piece.h",
    ],
    deps = ["//pagespeed/kernel/base:pagespeed_base"],
    visibility = ["//visibility:public"],
)

