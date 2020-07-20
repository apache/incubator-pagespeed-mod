licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
    name = "http",
    srcs = [
        "async_fetch.cc",
        "async_fetch_with_lock.cc",
        "cache_url_async_fetcher.cc",
        "counting_url_async_fetcher.cc",
        "external_url_fetcher.cc",
        "http_cache.cc",
        "http_cache_failure.cc",
        "http_dump_url_async_writer.cc",
        "http_dump_url_fetcher.cc",
        "http_response_parser.cc",
        "http_value.cc",
        #"http_value_explorer.cc",
        "http_value_writer.cc",
        "inflating_fetch.cc",
        "rate_controller.cc",
        "rate_controlling_url_async_fetcher.cc",
        "simulated_delay_fetcher.cc",
        "sync_fetcher_adapter_callback.cc",
        "url_async_fetcher.cc",
        "url_async_fetcher_stats.cc",
        "wait_url_async_fetcher.cc",
        "wget_url_fetcher.cc",
    ],
    hdrs = glob(["public/*.h"]),
    copts = [
        '-std=c++14',        
    ],    
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/opt/http:http",
        "//pagespeed/kernel/http:http",
    ],
)

pagespeed_cc_test_library(
    name = "http_test_base",
    srcs = [
        "mock_url_fetcher.cc",
        "ua_sensitive_test_fetcher.cc",
    ],
    hdrs = [
        "public/mock_url_fetcher.h",
        "public/ua_sensitive_test_fetcher.h",
    ],
    deps = [
        ":http",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/thread:thread_test_base",
        "//pagespeed/kernel/sharedmem:shared_mem_test_base",
    ],
)

pagespeed_cc_test(
    name = "http_test",
    srcs = glob(["*_test.cc"]),
    data = glob([
        "testdata/*",
        "testdata/www.google.com/*",
    ]),
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=old-style-cast",
#        "-Wno-error=inconsistent-missing-override",
    ],
    deps = [
        ":http_test_base",
    ],
)