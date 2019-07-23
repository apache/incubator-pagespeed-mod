licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

genrule(
    name = "js_keyworks_cc_gperf",
    srcs = ["js_keywords.gperf"],
    outs = ["js_keywords.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

envoy_cc_library(
    name = "js",
    srcs = [
        ":js_keyworks_cc_gperf",
        "js_minify.cc",
        "js_tokenizer.cc",
    ],
    hdrs = [
        "js_minify.h",
        "js_tokenizer.h",
        "js_keywords.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=missing-field-initializers",
    ],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util_re2",
        "@re2//:re2",
    ],
)

envoy_cc_test(
    name = "js_test",
    repository = "@envoy",
    srcs = glob(["*_test.cc"]),
    data = glob(["testdata/third_party/*"]),
    copts = ["-Wno-error=unused-parameter","-Wno-error=unused-private-field","-Wno-error=unused-function"],
    deps = [
        ":js",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)