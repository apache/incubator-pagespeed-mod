load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test(
    name = "js",
    srcs = glob(["*_test.cc"]),
    data = glob(["testdata/third_party/*"]),
    deps = [
        "//pagespeed/kernel/js",
    ],
)
