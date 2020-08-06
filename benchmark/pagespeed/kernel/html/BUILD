load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test(
    name = "html_speed_test",
    srcs = glob(["*speed_test.cc"]),
    deps = [
        "//pagespeed/kernel/html",
        "//test/pagespeed/kernel/html:html_test_base",
        "//benchmark",
    ],
)
