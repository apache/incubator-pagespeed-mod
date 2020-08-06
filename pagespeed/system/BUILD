load("@rules_cc//cc:defs.bzl", "cc_library")
load("//bazel:closure_compiler.bzl", "closure_compiler_gen")
load("//bazel:data2c.bzl", "data2c_gen2_admin_css", "data2c_gen2_admin_js")

licenses(["notice"])  # Apache 2

closure_compiler_gen(
    name = "caches_dbg",
    entry_points = [
        "goog:pagespeed.Caches",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/caches.js",
    opt = False,
)

closure_compiler_gen(
    name = "caches_opt",
    entry_points = [
        "goog:pagespeed.Caches",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/caches.js",
    opt = True,
)

closure_compiler_gen(
    name = "console_opt",
    entry_points = [
        "goog:pagespeed.Console",
        "goog:pagespeed.statistics",
    ],
    externs = [
        "pagespeed/system/js_externs.js",
        "third_party/closure/externs/google_visualization_api.js",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/console.js",
    opt = True,
)

closure_compiler_gen(
    name = "console_dbg",
    entry_points = [
        "goog:pagespeed.Console",
        "goog:pagespeed.statistics",
    ],
    externs = [
        "pagespeed/system/js_externs.js",
        "third_party/closure/externs/google_visualization_api.js",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/console.js",
    opt = False,
)

closure_compiler_gen(
    name = "graphs_dbg",
    entry_points = [
        "goog:pagespeed.Graphs",
    ],
    externs = [
        "third_party/closure/externs/google_visualization_api.js",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/graphs.js",
    opt = False,
)

closure_compiler_gen(
    name = "graphs_opt",
    entry_points = [
        "goog:pagespeed.Graphs",
    ],
    externs = [
        "third_party/closure/externs/google_visualization_api.js",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/graphs.js",
    opt = True,
)

closure_compiler_gen(
    name = "messages_dbg",
    entry_points = [
        "goog:pagespeed.Messages",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/messages.js",
    opt = False,
)

closure_compiler_gen(
    name = "messages_opt",
    entry_points = [
        "goog:pagespeed.Messages",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/messages.js",
    opt = True,
)

closure_compiler_gen(
    name = "statistics_dbg",
    entry_points = [
        "goog:pagespeed.Statistics",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/statistics.js",
    opt = False,
)

closure_compiler_gen(
    name = "statistics_opt",
    entry_points = [
        "goog:pagespeed.Statistics",
    ],
    js_dir = "system",
    js_src = "pagespeed/system/statistics.js",
    opt = True,
)

data2c_gen2_admin_js(
    name = "data2c_generated",
    srcs = [
        "caches",
        "console",
        "graphs",
        "messages",
        "statistics",
    ],
    opt = False,
)

data2c_gen2_admin_js(
    name = "data2c_generated",
    srcs = [
        "caches",
        "console",
        "graphs",
        "messages",
        "statistics",
    ],
    opt = True,
)

data2c_gen2_admin_css(
    name = "data2c_generated",
    srcs = [
        "admin_site",
        "caches",
        "console",
        "graphs",
        "statistics",
    ],
    opt = False,
)

data2c_gen2_admin_css(
    name = "data2c_generated",
    srcs = [
        "admin_site",
        "caches",
        "console",
        "graphs",
        "statistics",
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
        ":css_data2c_generated_admin_site",
        ":css_data2c_generated_admin_site_opt",
        ":css_data2c_generated_caches",
        ":css_data2c_generated_caches_opt",
        ":css_data2c_generated_console",
        ":css_data2c_generated_console_opt",
        ":css_data2c_generated_graphs",
        ":css_data2c_generated_graphs_opt",
        ":css_data2c_generated_statistics",
        ":css_data2c_generated_statistics_opt",
        ":js_data2c_generated_caches",
        ":js_data2c_generated_caches_opt",
        ":js_data2c_generated_console",
        ":js_data2c_generated_console_opt",
        ":js_data2c_generated_graphs",
        ":js_data2c_generated_graphs_opt",
        ":js_data2c_generated_messages",
        ":js_data2c_generated_messages_opt",
        ":js_data2c_generated_statistics",
        ":js_data2c_generated_statistics_opt",
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
    visibility = ["//visibility:public"],
    deps = [
        ":system_controller_process_h",
        "//net/instaweb/http",
        "//net/instaweb/rewriter",
        "//pagespeed/kernel/sharedmem",
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
    visibility = ["//visibility:public"],
)
