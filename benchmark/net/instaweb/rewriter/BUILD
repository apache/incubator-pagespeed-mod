load("//bazel:pagespeed_test.bzl", "pagespeed_cc_benchmark")

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "rewriter",
    srcs = glob(["*.cc"]),
    copts = [
        "-Ithird_party/css_parser/src/",
        "-Ithird_party/css_parser/src/third_party/utf",
    ],
    deps = [
        "//net/instaweb/rewriter:rewriter_test_base",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//benchmark",
        # TODO(XXX): this is just for data2c generated stuff. Lean up to just that.
        "//pagespeed/system", 
    ],
)
