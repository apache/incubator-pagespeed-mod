load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2


pagespeed_cc_test_library(
    name = "controller_test_base",
    srcs = [
        "grpc_server_test.cc",
    ],
    hdrs = [
        "controller_grpc_mocks.h",
        "grpc_server_test.h",
    ],
    deps = [
        "//pagespeed/controller",
        "//test/pagespeed/kernel/thread:thread_test_base",
        "//pagespeed/kernel/util",
    ],
)

proto_library(
    name = "grpc_test_proto",
    srcs = ["grpc_test.proto"],
)

cc_proto_library(
    name = "grpc_test_proto_cc",
    deps = [":grpc_test_proto"],
)

cc_grpc_library(
    name = "grpc_test_proto_grpc",
    srcs = [
        ":grpc_test_proto",
    ],
    grpc_only = True,
    proto_only = False,
    use_external = False,
    visibility = ["//visibility:public"],
    well_known_protos = False,
    deps = [
        ":grpc_test_proto_cc",
    ],
)

pagespeed_cc_test(
    name = "controller_test",
    srcs = [
        "central_controller_callback_test.cc",
        "context_registry_test.cc",
        "expensive_operation_rpc_context_test.cc",
        "expensive_operation_rpc_handler_test.cc",
        "named_lock_schedule_rewrite_controller_test.cc",
        "popularity_contest_schedule_rewrite_controller_test.cc",
        "priority_queue_test.cc",
        "queued_expensive_operation_controller_test.cc",
        "rpc_handler_test.cc",
        "schedule_rewrite_rpc_context_test.cc",
        "schedule_rewrite_rpc_handler_test.cc",
        "work_bound_expensive_operation_controller_test.cc",
    ],
    shard_count = 1,
    deps = [
        ":controller_test_base",
        ":grpc_test_proto_grpc",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)
