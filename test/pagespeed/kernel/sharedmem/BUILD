load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = [
        "shared_circular_buffer_test_base.cc",
        "shared_dynamic_string_map_test_base.cc",
        "shared_mem_cache_data_test_base.cc",
        "shared_mem_cache_test_base.cc",
        "shared_mem_lock_manager_test_base.cc",
        "shared_mem_statistics_test_base.cc",
        "shared_mem_test_base.cc",
    ],
    hdrs = [
        "shared_circular_buffer_test_base.h",
        "shared_dynamic_string_map_test_base.h",
        "shared_mem_cache_data_test_base.h",
        "shared_mem_cache_test_base.h",
        "shared_mem_lock_manager_test_base.h",
        "shared_mem_statistics_test_base.h",
        "shared_mem_test_base.h",
    ],
    deps = [
        "//pagespeed/kernel/sharedmem",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/cache:cache_test_base",
        "//test/pagespeed/kernel/thread:thread_test_base",
        "//test/pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "sharedmem",
    srcs = glob(["*_test.cc"]),
    shard_count = 1,
    deps = [
        "//pagespeed/kernel/cache",
        "//pagespeed/kernel/sharedmem",
        "//test/pagespeed/kernel/sharedmem:test_base",
    ],
)
