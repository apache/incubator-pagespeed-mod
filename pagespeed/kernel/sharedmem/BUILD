licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
    name = "shared_mem",
    srcs = [
        "shared_mem_cache.cc",
        "inprocess_shared_mem.cc",
        "shared_mem_statistics.cc",
        "shared_mem_cache_data.cc",
        "shared_circular_buffer.cc",
        "shared_dynamic_string_map.cc",
        "shared_mem_lock_manager.cc",
    ],
    hdrs = [
        "inprocess_shared_mem.h",
        "shared_mem_statistics.h",
        "shared_mem_cache.h",
        "shared_mem_cache_data.h",
        "shared_circular_buffer.h",
        "shared_dynamic_string_map.h",
        "shared_mem_lock_manager.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/cache:cache",
        "//pagespeed/kernel/util:util",
        ":shared_mem_cache_shapshot_proto_cc",
    ],
)

proto_library(
    name = "shared_mem_cache_shapshot_proto",
    srcs = ["shared_mem_cache_snapshot.proto"],
)

cc_proto_library(
    name = "shared_mem_cache_shapshot_proto_cc",
    deps = [":shared_mem_cache_shapshot_proto"],
)

pagespeed_cc_test_library (
    name = "shared_mem_test_base",
    srcs = [
        "shared_mem_test_base.cc",
        "shared_circular_buffer_test_base.cc",
        "shared_dynamic_string_map_test_base.cc",
        "shared_mem_cache_data_test_base.cc",
        "shared_mem_cache_test_base.cc",
        "shared_mem_lock_manager_test_base.cc",
        "shared_mem_statistics_test_base.cc",
    ],
    hdrs = [
        "shared_mem_test_base.h",
        "shared_circular_buffer_test_base.h",
        "shared_dynamic_string_map_test_base.h",
        "shared_mem_cache_data_test_base.h",
        "shared_mem_cache_test_base.h",
        "shared_mem_lock_manager_test_base.h",
        "shared_mem_statistics_test_base.h",
    ],
    deps = [
        ":shared_mem",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
        "//pagespeed/kernel/cache:cache_test_base",
        "//pagespeed/kernel/thread:thread_test_base",
    ]
)

pagespeed_cc_test(
    name = "shared_mem_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":shared_mem_test_base",
        "//pagespeed/kernel/cache:cache",
        ":shared_mem",
    ],
)