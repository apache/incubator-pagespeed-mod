licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "pagespeed_base_core",
    srcs = [
        'abstract_mutex.cc',
        'annotated_message_handler.cc',
        "atom.cc",
        #'debug.cc',
        #'file_message_handler.cc',
        #'file_system.cc',
        'google_message_handler.cc',
        'message_handler.cc',
        'null_message_handler.cc',
        #'null_mutex.cc',
        #'null_shared_mem.cc',
        #'null_writer.cc',
        'print_message_handler.cc',
        #'statistics.cc',
        #'stdio_file_system.cc',
        "string_convert.cc",
        "string_util.cc",
        #'string_writer.cc',
        "symbol_table.cc",
        #'time_util.cc',
        #'timer.cc',
        #'thread_system.cc',
        #'writer.cc',
    ],
    hdrs = [
        'abstract_mutex.h',
        'annotated_message_handler.h',
        "atom.h",
        #'debug.h',
        #'file_message_handler.h',
        #'file_system.h',
        'google_message_handler.h',
        'message_handler.h',
        'null_message_handler.h',
        #'null_mutex.h',
        #'null_shared_mem.h',
        #'null_writer.h',
        'print_message_handler.h',
        #'statistics.h',
        #'stdio_file_system.h',
        "string_util.h",
        #'string_writer.h',
        "symbol_table.h",
        #'time_util.h',
        #'timer.h',
        #'thread_system.h',
        #'writer.h',
        "basictypes.h",
        "printf_format.h",
        "string.h",
        "string_hash.h",
        "thread_annotations.h",
        "stack_buffer.h",
        "dense_hash_map.h",
        #"abstract_shared_mem.h",
        #"scoped_ptr.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/google-sparsehash:google_sparsehash_gen",
        "@google_sparsehash",
        "@glog",
    ],
)

envoy_cc_library(
    name = "pagespeed_gtest",
    srcs = ["gtest.cc"],
    hdrs = ["gtest.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":pagespeed_base_core",
        "@com_google_googletest//:gtest",
    ],
)

envoy_cc_library (
    name = "pagespeed_test_base",
    hdrs = ["message_handler_test_base.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
)

envoy_cc_test(
    name = "pagespeed_base_core_test",
    srcs = [
        "annotated_message_handler_test.cc",
        "message_handler_test.cc",
        "string_util_test.cc",
        "symbol_table_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":pagespeed_base_core",
        ":pagespeed_gtest",
        ":pagespeed_test_base"
    ],
)
