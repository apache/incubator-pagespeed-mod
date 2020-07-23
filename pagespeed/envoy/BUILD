load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
    "envoy_cc_library",
    "envoy_cc_test",
)
load("@envoy_api//bazel:api_build_system.bzl", "api_cc_py_proto_library")

package(default_visibility = ["//visibility:public"])

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    deps = [
        ":http_filter_config",
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

api_cc_py_proto_library(
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
        "-Ithird_party/aprutil/gen/arch/linux/x64/include/",
        "-Ithird_party/apr/gen/arch/linux/x64/include/",
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
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Ithird_party/aprutil/gen/arch/linux/x64/include/",
        "-Ithird_party/apr/gen/arch/linux/x64/include/",
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
