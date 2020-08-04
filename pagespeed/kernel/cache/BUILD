load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

cc_library(
    name = "cache",
    srcs = [
        "async_cache.cc",
        "cache_batcher.cc",
        "cache_key_prepender.cc",
        "cache_stats.cc",
        "compressed_cache.cc",
        "delay_cache.cc",
        "delegating_cache_callback.cc",
        "fallback_cache.cc",
        "file_cache.cc",
        "in_memory_cache.cc",
        "key_value_codec.cc",
        "lru_cache.cc",
        "purge_context.cc",
        "purge_set.cc",
        "threadsafe_cache.cc",
        "write_through_cache.cc",
    ],
    hdrs = [
        "async_cache.h",
        "cache_batcher.h",
        "cache_interface.h",
        "cache_key_prepender.h",
        "cache_stats.h",
        "compressed_cache.h",
        "delay_cache.h",
        "delegating_cache_callback.h",
        "fallback_cache.h",
        "file_cache.h",
        "in_memory_cache.h",
        "key_value_codec.h",
        "lru_cache.h",
        "lru_cache_base.h",
        "purge_context.h",
        "purge_set.h",
        "threadsafe_cache.h",
        "write_through_cache.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/thread",
        "//pagespeed/kernel/util",
    ],
)

pagespeed_cc_test_library(
    name = "cache_test_base",
    srcs = [
        "cache_spammer.cc",
        "mock_time_cache.cc",
    ],
    hdrs = [
        "cache_batcher_testing_peer.h",
        "cache_spammer.h",
        "cache_test_base.h",
        "mock_time_cache.h",
    ],
    deps = [
        ":cache",
        "//pagespeed/kernel/base:benchmark",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "cache_test",
    srcs = glob(
        ["*_test.cc"],
        exclude = glob(["*_speed_test.cc"]),
    ),
    deps = [
        ":cache",
        ":cache_test_base",
    ],
)

pagespeed_cc_test(
    name = "cache_speed_test",
    srcs = glob(["*_speed_test.cc"]),
    deps = [
        ":cache",
        ":cache_test_base",
        "//pagespeed/kernel/base:benchmark",
    ],
)
