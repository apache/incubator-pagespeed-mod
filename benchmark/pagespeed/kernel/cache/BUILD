load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test(
    name = "cache",
    srcs = glob(["*.cc"]),
    deps = [
        "//benchmark",
        "//pagespeed/kernel/cache",
    ],
)
