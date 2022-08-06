load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

licenses(["notice"])  # Apache 2

genrule(
    name = "bot_checker_cc_gperf",
    srcs = ["bot_checker.gperf"],
    outs = ["bot_checker.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

proto_library(
    name = "http_proto",
    srcs = [
        "http.proto",
        "image_types.proto",
    ],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "http_proto_cc",
    deps = [":http_proto"],
)

cc_library(
    name = "http",
    srcs = [
        "caching_headers.cc",
        "content_type.cc",
        "data_url.cc",
        "domain_registry.cc",
        "google_url.cc",
        "headers.cc",
        "http_names.cc",
        "http_options.cc",
        "query_params.cc",
        "request_headers.cc",
        "response_headers.cc",
        "response_headers_parser.cc",
        "semantic_type.cc",
        "user_agent_matcher.cc",
        "user_agent_normalizer.cc",
        ":bot_checker_cc_gperf",
    ],
    hdrs = [
        "bot_checker.h",
        "caching_headers.h",
        "content_type.h",
        "data_url.h",
        "domain_registry.h",
        "google_url.h",
        "headers.h",
        "http_names.h",
        "http_options.h",
        "query_params.h",
        "request_headers.h",
        "response_headers.h",
        "response_headers_parser.h",
        "semantic_type.h",
        "user_agent_matcher.h",
        "user_agent_normalizer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":http_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util_re2",
        "@com_googlesource_googleurl//url",
        "@drp",
    ],
)
