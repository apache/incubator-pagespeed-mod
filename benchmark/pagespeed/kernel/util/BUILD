load("//bazel:pagespeed_test.bzl", "pagespeed_cc_benchmark")

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "util",
    srcs = glob(["*.cc"]),
    deps = [
        "//pagespeed/kernel/util",
        "//benchmark",
    ],
)
