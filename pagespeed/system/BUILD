load("//bazel:closure_compiler.bzl", "closure_compiler_gen")
load("//bazel:data2c.bzl", "data2c_gen2_admin_js", "data2c_gen2_admin_css")

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)


licenses(["notice"])  # Apache 2

closure_compiler_gen(
    name = "caches_dbg",
    js_src = "pagespeed/system/caches.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Caches",
    ],
    opt = False,
)

closure_compiler_gen(
    name = "caches_opt",
    js_src = "pagespeed/system/caches.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Caches",
    ],
    opt = True,
)

closure_compiler_gen(
    name = "console_opt",
    js_src = "pagespeed/system/console.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Console",
        "goog:pagespeed.statistics",
    ],
    externs = [
        "pagespeed/system/js_externs.js",
        "third_party/closure/externs/google_visualization_api.js",
    ],
    opt = True,
)

closure_compiler_gen(
    name = "console_dbg",
    js_src = "pagespeed/system/console.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Console",
        "goog:pagespeed.statistics",
    ],
    externs = [
        "pagespeed/system/js_externs.js",
        "third_party/closure/externs/google_visualization_api.js",
    ],
    opt = False,
)

closure_compiler_gen(
    name = "graphs_dbg",
    js_src = "pagespeed/system/graphs.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Graphs",
    ],
    externs = [
        "third_party/closure/externs/google_visualization_api.js",
    ],
    opt = False,
)

closure_compiler_gen(
    name = "graphs_opt",
    js_src = "pagespeed/system/graphs.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Graphs",
    ],
    externs = [
        "third_party/closure/externs/google_visualization_api.js",
    ],
    opt = True,
)

closure_compiler_gen(
    name = "messages_dbg",
    js_src = "pagespeed/system/messages.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Messages",
    ],
    opt = False,
)

closure_compiler_gen(
    name = "messages_opt",
    js_src = "pagespeed/system/messages.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Messages",
    ],
    opt = True,
)

closure_compiler_gen(
    name = "statistics_dbg",
    js_src = "pagespeed/system/statistics.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Statistics",
    ],
    opt = False,
)

closure_compiler_gen(
    name = "statistics_opt",
    js_src = "pagespeed/system/statistics.js",
    js_dir = "system",
    entry_points = [
        "goog:pagespeed.Statistics",
    ],
    opt = True,
)

data2c_gen2_admin_js(
    name = "data2c_generated",
    srcs = [
        "console",
        "caches",
        "statistics",
        "graphs",
        "messages",
    ],
    opt = False,
)

data2c_gen2_admin_js(
    name = "data2c_generated",
    srcs = [
        "console",
        "caches",
        "statistics",
        "graphs",
        "messages",
    ],
    opt = True,
)


data2c_gen2_admin_css(
    name = "data2c_generated",
    srcs = [
        "console",
        "caches",
        "admin_site",
        "statistics",
        "graphs",
    ],
    opt = False,
)

data2c_gen2_admin_css(
    name = "data2c_generated",
    srcs = [
        "console",
        "caches",
        "admin_site",
        "statistics",
        "graphs",
    ],
    opt = True,
)


cc_library(
    name = "system",
    srcs = [
        "add_headers_fetcher.cc",
        "admin_site.cc",
        "apr_mem_cache.cc",
        "apr_thread_compatible_pool.cc",
        "controller_manager.cc",
        "external_server_spec.cc",
        "in_place_resource_recorder.cc",
        "loopback_route_fetcher.cc",
        "redis_cache.cc",
        "serf_url_async_fetcher.cc",
        "system_cache_path.cc",
        "system_caches.cc",
        "system_message_handler.cc",
        "system_request_context.cc",
        "system_rewrite_driver_factory.cc",
        "system_rewrite_options.cc",
        "system_server_context.cc",
        "system_thread_system.cc",
        ":js_data2c_generated_console",
        ":js_data2c_generated_console_opt",
        ":js_data2c_generated_caches",
        ":js_data2c_generated_caches_opt",
        ":js_data2c_generated_statistics",
        ":js_data2c_generated_statistics_opt",
        ":js_data2c_generated_graphs",
        ":js_data2c_generated_graphs_opt",
        ":js_data2c_generated_messages",
        ":js_data2c_generated_messages_opt",
        ":css_data2c_generated_console",
        ":css_data2c_generated_console_opt",
        ":css_data2c_generated_caches",
        ":css_data2c_generated_caches_opt",
        ":css_data2c_generated_admin_site",
        ":css_data2c_generated_admin_site_opt",
        ":css_data2c_generated_statistics",
        ":css_data2c_generated_statistics_opt",
        ":css_data2c_generated_graphs",
        ":css_data2c_generated_graphs_opt",
    ],
    hdrs = [
        "add_headers_fetcher.h",
        "admin_site.h",
        "apr_mem_cache.h",
        "apr_thread_compatible_pool.h",
        "controller_manager.h",
        "controller_process.h",
        "external_server_spec.h",
        "in_place_resource_recorder.h",
        "loopback_route_fetcher.h",
        "redis_cache.h",
        "serf_url_async_fetcher.h",
        "system_cache_path.h",
        "system_caches.h",
        "system_message_handler.h",
        "system_request_context.h",
        "system_rewrite_driver_factory.h",
        "system_rewrite_options.h",
        "system_server_context.h",
        "system_thread_system.h",
    ],
    copts = [
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/private/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
        "-DPAGESPEED_SUPPORT_POSIX_SHARED_MEM=1",       
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":system_controller_process_h",
        "//net/instaweb/http",
        "//net/instaweb/rewriter",
        "//pagespeed/kernel/sharedmem:shared_mem",
        "//third_party/redis-crc",
        "@apr",
        "@aprutil",
        "@hiredis",
        "@serf",
    ],
)

cc_library(
    name = "system_controller_process_h",
    srcs = [
    ],
    hdrs = [
        "controller_process.h",
    ],
    copts = [
    ],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "redis_cache_cluster_setup",
    testonly = True,
    srcs = [
        "redis_cache_cluster_setup_main.cc",
    ],
    copts = [
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/private/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
    ],
    deps = [
        ":system_test_base",
    ],
)

pagespeed_cc_test_library(
    name = "system_test_base",
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
    copts = [
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/private/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
    ],
    deps = [
        ":system",
        "//net/instaweb/rewriter:rewriter_test_base",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/cache:cache_test_base",
        "//pagespeed/kernel/thread:thread_test_base",
    ],
)

pagespeed_cc_test(
    name = "system_test",
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
    copts = [
        "-Iexternal/aprutil/include/",
        "-Iexternal/aprutil/include/private/",
        "-Iexternal/aprutil/include/arch/unix/",
        "-Iexternal/aprutil/",
        "-Iexternal/mod_pagespeed/third_party/aprutil/gen/arch/linux/x64/include/",
        "-Iexternal/mod_pagespeed/third_party/apr/gen/arch/linux/x64/include/",
        "-Iexternal/apr/include/",
        "-Iexternal/apr/include/arch/unix/",
        "-Iexternal/apr/",
        "-Iexternal/serf/include/",
        "-Iexternal/serf/",
    ],
    deps = [
        ":system_test_base",
    ],
)
