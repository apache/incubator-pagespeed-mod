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
        "redis_cache.cc",
        #"loopback_route_fetcher.cc",
        "in_place_resource_recorder.cc",
    ],
    hdrs = [
        "apr_mem_cache.h",
        "apr_thread_compatible_pool.h",
        "controller_process.h",
        "external_server_spec.h",
        #"loopback_route_fetcher.h",
        "redis_cache.h",
        "in_place_resource_recorder.h",
    ],
    copts = [
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//net/instaweb/http",
        #"//net/instaweb/rewriter:rewriter",
        "//third_party/redis-crc",
        "@apr",
        "@aprutil",
        "@hiredis",
    ],
)

pagespeed_cc_test_library(
    name = "system_test_base",
    srcs = [
        "tcp_server_thread_for_testing.cc",
        "redis_cache_cluster_setup.cc",
        "tcp_connection_for_testing.cc",
    ],
    hdrs = [
        "tcp_server_thread_for_testing.h",
        "redis_cache_cluster_setup.h",
        "tcp_connection_for_testing.h",
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
        "//net/instaweb/rewriter:rewriter_test_base",
    ],
)

pagespeed_cc_test(
    name = "system_test",
    srcs = [
        "apr_mem_cache_test.cc",
        "external_server_spec_test.cc",
        #"loopback_route_fetcher_test.cc",
        "redis_cache_cluster_test.cc",
        "in_place_resource_recorder_test.cc",
    ],
    copts = [
        "-Iexternal/apr/include/",
        "-Iexternal/aprutil/include/",
    ],
    deps = [
        ":system_test_base",
    ],
)
