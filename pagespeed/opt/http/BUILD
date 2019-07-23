load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

proto_library(
    name = "property_cache_proto",
    srcs = [
        "property_cache.proto",
    ],
    deps = [
#        "//pagespeed/kernel/http:http_proto",
    ],
)

cc_proto_library(
    name = "property_cache_proto_cc",
    deps = [
        ":property_cache_proto",
    ],
)


envoy_cc_library(
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
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/cache",
        "//pagespeed/opt/logging",
        ":property_cache_proto_cc"
    ],
)

envoy_cc_test_library(
    name = "http_test_base",
    srcs = [
        "mock_property_page.cc",        
    ],
    hdrs = [
        "mock_property_page.h",
    ],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        ":http",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)


envoy_cc_test(
    name = "http_test",
    srcs = glob(["*_test.cc"]),
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-const-variable",
    ],
    repository = "@envoy",
    deps = [
        ":http_test_base",    ],
)
