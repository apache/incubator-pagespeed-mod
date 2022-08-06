load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

package(default_visibility = ["//visibility:public"])

api_cc_py_proto_library(
    name = "http_filter_proto",
    srcs = ["http_filter.proto"],
)

envoy_cc_library(
    name = "http_filter_lib",
    srcs = [
        "envoy_base_fetch.cc",
        "envoy_message_handler.cc",
        "envoy_process_context.cc",
        "envoy_rewrite_driver_factory.cc",
        "envoy_rewrite_options.cc",
        "envoy_server_context.cc",
        "http_filter.cc",
        "log_message_handler.cc",
    ],
    hdrs = [
        "envoy_base_fetch.h",
        "envoy_message_handler.h",
        "envoy_process_context.h",
        "envoy_rewrite_driver_factory.h",
        "envoy_rewrite_options.h",
        "envoy_server_context.h",
        "envoy_thread_system.h",
        "header_utils.h",
        "http_filter.h",
        "log_message_handler.h",
    ],
    copts = [
        "-Wno-unused-parameter",
        "-Wno-old-style-cast",
        "-Wno-macro-redefined",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":http_filter_proto_cc_proto",
        "//pagespeed/automatic",
        "//pagespeed/system",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
    ],
)

envoy_cc_library(
    name = "http_filter_config",
    srcs = ["http_filter_config.cc"],
    copts = [
        "-Wno-unused-parameter",
        "-Wno-old-style-cast",
        "-Wno-macro-redefined",
    ],
    repository = "@envoy",
    deps = [
        ":http_filter_lib",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
    ],
)

envoy_cc_test(
    name = "http_filter_integration_test",
    srcs = ["http_filter_integration_test.cc"],
    repository = "@envoy",
    deps = [
        ":http_filter_config",
        "@envoy//test/integration:http_integration_lib",
    ],
)

filegroup(
    name = "envoy_fetcher_headers",
    srcs = [
        "envoy_cluster_manager.h",
        "envoy_fetch.h",
        "envoy_logger.h",
        "envoy_url_async_fetcher.h",
        "header_utils.h",
        "pagespeed_remote_data_fetcher.h",
    ],
)

envoy_cc_library(
    name = "envoy_fetcher",
    srcs = [
        "envoy_cluster_manager.cc",
        "envoy_fetch.cc",
        "envoy_logger.cc",
        "envoy_url_async_fetcher.cc",
        "pagespeed_remote_data_fetcher.cc",
    ],
    hdrs = [
        ":envoy_fetcher_headers",
    ],
    copts = [
        "-Wno-unused-parameter",
        "-Wno-old-style-cast",
        "-Wno-macro-redefined",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//net/instaweb/http",
        "@envoy//envoy/event:dispatcher_interface_with_external_headers",
        "@envoy//source/common/access_log:access_log_manager_lib_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//source/common/common:assert_lib_with_external_headers",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/common:random_generator_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/grpc:context_lib_with_external_headers",
        "@envoy//source/common/http:context_lib_with_external_headers",
        "@envoy//source/common/http:utility_lib_with_external_headers",
        "@envoy//source/common/http/http1:conn_pool_lib_with_external_headers",
        "@envoy//source/common/init:manager_lib_with_external_headers",
        "@envoy//source/common/local_info:local_info_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/router:context_lib_with_external_headers",
        "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        "@envoy//source/common/secret:secret_manager_impl_lib_with_external_headers",
        "@envoy//source/common/singleton:manager_impl_lib_with_external_headers",
        "@envoy//source/common/stats:allocator_lib_with_external_headers",
        "@envoy//source/common/stats:stats_lib_with_external_headers",
        "@envoy//source/common/stats:thread_local_store_lib_with_external_headers",
        "@envoy//source/common/thread_local:thread_local_lib_with_external_headers",
        "@envoy//source/common/upstream:cluster_manager_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib_with_external_headers",
        "@envoy//source/exe:process_wide_lib_with_external_headers",
        "@envoy//source/extensions/transport_sockets/tls:context_lib_with_external_headers",
        "@envoy//source/server/config_validation:admin_lib_with_external_headers",
        "@envoy//source/server:options_lib_with_external_headers",
        "@envoy//source/server:server_lib_with_external_headers",        
    ],
)

envoy_cc_test(
    name = "envoy_fetcher_test",
    srcs = ["envoy_url_async_fetcher_test.cc"],
    copts = [
        "-Wno-unused-parameter",
        "-Wno-old-style-cast",
        "-Wno-macro-redefined",
    ],
    repository = "@envoy",
    deps = [
        ":envoy_fetcher",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)
