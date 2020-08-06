load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "sharedmem",
    srcs = [
        "inprocess_shared_mem.cc",
        "shared_circular_buffer.cc",
        "shared_dynamic_string_map.cc",
        "shared_mem_cache.cc",
        "shared_mem_cache_data.cc",
        "shared_mem_lock_manager.cc",
        "shared_mem_statistics.cc",
    ],
    hdrs = [
        "inprocess_shared_mem.h",
        "shared_circular_buffer.h",
        "shared_dynamic_string_map.h",
        "shared_mem_cache.h",
        "shared_mem_cache_data.h",
        "shared_mem_lock_manager.h",
        "shared_mem_statistics.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":shared_mem_cache_shapshot_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/cache",
        "//pagespeed/kernel/util",
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
