load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

proto_library(
    name = "proto_matcher_test_proto",
    srcs = ["proto_matcher_test.proto"],
)

cc_proto_library(
    name = "proto_matcher_test_proto_cc",
    deps = [":proto_matcher_test_proto"],
)

pagespeed_cc_test_library(
    name = "pagespeed_gtest",
    srcs = ["gtest.cc"],
    hdrs = [
        "gmock.h",
        "gtest.h",
    ],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base_core",
        "@com_google_googletest//:gtest",
    ],
)

pagespeed_cc_test_library(
    name = "kernel_test_util",
    srcs = [
        "file_system_test_base.cc",
        "mem_file_system.cc",
        "message_handler_test_base.cc",
        "mock_hasher.cc",
        "mock_message_handler.cc",
        "mock_timer.cc",
        "null_thread_system.cc",
    ],
    hdrs = [
        "file_system_test_base.h",
        "mem_file_system.h",
        "message_handler_test_base.h",
        "mock_hasher.h",
        "mock_message_handler.h",
        "mock_timer.h",
        "named_lock_tester.h",
        "null_thread_system.h",
    ],
    deps = [
        ":pagespeed_gtest",
        ":proto_matcher_test_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util",
    ],
)

pagespeed_cc_test(
    name = "base_test",
    srcs = glob(
        ["*_test.cc"],
        exclude = glob(["*speed_test.cc"]),
    ),
    shard_count = 1,
    deps = [
        ":kernel_test_util",
        "//pagespeed/kernel/sharedmem",
    ],
)
