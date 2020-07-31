load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

genrule(
    name = "html_name_gperf",
    srcs = ["html_name.gperf"],
    outs = ["html_name.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

cc_library(
    name = "html",
    srcs = [
        "amp_document_filter.cc",
        "canonical_attributes.cc",
        "collapse_whitespace_filter.cc",
        "doctype.cc",
        "elide_attributes_filter.cc",
        "empty_html_filter.cc",
        "explicit_close_tag.cc",
        "html_attribute_quote_removal.cc",
        "html_element.cc",
        "html_event.cc",
        "html_filter.cc",
        "html_keywords.cc",
        "html_lexer.cc",
        "html_node.cc",
        "html_parse.cc",
        "html_writer_filter.cc",
        "remove_comments_filter.cc",
        ":html_name_gperf",
    ],
    hdrs = [
        "amp_document_filter.h",
        "canonical_attributes.h",
        "collapse_whitespace_filter.h",
        "doctype.h",
        "elide_attributes_filter.h",
        "empty_html_filter.h",
        "explicit_close_tag.h",
        "html_attribute_quote_removal.h",
        "html_element.h",
        "html_event.h",
        "html_filter.h",
        "html_keywords.h",
        "html_lexer.h",
        "html_name.h",
        "html_node.h",
        "html_parse.h",
        "html_writer_filter.h",
        "remove_comments_filter.h",
    ],
    copts = [
        "-std=c++17",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/http",
        "@com_google_absl//absl/strings",
    ],
)

pagespeed_cc_test_library(
    name = "html_test_base",
    srcs = [
        "html_parse_test_base.cc",
    ],
    hdrs = [
        "disable_test_filter.h",
        "html_parse_test_base.h",
        "html_testing_peer.h",
    ],
    deps = [
        ":html",
        "//pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "html_test",
    srcs = glob(
        ["*_test.cc"],
        exclude = glob(["*speed_test.cc"]),
    ),
    deps = [
        ":html",
        ":html_test_base",
    ],
)

pagespeed_cc_test(
    name = "html_speed_test",
    srcs = glob(["*speed_test.cc"]),
    deps = [
        ":html",
        ":html_test_base",
        "//pagespeed/kernel/base:benchmark",
    ],
)
