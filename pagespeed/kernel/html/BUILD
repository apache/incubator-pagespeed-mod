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
    name = "html_name_gperf",
    srcs = ["html_name.gperf"],
    outs = ["html_name.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

envoy_cc_library(
    name = "html",
    srcs = [
        "html_keywords.cc",
        ":html_name_gperf",
        "elide_attributes_filter.cc",
        "remove_comments_filter.cc",
        "html_attribute_quote_removal.cc",
        "html_parse.cc",
        "empty_html_filter.cc",
        "collapse_whitespace_filter.cc",
        "amp_document_filter.cc",
        "canonical_attributes.cc",
        "doctype.cc",
        "html_filter.cc",
        "html_node.cc",
        "html_element.cc",
        "html_writer_filter.cc",
        "explicit_close_tag.cc",
        "html_event.cc",
        "html_lexer.cc",
    ],
    hdrs = [
        "html_keywords.h",
        "html_name.h",
        "elide_attributes_filter.h",
        "remove_comments_filter.h",
        "html_attribute_quote_removal.h",
        "html_parse.h",
        "empty_html_filter.h",
        "collapse_whitespace_filter.h",
        "amp_document_filter.h",
        "canonical_attributes.h",
        "doctype.h",
        "html_filter.h",
        "html_node.h",
        "html_element.h",
        "html_writer_filter.h",
        "explicit_close_tag.h",
        "html_event.h",
        "html_lexer.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-inconsistent-missing-override",
        "-Wno-missing-field-initializers",
    ],
    deps = [
        "//gurl:url",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/http:http",
#        "//net/instaweb/genfiles:htmlparse",
    ],
)

envoy_cc_test_library (
    name = "html_test_base",
    srcs = [
        "html_parse_test_base.cc",
    ],
    hdrs = [
        "html_parse_test_base.h",
        "disable_test_filter.h",
        "html_testing_peer.h",
    ],
    repository = "@envoy",
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-inconsistent-missing-override",
        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":html",
        "//pagespeed/kernel/base:kernel_test_util",
        "@re2//:benchmark",
    ]
)

envoy_cc_test(
    name = "html_test",
    repository = "@envoy",
    srcs = glob(["*_test.cc"]),
    copts = [
        "-Wno-error=old-style-cast",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
        "-Wno-error=inconsistent-missing-override",
        "-Wno-inconsistent-missing-override",
        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":html",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":html_test_base"
    ],
)