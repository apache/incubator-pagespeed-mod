load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

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

cc_library(
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
    visibility = ["//visibility:public"],
    deps = [
        ":logging_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

pagespeed_cc_test_library(
    name = "logging_test_base",
    srcs = [
        "log_record_test_helper.cc",
    ],
    hdrs = [
        "log_record_test_helper.h",
    ],
    deps = [
        ":logging",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)

pagespeed_cc_test(
    name = "logging_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":logging_test_base",
    ],
)
