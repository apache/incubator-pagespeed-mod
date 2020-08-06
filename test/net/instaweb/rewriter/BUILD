load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = [
        "critical_images_finder_test_base.cc",
        "css_rewrite_test_base.cc",
        "image_test_base.cc",
        "mock_critical_images_finder.cc",
        "mock_resource_callback.cc",
        "notifying_fetch.cc",
        "rewrite_context_test_base.cc",
        "rewrite_test_base.cc",
        "test_rewrite_driver_factory.cc",
        "test_url_namer.cc",
    ],
    hdrs = [
        "critical_images_finder_test_base.h",
        "css_rewrite_test_base.h",
        "custom_rewrite_test_base.h",
        "image_test_base.h",
        "image_testing_peer.h",
        "mock_critical_images_finder.h",
        "mock_resource_callback.h",
        "notifying_fetch.h",
        "rewrite_context_test_base.h",
        "rewrite_options_test_base.h",
        "rewrite_test_base.h",
        "test_rewrite_driver_factory.h",
        "test_url_namer.h",
    ],
    deps = [
        "//net/instaweb/rewriter",
        "//test/net/instaweb/http:http_test_base",
        "//test/pagespeed/kernel/cache:cache_test_base",
        "//test/pagespeed/kernel/html:html_test_base",
        "//test/pagespeed/kernel/http:http_test_base",
        "//test/pagespeed/kernel/image:image_test_base",
        "//test/pagespeed/kernel/thread:thread_test_base",
        "//test/pagespeed/kernel/util:util_test_base",
        "//test/pagespeed/opt/http:test_base",
        "//test/pagespeed/opt/logging:test_base",
    ],
)

filegroup(
    name = "testdata",
    srcs = glob(["testdata/*"]),
    visibility = ["//visibility:public"],
)

pagespeed_cc_test(
    name = "rewriter",
    srcs = glob(["*_test.cc"]),
    data = [":testdata"],
    deps = ["//test/net/instaweb/rewriter:test_base"],
)

pagespeed_cc_test(
    name = "rewriter_config",
    srcs = glob(["config/*_test.cc"]),
    deps = [
        "//test/pagespeed/automatic:test_base",
    ],
)
