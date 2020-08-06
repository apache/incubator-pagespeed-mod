load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    deps = ["//pagespeed/opt/ads"],
)

pagespeed_cc_test(
    name = "ads",
    srcs = glob(["*_test.cc"]),
    deps = [":test_base"],
)
