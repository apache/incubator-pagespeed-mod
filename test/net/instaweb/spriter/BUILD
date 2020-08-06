load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "test_base",
    srcs = ["mock_image_library_interface.h"],
    deps = ["//net/instaweb/spriter"],
)

pagespeed_cc_test(
    name = "spriter",
    srcs = glob(["*_test.cc"]),
    data = ["//test/net/instaweb/rewriter:testdata"],
    deps = ["//test/net/instaweb/spriter:test_base"],
)
