load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2


pagespeed_cc_test_library(
    name = "util_test_base",
    srcs = [
        "lock_manager_spammer.cc",
        "mock_nonce_generator.cc",
        "nonce_generator_test_base.cc",
    ],
    hdrs = [
        "lock_manager_spammer.h",
        "mock_nonce_generator.h",
        "nonce_generator_test_base.h",
    ],
    deps = [
        "//pagespeed/kernel/util",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//test/pagespeed/kernel/thread:thread_test_base",
    ],
)

pagespeed_cc_test(
    name = "util",
    srcs = glob(["*_test.cc"]),
    deps = [
        "//test/pagespeed/kernel/util:util_test_base",
    ],
)
