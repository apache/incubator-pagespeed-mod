load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2


pagespeed_cc_test_library(
    name = "html_test_base",
    srcs = glob(["*.cc"], exclude=glob(["*_test.cc"])),
    hdrs = glob(["*.h"]),
    deps = [
        "//pagespeed/kernel/html",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "html_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        "//test/pagespeed/kernel/html:html_test_base",
    ],
)
