load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_benchmark",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_benchmark(
    name = "html_speed_test",
    srcs = glob(["*speed_test.cc"]),
    deps = [
        "//benchmark",
        "//pagespeed/kernel/html",
    ],
)
