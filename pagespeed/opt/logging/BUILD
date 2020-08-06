load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

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
