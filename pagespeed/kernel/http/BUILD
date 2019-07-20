licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

genrule(
    name = "bot_checker_cc_gperf",
    srcs = ["bot_checker.gperf"],
    outs = ["bot_checker.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

proto_library(
    name = "http_proto",
    srcs = ["http.proto", "image_types.proto"],
)

cc_proto_library(
    name = "http_proto_cc",
    deps = [":http_proto"],
)

envoy_cc_library(
    name = "http",
    srcs = [
        "google_url.cc",
        "query_params.cc",
        "caching_headers.cc",
        "http_names.cc",
        ":bot_checker_cc_gperf",
        "user_agent_normalizer.cc",
        "semantic_type.cc",
        "response_headers.cc",
        "request_headers.cc",
        "data_url.cc",
        "domain_registry.cc",
        "content_type.cc",
        "headers.cc",
        "http_options.cc",
        "user_agent_matcher.cc",
        "response_headers_parser.cc",
    ],
    hdrs = [
        "google_url.h",
        "query_params.h",
        "bot_checker.h",
        "caching_headers.h",
        "http_names.h",
        "user_agent_normalizer.h",
        "user_agent_matcher.h",
        "semantic_type.h",
        "response_headers.h",
        "request_headers.h",
        "data_url.h",
        "domain_registry.h",
        "content_type.h",
        "headers.h",
        "http_options.h",
        "response_headers_parser.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=unused-function"
    ],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util_re2",
        "//gurl:url",
        "//base:base",
        "@drp//:drp",
        "@re2//:re2",
        ":http_proto_cc",
    ],
)

envoy_cc_test_library (
    name = "http_test_base",
    srcs = [
        "user_agent_matcher_test_base.cc",
    ],
    hdrs = [
        "user_agent_matcher_test_base.h",
    ],
    repository = "@envoy",
    deps = [
        ":http",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/util:util_test_base",
    ]
)

envoy_cc_test(
    name = "http_test",
    repository = "@envoy",
    srcs = glob(["*_test.cc"]),
    copts = ["-Wno-error=unused-parameter","-Wno-error=unused-private-field","-Wno-error=unused-function"],
    deps = [
        ":http",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":http_test_base"
    ],
)