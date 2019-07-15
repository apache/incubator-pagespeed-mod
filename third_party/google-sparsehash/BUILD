licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "google_sparsehash_gen",
    hdrs = [
        "gen/arch/linux/x64/include/google/sparsehash/sparseconfig.h",
    ],
    repository = "@envoy",
    strip_include_prefix = "gen/arch/linux/x64/include",
    visibility = ["//visibility:public"],
    deps = ["@google_sparsehash"],
)
