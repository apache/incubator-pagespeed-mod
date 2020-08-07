load("//bazel:pagespeed_test.bzl", "pagespeed_cc_benchmark")

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "css_minify_speed_test",
    srcs = ["css_minify_speed_test.cc"],
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//benchmark",
        # TODO(XXX): this is just for data2c generated stuff. Lean up to just that.
        "//pagespeed/system",
    ],
)

pagespeed_cc_benchmark(
    name = "domain_lawyer_speed_test",
    srcs = ["domain_lawyer_speed_test.cc"],
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//benchmark",
    ],
)

pagespeed_cc_benchmark(
    name = "image_speed_test",
    srcs = ["image_speed_test.cc"],
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//benchmark",
    ],
)

pagespeed_cc_benchmark(
    name = "javascript_minify_speed_test",
    srcs = ["javascript_minify_speed_test.cc"],
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//benchmark",
        # TODO(XXX): this is just for data2c generated stuff. Lean up to just that.
        "//pagespeed/system",
    ],
)

pagespeed_cc_benchmark(
    name = "rewrite_driver_speed_test",
    srcs = ["rewrite_driver_speed_test.cc"],
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//benchmark",
    ],
)
