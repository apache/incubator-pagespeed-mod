load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "benchmark",
    hdrs = ["benchmark.h"],
    deps = ["@re2//:benchmark"],
)

pagespeed_cc_test(
    name = "base_speed_test",
    srcs = glob(["*speed_test.cc"]),
    shard_count = 1,
    deps = [
        ":benchmark",
        "//test/pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/sharedmem:shared_mem",
    ],
)
