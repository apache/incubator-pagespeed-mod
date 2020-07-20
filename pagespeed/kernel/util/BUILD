licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

cc_library(
    name = "util_re2",
    hdrs = [
        "re2.h",
    ],
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=unused-parameter",
        "-Wno-unused-parameter",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "util",
    srcs = [
        "brotli_inflater.cc",
        "file_system_lock_manager.cc",
        "gflags.cc",  # XXX
        "gzip_inflater.cc",
        "hashed_nonce_generator.cc",
        "input_file_nonce_generator.cc",
        "mem_lock.cc",
        "mem_lock_manager.cc",
        "mem_lock_state.cc",
        "nonce_generator.cc",
        "platform.cc",
        "simple_random.cc",
        "simple_stats.cc",
        "statistics_logger.cc",
        "threadsafe_lock_manager.cc",
        "url_escaper.cc",
        "url_multipart_encoder.cc",
        "url_segment_encoder.cc",
        "url_to_filename_encoder.cc",
    ],
    hdrs = [
        "brotli_inflater.h",
        "categorized_refcount.h",
        "copy_on_write.h",
        "file_system_lock_manager.h",
        "gflags.h",  # XXX
        "grpc.h",  # XXX
        "gzip_inflater.h",
        "hashed_nonce_generator.h",
        "input_file_nonce_generator.h",
        "mem_lock.h",
        "mem_lock_manager.h",
        "mem_lock_state.h",
        "nonce_generator.h",
        "platform.h",
        "simple_random.h",
        "simple_stats.h",
        "statistics_logger.h",
        "threadsafe_lock_manager.h",
        "url_escaper.h",
        "url_multipart_encoder.h",
        "url_segment_encoder.h",
        "url_to_filename_encoder.h",
    ],
    copts = [
        "-std=c++14",
        "-Wno-implicit-fallthrough",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":util_re2",
        "//gurl:url",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html",
        "//pagespeed/kernel/http",
        "//pagespeed/kernel/thread",
        "@brotli//:brotlidec",
        "@brotli//:brotlienc",
        "@com_github_grpc_grpc//:grpc++",
        "@envoy//bazel/foreign_cc:zlib",
    ],
)

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
        ":util",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/thread:thread_test_base",
        "@re2",
        "@re2//:benchmark",
    ],
)

pagespeed_cc_test(
    name = "util_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":util_test_base",
    ],
)
