load("//bazel:pagespeed_test.bzl", "pagespeed_cc_benchmark")

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "rewriter",
    srcs = glob(["*.cc"]),
    deps = [
        "//test/net/instaweb/rewriter:test_base",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//benchmark",
        # TODO(XXX): this is just for data2c generated stuff. Lean up to just that.
        "//pagespeed/system",
    ],
)
