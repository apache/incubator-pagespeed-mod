load("@rules_cc//cc:defs.bzl", "cc_binary")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

cc_binary(
    name = "redis_cache_cluster_setup",
    testonly = True,
    srcs = [
        "redis_cache_cluster_setup_main.cc",
    ],
    deps = [
        "//test/pagespeed/system:test_base",
    ],
)

pagespeed_cc_test_library(
    name = "test_base",
    srcs = [
        "redis_cache_cluster_setup.cc",
        "tcp_connection_for_testing.cc",
        "tcp_server_thread_for_testing.cc",
    ],
    hdrs = [
        "redis_cache_cluster_setup.h",
        "tcp_connection_for_testing.h",
        "tcp_server_thread_for_testing.h",
    ],
    deps = [
        "//pagespeed/system",
        "//test/net/instaweb/rewriter:test_base",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/cache:cache_test_base",
        "//test/pagespeed/kernel/thread:thread_test_base",
    ],
)

pagespeed_cc_test(
    name = "system",
    srcs = [
        "apr_mem_cache_test.cc",
        "external_server_spec_test.cc",
        "in_place_resource_recorder_test.cc",
        "loopback_route_fetcher_test.cc",
        "redis_cache_cluster_test.cc",
        "serf_url_async_fetcher_test.cc",
        "system_caches_test.cc",
        "system_message_handler_test.cc",
    ],
    shard_count = 1,
    deps = [
        "//test/pagespeed/system:test_base",
    ],
)
