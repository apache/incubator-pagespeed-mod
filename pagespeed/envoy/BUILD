load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_proto_library")

package(default_visibility = ["//visibility:public"])

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    linkopts = [
        "-lrt",
    ],
    deps = [
        ":http_filter_config",
        ":envoy_fetcher",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

api_proto_library(
    name = "http_filter_proto",
    srcs = ["http_filter.proto"],
)

envoy_cc_library(
    name = "http_filter_lib",
    srcs = [
        "envoy_base_fetch.cc",
        "envoy_message_handler.cc",
        "envoy_rewrite_driver_factory.cc",
        "envoy_rewrite_options.cc",
        "envoy_server_context.cc",
        "envoy_process_context.cc",
        "http_filter.cc",
        "log_message_handler.cc",
    ],
    hdrs = [
        "envoy_base_fetch.h",
        "envoy_message_handler.h",
        "envoy_rewrite_driver_factory.h",
        "envoy_rewrite_options.h",
        "envoy_server_context.h",
        "envoy_process_context.h",
        "envoy_thread_system.h",
        "http_filter.h",
        "log_message_handler.h",
        "header_utils.h",
    ],
    copts = [
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
        "-Wno-error=unused-private-field",
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=macro-redefined",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=unused-function",
        "-Wno-error=thread-safety-attributes",
        "-Wno-error=thread-safety-analysis",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":http_filter_proto_cc",
        "//base",
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
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=macro-redefined",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=thread-safety-attributes",
    ],
    repository = "@envoy",
    deps = [
        ":http_filter_lib",
        "@envoy//include/envoy/server:filter_config_interface",
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
        "envoy_url_async_fetcher.h",
        "envoy_cluster_manager.h",
        "pagespeed_remote_data_fetcher.h",
        "header_utils.h",
        "envoy_fetch.h",
    ],
)


envoy_cc_library(
    name = "envoy_fetcher",
    srcs = [
        "envoy_url_async_fetcher.cc",
        "envoy_cluster_manager.cc",
        "pagespeed_remote_data_fetcher.cc",
        "envoy_fetch.cc",
    ],
    hdrs = [
        ":envoy_fetcher_headers",
    ],
    repository = "@envoy",
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=macro-redefined",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=thread-safety-attributes",
        "-Wno-error=old-style-cast",
        "-Wno-error=thread-safety-analysis",
        "-Wno-error=unused-variable",
        "-Wno-error=unused-lambda-capture",
        "-Wno-error=unused-private-field",
        "-Wno-error=reorder",
    ],
    deps = [
        "@envoy//source/common/config:datasource_lib_with_external_headers",
        "@envoy//source/common/access_log:access_log_manager_lib_with_external_headers",
        "@envoy//source/common/common:minimal_logger_lib_with_external_headers",
        "@envoy//source/common/event:real_time_system_lib_with_external_headers",
        "@envoy//source/common/http:context_lib_with_external_headers",
        "@envoy//source/common/http:utility_lib_with_external_headers",
        "@envoy//source/common/protobuf:message_validator_lib_with_external_headers",
        "@envoy//source/common/secret:secret_manager_impl_lib_with_external_headers",
        "@envoy//source/common/stats:allocator_lib_with_external_headers",
        "@envoy//source/common/stats:stats_lib_with_external_headers",
        "@envoy//source/common/stats:thread_local_store_lib_with_external_headers",
        "@envoy//source/common/thread_local:thread_local_lib_with_external_headers",
        "@envoy//source/common/upstream:cluster_manager_lib_with_external_headers",
        "@envoy//source/exe:platform_header_lib_with_external_headers",
        "@envoy//source/exe:process_wide_lib_with_external_headers",
        "@envoy//source/extensions/transport_sockets/tls:context_lib_with_external_headers",
        "@envoy//source/server/config_validation:admin_lib_with_external_headers",
        "@envoy//source/common/runtime:runtime_lib_with_external_headers",
        "@envoy//source/common/http/http1:conn_pool_lib_with_external_headers",
        "@envoy//source/common/api:api_lib_with_external_headers",
        "@envoy//source/common/common:cleanup_lib_with_external_headers",
        "@envoy//source/common/config:utility_lib_with_external_headers",
        "@envoy//source/common/network:dns_lib_with_external_headers",
        "@envoy//source/common/network:utility_lib_with_external_headers",
        "@envoy//source/common/event:dispatcher_includes_with_external_headers",
        "@envoy//include/envoy/event:dispatcher_interface_with_external_headers",
        "@envoy//source/common/init:manager_lib_with_external_headers",
        "@envoy//source/common/local_info:local_info_lib_with_external_headers",
        "@envoy//source/common/singleton:manager_impl_lib_with_external_headers",
        "@envoy//source/extensions/transport_sockets:well_known_names_with_external_headers",
        "@envoy//source/server:options_lib_with_external_headers",
        "@envoy//source/exe:platform_impl_lib_with_external_headers",
        "@envoy//source/exe:envoy_common_lib_with_external_headers",
        "//net/instaweb/http",
    ],
    visibility = ["//visibility:public"],
)

envoy_cc_test(
    name = "envoy_fetcher_test",
    srcs = ["envoy_url_async_fetcher_test.cc",],
    repository = "@envoy",
    copts = [
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Wno-error=unused-parameter",
        "-Wno-error=macro-redefined",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=thread-safety-attributes",
        "-Wno-error=old-style-cast",
        "-Wno-error=thread-safety-analysis",
        "-Wno-error=unused-variable",
        "-Wno-error=unused-lambda-capture",
        "-Wno-error=reorder",
    ],
    deps = [
        ":envoy_fetcher",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)
