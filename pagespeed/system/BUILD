load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

cc_library(
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
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/cache",
        "@apr//:apr",
        "@aprutil//:aprutil",
    ],
)

pagespeed_cc_test_library(
    name = "system_test_base",
    srcs = [
        "tcp_server_thread_for_testing.cc",
    ],
    hdrs = [
        "tcp_server_thread_for_testing.h",
    ],
    copts = [
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    deps = [
        ":system",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/cache:cache_test_base",
        "//pagespeed/kernel/thread:thread_test_base",
    ],
)

pagespeed_cc_test(
    name = "system_test",
    srcs = [
        "apr_mem_cache_test.cc",
        "external_server_spec_test.cc",
    ],
   copts = [
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    deps = [
        ":system_test_base",
    ],
)
