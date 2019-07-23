load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

proto_library(
    name = "logging_proto",
    srcs = [
        "enums.proto",
        "logging.proto",
    ],
    deps = [
        "//pagespeed/kernel/http:http_proto",
    ],
)

cc_proto_library(
    name = "logging_proto_cc",
    deps = [
        ":logging_proto",
    ],
)

envoy_cc_library(
    name = "logging",
    srcs = [
        "log_record.cc",
        "request_timing_info.cc",
    ],
    hdrs = [
        "log_record.h",
        "logging_proto.h",
        "logging_proto_impl.h",
        "request_timing_info.h",
    ],
    copts = [
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":logging_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

envoy_cc_test_library(
    name = "logging_test_base",
    srcs = [
        "log_record_test_helper.cc",
    ],
    hdrs = [
        "log_record_test_helper.h",
    ],
    copts = [
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        ":logging",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)

envoy_cc_test(
    name = "logging_test",
    srcs = glob(["*_test.cc"]),
    copts = [
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=unused-parameter",
    ],
    repository = "@envoy",
    deps = [
        ":logging_test_base",
    ],
)
