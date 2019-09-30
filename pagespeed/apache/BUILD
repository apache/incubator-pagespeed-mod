licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_binary(
    name = "libmod_pagespeed.so",
    linkshared = True,
    linkstatic= True,
    visibility = ["//visibility:public"],
    deps = [":apache"],
)

cc_library(
    name = "apache",
    srcs = [
        "apache_config.cc",
        "apache_fetch.cc",
        "apache_message_handler.cc",
        "apache_request_context.cc",
        "apache_rewrite_driver_factory.cc",
        "apache_server_context.cc",
        "apache_slurp.cc",
        "apache_thread_system.cc",
        "apache_writer.cc",
        "apr_timer.cc",
        "header_util.cc",
        "instaweb_context.cc",
        "instaweb_handler.cc",
        "log_message_handler.cc",
        "mod_instaweb.cc",
        "simple_buffered_apache_fetch.cc",
    ],
    hdrs = [
        "apache_config.h",
        "apache_fetch.h",
        "apache_httpd_includes.h",
        "apache_logging_includes.h",
        "apache_message_handler.h",
        "apache_request_context.h",
        "apache_rewrite_driver_factory.h",
        "apache_server_context.h",
        "apache_thread_system.h",
        "apache_writer.h",
        "apr_timer.h",
        "header_util.h",
        "instaweb_context.h",
        "instaweb_handler.h",
        "log_message_handler.h",
        "mod_instaweb.h",
        "simple_buffered_apache_fetch.h",
    ],
    copts = [
        "-I/usr/include/apache2/",
        "-Iexternal/aprutil/include/",
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
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//pagespeed/automatic",
        "//pagespeed/system",
    ],
)

pagespeed_cc_test_library(
    name = "apache_test_base",
    srcs = [
        "mock_apache.cc",
    ],
    hdrs = [
        "mock_apache.h",
    ],
    copts = [
        "-I/usr/include/apache2/",
        "-Iexternal/aprutil/include/",
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
        ":apache",
        "//net/instaweb/rewriter:rewriter_test_base",
        "//pagespeed/kernel/base:benchmark",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "apache_test",
    srcs = [
        "apache_config_test.cc",
        "apache_fetch_test.cc",
        "apache_writer_test.cc",
        "header_util_test.cc",
        "simple_buffered_apache_fetch_test.cc",
    ],
    copts = [
        "-I/usr/include/apache2/",
        "-Iexternal/aprutil/include/",
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
    data = ["//net/instaweb/rewriter:testdata"],
    deps = [
        ":apache_test_base",
    ],
)
