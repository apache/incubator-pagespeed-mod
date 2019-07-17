licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "thread",
    srcs = [
        "slow_worker.cc",
        "worker.cc",
        "scheduler_based_abstract_lock.cc",
        "scheduler.cc",
        "queued_worker_pool.cc",
        "sequence.cc",
        "scheduler_sequence.cc",
        "queued_worker.cc",
        "pthread_shared_mem.cc",
    ],
    hdrs = [
        "slow_worker.h",
        "worker.h",
        "scheduler_based_abstract_lock.h",
        "scheduler.h",
        "queued_worker_pool.h",
        "sequence.h",
        "scheduler_sequence.h",
        "queued_worker.h",
        "pthread_shared_mem.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "@glog",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

envoy_cc_test_library (
    name = "thread_test_base",
    srcs = [
        "mock_scheduler.cc",
    ],
    hdrs = [
        "mock_scheduler.h",
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":thread",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ]
)

envoy_cc_test(
    name = "thread_test",
    repository = "@envoy",
    copts = ["-Wno-error=unused-private-field", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":thread_test_base"
    ],
)