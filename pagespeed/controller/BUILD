load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

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
    well_known_protos = True,
    deps = [
        ":controller_proto_cc",
    ],
)

envoy_cc_library(
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
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=old-style-cast",  # rdestl
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        #        "//pagespeed/opt/http:http",
        #        "//pagespeed/kernel/http:http",
        ":controller_proto_grpc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/thread",
        "//pagespeed/kernel/util",
        "//pagespeed/system:system",
        "//third_party/rdestl",
    ],
)

envoy_cc_test_library(
    name = "controller_test_base",
    srcs = [
        "grpc_server_test.cc",
    ],
    hdrs = [
        "controller_grpc_mocks.h",
        "grpc_server_test.h",
    ],
    copts = [
        "-Wno-error=unused-parameter",
    ],
    data = [
    ],
    repository = "@envoy",
    deps = [
        ":controller",
        #        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/util",
        "//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/sharedmem:shared_mem_test_base",
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
    well_known_protos = True,
    deps = [
        ":grpc_test_proto_cc",
    ],
)

envoy_cc_test(
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
    copts = [
        "-Wno-error=unused-parameter",
        #        "-Wno-error=unused-private-field",
        #        "-Wno-error=unused-function",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=old-style-cast",  # rdestl
        #        "-Wno-error=missing-field-initializers",
    ],
    repository = "@envoy",
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",        
        ":controller_test_base",
        ":grpc_test_proto_grpc",
    ],
)
