licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
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
        "pthread_thread_system.cc",
        "pthread_mutex.cc",
        "pthread_condvar.cc",
        "pthread_rw_lock.cc",
        "queued_alarm.cc",
        "scheduler_thread.cc",
        "thread_synchronizer.cc",
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
        "pthread_thread_system.h",
        "pthread_mutex.h",
        "pthread_condvar.h",
        "pthread_rw_lock.h",
        "queued_alarm.h",
        "scheduler_thread.h",
        "thread_synchronizer.h", # XXX(oschaaf): check, for test?
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_googlesource_googleurl//url",
        "@mod_pagespeed//base:log_shim",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

pagespeed_cc_test_library (
    name = "thread_test_base",
    srcs = [
        "mock_scheduler.cc",
        "thread_system_test_base.cc",
        "worker_test_base.cc",
    ],
    hdrs = [
        "blocking_callback.h",
        "mock_scheduler.h",
        "thread_system_test_base.h",
        "worker_test_base.h",
        "condvar_test_base.h",
    ],
    deps = [
        ":thread",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/util:util",
    ]
)

pagespeed_cc_test(
    name = "thread_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":thread_test_base"
    ],
)