licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "htmlparse",
    hdrs = [
        # XXX(oschaaf): need to generate this with gperf
        #"htmlparse/public/html_agent.h",
    ],
    srcs = [
        "htmlparse/html_name.cc",
        "htmlparse/html_agent.cc",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    deps = ["@mod_pagespeed//base:log_shim"],
)
