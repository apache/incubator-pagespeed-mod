load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")
load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

licenses(["notice"])  # Apache 2

proto_library(
    name = "controller_proto",
    srcs = ["controller.proto"],
)

cc_proto_library(
    name = "controller_proto_cc",
    deps = [":controller_proto"],
)

cc_grpc_library(
    name = "controller_proto_grpc",
    srcs = [
        ":controller_proto",
    ],
    grpc_only = True,
    proto_only = False,
    use_external = False,
    visibility = ["//visibility:public"],
    well_known_protos = False,
    deps = [
        ":controller_proto_cc",
    ],
)

cc_library(
    name = "controller",
    srcs = [
        "central_controller.cc",
        "central_controller_rpc_client.cc",
        "central_controller_rpc_server.cc",
        "compatible_central_controller.cc",
        "expensive_operation_callback.cc",
        "expensive_operation_rpc_context.cc",
        "expensive_operation_rpc_handler.cc",
        "in_process_central_controller.cc",
        "named_lock_schedule_rewrite_controller.cc",
        "popularity_contest_schedule_rewrite_controller.cc",
        "queued_expensive_operation_controller.cc",
        "schedule_rewrite_callback.cc",
        "schedule_rewrite_rpc_context.cc",
        "schedule_rewrite_rpc_handler.cc",
        "work_bound_expensive_operation_controller.cc",
    ],
    hdrs = [
        "central_controller.h",
        "central_controller_callback.h",
        "central_controller_rpc_client.h",
        "central_controller_rpc_server.h",
        "compatible_central_controller.h",
        "context_registry.h",
        "expensive_operation_callback.h",
        "expensive_operation_controller.h",
        "expensive_operation_rpc_context.h",
        "expensive_operation_rpc_handler.h",
        "in_process_central_controller.h",
        "named_lock_schedule_rewrite_controller.h",
        "popularity_contest_schedule_rewrite_controller.h",
        "priority_queue.h",
        "queued_expensive_operation_controller.h",
        "request_result_rpc_client.h",
        "request_result_rpc_handler.h",
        "rpc_handler.h",
        "schedule_rewrite_callback.h",
        "schedule_rewrite_controller.h",
        "schedule_rewrite_rpc_context.h",
        "schedule_rewrite_rpc_handler.h",
        "work_bound_expensive_operation_controller.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":controller_proto_grpc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/thread",
        "//pagespeed/kernel/util",
        "//pagespeed/system:system_controller_process_h",
    ],
)
