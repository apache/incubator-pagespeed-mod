load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

genrule(
    name = "js_keyworks_cc_gperf",
    srcs = ["js_keywords.gperf"],
    outs = ["js_keywords.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

cc_library(
    name = "js",
    srcs = [
        "js_minify.cc",
        "js_tokenizer.cc",
        ":js_keyworks_cc_gperf",
    ],
    hdrs = [
        "js_keywords.h",
        "js_minify.h",
        "js_tokenizer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util_re2",
    ],
)
