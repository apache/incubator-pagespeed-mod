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
    name = "util",
    srcs = [
        "statistics_logger.cc",
        #"url_to_filename_encoder.cc",
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
        "simple_random.cc",
        "mem_lock_state.cc",
        "url_multipart_encoder.cc",
        "url_segment_encoder.cc",
    ],
    hdrs = [
        "statistics_logger.h",
        #"url_to_filename_encoder.h",
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
        "simple_random.h",
        "mem_lock_state.h",
        "url_multipart_encoder.h",
        "url_segment_encoder.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=old-style-cast", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = [
        "@glog",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/html:html",
        "//pagespeed/kernel/http:http",
        "//pagespeed/kernel/thread:thread",
        "@brotli//:brotli_enc",
        "@brotli//:brotli_dec",
        "@zlib//:zlib",
    ],
)

envoy_cc_test_library (
    name = "util_test_base",
    srcs = [
        "simple_stats.cc",
        "nonce_generator_test_base.cc",
        "lock_manager_spammer.cc",
    ],
    hdrs = [
        "simple_stats.h",
        "re2.h",
        "nonce_generator_test_base.h",
        "lock_manager_spammer.h",
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