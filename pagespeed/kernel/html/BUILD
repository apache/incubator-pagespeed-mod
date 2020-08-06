load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache 2

genrule(
    name = "html_name_gperf",
    srcs = ["html_name.gperf"],
    outs = ["html_name.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

cc_library(
    name = "html",
    srcs = glob(["*.cc"]) + [":html_name_gperf"],
    hdrs = glob(["*.h"]),
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/http",
    ],
)

