licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
    name = "pagespeed_base_core",
    srcs = [
        'abstract_mutex.cc',
        'abstract_shared_mem.cc',
        'annotated_message_handler.cc',
        'atom.cc',
        'debug.cc',
        'file_message_handler.cc',
        'file_system.cc',
        'google_message_handler.cc',
        'message_handler.cc',
        'null_message_handler.cc',
        'null_mutex.cc',
        'null_shared_mem.cc',
        'null_statistics.cc',
        'null_writer.cc',
        'print_message_handler.cc',
        'statistics.cc',
        'stdio_file_system.cc',
        'string_convert.cc',
        'string_util.cc',
        'string_writer.cc',
        'symbol_table.cc',
        'time_util.cc',
        'timer.cc',
        'thread_system.cc',
        'writer.cc',
        #'mem_debug.cc',
    ],
    hdrs = [
        'abstract_mutex.h',
        'abstract_shared_mem.h',
        'writer.h',
        'string_writer.h',
        'annotated_message_handler.h',
        "atom.h",
        'debug.h',
        'file_message_handler.h',
        'file_system.h',
        'google_message_handler.h',
        'message_handler.h',
        'null_message_handler.h',
        'null_mutex.h',
        'null_shared_mem.h',
        'null_writer.h',
        'null_statistics.h',
        'statistics_template.h',
        'stl_util.h',
        'print_message_handler.h',
        'statistics.h',
        'stdio_file_system.h',
        "string_util.h",
        "symbol_table.h",
        'time_util.h',
        'timer.h',
        'thread_system.h',
        "basictypes.h",
        "printf_format.h",
        "string.h",
        "string_hash.h",
        "thread_annotations.h",
        "stack_buffer.h",
        "dense_hash_map.h",
        "dense_hash_set.h",
        "sparse_hash_map.h",
        "rde_hash_map.h",
        "scoped_ptr.h",
        "dynamic_annotations.h",
        "vector_deque.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/google-sparsehash:google_sparsehash_gen",
        "@google_sparsehash",
        "//gurl:url",
        "//third_party/nspr:prtime",
        "@icu//:icudata",
        "@icu//:icuuc",
        "@mod_pagespeed//base:log_shim",
        "@glog",
        #"//pagespeed/kernel/sharedmem:shared_mem",
        #"//pagespeed/kernel/util:util",
        "@com_google_absl//absl/base:base",
        "@com_google_absl//absl/strings:strings",
        "@com_github_fmtlib_fmt//:fmtlib"
    ],
)

proto_library(
    name = "proto_matcher_test_proto",
    srcs = ["proto_matcher_test.proto"],
)

cc_proto_library(
    name = "proto_matcher_test_proto_cc",
    deps = [":proto_matcher_test_proto"],
)

cc_library(
    name = "pagespeed_base",
    srcs = [
        'cache_interface.cc',
        'charset_util.cc',
        'checking_thread_system.cc',
        'chunking_writer.cc',
        'circular_buffer.cc',
        'condvar.cc',
        'countdown_timer.cc',
        'escaping.cc',
        "wildcard_group.cc",
        'fast_wildcard_group.cc',
        'file_writer.cc',
        'function.cc',
        'hasher.cc',
        'hostname_util.cc',
        'json_writer.cc',
        'md5_hasher.cc',
        'named_lock_manager.cc',
        'null_rw_lock.cc',
        'posix_timer.cc',
        'request_trace.cc',
        'rolling_hash.cc',
        'sha1_signature.cc',
        'shared_string.cc',
        'signature.cc',
        'source_map.cc',
        'split_statistics.cc',
        'split_writer.cc',
        'thread.cc',
        'waveform.cc',
        'wildcard.cc',
    ],
    hdrs = [
        "sha1_signature.h",
        "null_rw_lock.h",
        "cache_interface.h",
        "shared_string.h",
        "request_trace.h",
        "split_writer.h",
        "split_statistics.h",
        "json_writer.h",
        "source_map.h",
        "signature.h",
        "rolling_hash.h",
        "condvar.h",
        "escaping.h",
        "checking_thread_system.h",
        "charset_util.h",
        "md5_hasher.h",
        "hasher.h",
        "chunking_writer.h",
        "posix_timer.h",
        "hostname_util.h",
        "wildcard_group.h",
        "fast_wildcard_group.h",
        "function.h",
        "countdown_timer.h",
        "named_lock_manager.h",
        "atomic_bool.h",
        "wildcard.h",
        "atomicops.h",
        "file_writer.h",
        "atomic_int32.h",
        "waveform.h",
        "ref_counted_ptr.h",
        "circular_buffer.h",
        "base64_util.h",
        "json.h",
        "thread.h",
        "arena.h",
        "callback.h",
        "inline_slist.h",
        "pool.h",
        "pool_element.h",
        "proto_matcher_impl.h",
        "proto_matcher.h",
        "proto_util.h",
        "enum_set.h",
        "string_multi_map.h",
        "manually_ref_counted.h",
        "gtest_prod.h", # hmm, is this right?
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":pagespeed_base_core",
        "//third_party/base64:base64",
        "@jsoncpp//:jsoncpp",
        "@com_google_googletest//:gtest",# for gtest_prod.h   
        "@boringssl//:ssl",     
    ],
)

pagespeed_cc_test_library(
    name = "pagespeed_gtest",
    srcs = ["gtest.cc"],
    hdrs = ["gtest.h", "gmock.h"],
    deps = [
        ":pagespeed_base_core",
        "@com_google_googletest//:gtest",
    ],
)

pagespeed_cc_test_library (
    name = "kernel_test_util",
    hdrs = [
            "message_handler_test_base.h",
            "file_system_test_base.h",
            "mock_message_handler.h",
            "mock_timer.h",
            "mock_hasher.h",
            'mem_file_system.h',        
            "null_thread_system.h",
            "named_lock_tester.h",
        ],
    srcs = 
        [
            "message_handler_test_base.cc",
            "file_system_test_base.cc",
            "mock_message_handler.cc",
            "mock_timer.cc",
            "mock_hasher.cc",
            'mem_file_system.cc',
            "null_thread_system.cc"
        ],
    deps = [
        ":pagespeed_base_core",
        ":pagespeed_base",
        ":pagespeed_gtest",
        ":proto_matcher_test_proto_cc",
        ":benchmark",
        "//pagespeed/kernel/util:util",
    ]
)


pagespeed_cc_test_library (
    name = "benchmark",
    hdrs = [
            "benchmark.h",
        ],
    srcs = 
        [
        ],
    deps = [
        "@re2//:benchmark"
    ]
)


pagespeed_cc_test(
    name = "base_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":pagespeed_base",
        ":pagespeed_gtest",
        ":kernel_test_util",
        "//pagespeed/kernel/sharedmem:shared_mem"
    ],
)

