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

envoy_cc_library(
    name = "system",
    srcs = [
        "apr_mem_cache.cc",
        "apr_thread_compatible_pool.cc",
        "external_server_spec.cc",
    ],
    hdrs = [
        "controller_process.h",
        "apr_mem_cache.h",
        "apr_thread_compatible_pool.h",
        "external_server_spec.h",
    ],
    copts = [
        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-error=old-style-cast",  # rdestl
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        #        "//pagespeed/opt/http:http",
        #        "//pagespeed/kernel/http:http",
 #       ":controller_proto_grpc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/cache",
        "@apr//:apr",
        "@aprutil//:aprutil",
 #       "//pagespeed/kernel/thread",
 #       "//pagespeed/kernel/util",
 #       "//third_party/rdestl",
    ],
)

envoy_cc_test_library(
    name = "system_test_base",
    srcs = [
        "tcp_server_thread_for_testing.cc",
    ],
    hdrs = [
        "tcp_server_thread_for_testing.h",
    ],
    copts = [
#        "-Wno-error=unused-parameter",
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",

    ],
    data = [
    ],
    repository = "@envoy",
    deps = [
        ":system",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/cache:cache_test_base",
        #"//pagespeed/kernel/util",
        "//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/sharedmem:shared_mem_test_base",
    ],
)

envoy_cc_test(
    name = "system_test",
    srcs = [
        "apr_mem_cache_test.cc",
        "external_server_spec_test.cc",
    ],
   copts = [
        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=old-style-cast",  # rdestl
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",

    ],
    repository = "@envoy",
    deps = [
        ":system_test_base",
#        ":grpc_test_proto_grpc",
    ],
)
