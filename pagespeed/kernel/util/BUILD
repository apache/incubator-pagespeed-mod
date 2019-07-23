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
    name = "util_re2",
    hdrs = [
        "re2.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=old-style-cast", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
)

envoy_cc_library(
    name = "util",
    srcs = [
        "statistics_logger.cc",
        "url_to_filename_encoder.cc",
        "url_escaper.cc",
        "platform.cc",
        "brotli_inflater.cc",
        "gzip_inflater.cc",
        "mem_lock_manager.cc",
        "hashed_nonce_generator.cc",
        "input_file_nonce_generator.cc",
        "file_system_lock_manager.cc",
        "threadsafe_lock_manager.cc",
        "mem_lock.cc",
        "nonce_generator.cc",
        "mem_lock_state.cc",
        "url_multipart_encoder.cc",
        "url_segment_encoder.cc",
        "simple_stats.cc",
        "simple_random.cc",
        "gflags.cc", # XXX
    ],
    hdrs = [
        "statistics_logger.h",
        "url_to_filename_encoder.h",
        "url_escaper.h",
        "brotli_inflater.h",
        "platform.h",
        "gzip_inflater.h",
        "copy_on_write.h",
        "mem_lock_manager.h",
        "hashed_nonce_generator.h",
        "input_file_nonce_generator.h",
        "categorized_refcount.h",
        "file_system_lock_manager.h",
        "threadsafe_lock_manager.h",
        "mem_lock.h",
        "nonce_generator.h",
        "mem_lock_state.h",
        "url_multipart_encoder.h",
        "url_segment_encoder.h",
        "simple_stats.h",
        "simple_random.h",
        "gflags.h", # XXX
        "grpc.h", # XXX
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=old-style-cast", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "//gurl:url",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html:html",
        "//pagespeed/kernel/thread:thread",
        "//pagespeed/kernel/http:http",
        "@brotli//:brotli_enc",
        "@brotli//:brotli_dec",
        "@zlib//:zlib",
        ":util_re2",
        "@grpc//:grpc++",
    ],
)

envoy_cc_test_library (
    name = "util_test_base",
    srcs = [
        "nonce_generator_test_base.cc",
        "lock_manager_spammer.cc",
        "mock_nonce_generator.cc",
    ],
    hdrs = [
        "nonce_generator_test_base.h",
        "lock_manager_spammer.h",
        "mock_nonce_generator.h",
    ],
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        ":util",
        "//pagespeed/kernel/base:pagespeed_gtest",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/thread:thread_test_base",
        "@re2//:re2",
        "@re2//:benchmark",
    ]
)

envoy_cc_test(
    name = "util_test",
    srcs = glob(["*_test.cc"]),
    repository = "@envoy",
    copts = ["-Wno-error=unused-parameter"],
    deps = [
        ":util_test_base",
    ],
)