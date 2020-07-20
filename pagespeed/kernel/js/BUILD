licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

genrule(
    name = "js_keyworks_cc_gperf",
    srcs = ["js_keywords.gperf"],
    outs = ["js_keywords.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

cc_library(
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
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util_re2",
        "@re2//:re2",
    ],
)

pagespeed_cc_test(
    name = "js_test",
    srcs = glob(["*_test.cc"]),
    data = glob(["testdata/third_party/*"]),
    deps = [
        ":js",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)