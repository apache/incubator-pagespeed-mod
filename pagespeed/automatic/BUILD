load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

licenses(["notice"])  # Apache 2

cc_library(
    name = "static_rewriter_lib",
    srcs = ["static_rewriter.cc"],
    hdrs = ["static_rewriter.h"],
    visibility = ["//visibility:public"],
    deps = ["//pagespeed/system"],

)

cc_binary(
    name = "static_rewriter",
    srcs = ["static_rewriter_main.cc"],
    deps = ["//pagespeed/automatic:static_rewriter_lib"],
)

cc_library(
    name = "automatic",
    srcs = [
        "html_detector.cc",
        "proxy_fetch.cc",
        "proxy_interface.cc",
    ],
    hdrs = [
        "html_detector.h",
        "proxy_fetch.h",
        "proxy_interface.h",
    ],
    linkstatic = True,
    visibility = ["//visibility:public"],
    deps = [
        "//net/instaweb/http",
        "//net/instaweb/rewriter",
        "//net/instaweb/util",
    ],
)
