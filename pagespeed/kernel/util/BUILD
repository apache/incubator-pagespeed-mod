load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "util_re2",
    hdrs = ["re2.h"],
    deps = ["@com_googlesource_code_re2//:re2"],
    visibility = ["//visibility:public"],
)

# Brotli isn't used, and building is disabled for it
# because the dep. build broke.
cc_library(
    name = "util",
    srcs = [
        "brotli_inflater.cc",
        "file_system_lock_manager.cc",
        "gflags.cc",
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
        "gflags.h",
        "grpc.h",
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
    visibility = ["//visibility:public"],
    deps = [
        ":util_re2",
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
