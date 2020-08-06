load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "http_test_base",
    srcs = [
        "user_agent_matcher_test_base.cc",
    ],
    hdrs = [
        "user_agent_matcher_test_base.h",
    ],
    deps = [
        "//pagespeed/kernel/http",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/util:util_test_base",
    ],
)

pagespeed_cc_test(
    name = "http",
    srcs = glob(["*_test.cc"]),
    deps = [
        "//test/pagespeed/kernel/http:http_test_base",
    ],
)
