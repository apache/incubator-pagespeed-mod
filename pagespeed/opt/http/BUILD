load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

licenses(["notice"])  # Apache 2

proto_library(
    name = "property_cache_proto",
    srcs = [
        "property_cache.proto",
    ],
)

cc_proto_library(
    name = "property_cache_proto_cc",
    deps = [
        ":property_cache_proto",
    ],
)

cc_library(
    name = "http",
    srcs = [
        "abstract_property_store_get_callback.cc",
        "cache_property_store.cc",
        "fallback_property_page.cc",
        "property_cache.cc",
        "property_store.cc",
        "request_context.cc",
        "two_level_property_store.cc",
    ],
    hdrs = [
        "abstract_property_store_get_callback.h",
        "cache_property_store.h",
        "fallback_property_page.h",
        "property_cache.h",
        "property_store.h",
        "request_context.h",
        "two_level_property_store.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":property_cache_proto_cc",
        "//pagespeed/kernel/cache",
        "//pagespeed/opt/logging",
    ],
)
