load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "thread",
    srcs = [
        "pthread_condvar.cc",
        "pthread_mutex.cc",
        "pthread_rw_lock.cc",
        "pthread_shared_mem.cc",
        "pthread_thread_system.cc",
        "queued_alarm.cc",
        "queued_worker.cc",
        "queued_worker_pool.cc",
        "scheduler.cc",
        "scheduler_based_abstract_lock.cc",
        "scheduler_sequence.cc",
        "scheduler_thread.cc",
        "sequence.cc",
        "slow_worker.cc",
        "thread_synchronizer.cc",
        "worker.cc",
    ],
    hdrs = [
        "pthread_condvar.h",
        "pthread_mutex.h",
        "pthread_rw_lock.h",
        "pthread_shared_mem.h",
        "pthread_thread_system.h",
        "queued_alarm.h",
        "queued_worker.h",
        "queued_worker_pool.h",
        "scheduler.h",
        "scheduler_based_abstract_lock.h",
        "scheduler_sequence.h",
        "scheduler_thread.h",
        "sequence.h",
        "slow_worker.h",
        "thread_synchronizer.h",  # XXX(oschaaf): check, for test?
        "worker.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "@mod_pagespeed//base:log_shim",
    ],
)
