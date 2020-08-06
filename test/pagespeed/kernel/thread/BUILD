load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "thread_test_base",
    srcs = [
        "mock_scheduler.cc",
        "thread_system_test_base.cc",
        "worker_test_base.cc",
    ],
    hdrs = [
        "blocking_callback.h",
        "condvar_test_base.h",
        "mock_scheduler.h",
        "thread_system_test_base.h",
        "worker_test_base.h",
    ],
    deps = [
        "//pagespeed/kernel/thread",
        "//pagespeed/kernel/util",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "thread_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":thread_test_base",
    ],
)
