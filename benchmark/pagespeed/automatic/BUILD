load("//bazel:pagespeed_test.bzl", "pagespeed_cc_benchmark")

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "automatic",
    srcs = glob(["*.cc"]),
    data = ["//net/instaweb/rewriter:testdata"],
    deps = [
        "//pagespeed/automatic:static_rewriter",
        "//benchmark",
    ],
)
