load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

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
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/http",
        "//pagespeed/opt/http",
    ],
)
