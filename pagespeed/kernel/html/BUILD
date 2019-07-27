licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

genrule(
    name = "html_name_gperf",
    srcs = ["html_name.gperf"],
    outs = ["html_name.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

cc_library(
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
    visibility = ["//visibility:public"],
    copts = [
        '-std=c++14',        
    ],    
    deps = [
        "//gurl:url",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/http:http",
#        "//net/instaweb/genfiles:htmlparse",
    ],
)

pagespeed_cc_test_library (
    name = "html_test_base",
    srcs = [
        "html_parse_test_base.cc",
    ],
    hdrs = [
        "html_parse_test_base.h",
        "disable_test_filter.h",
        "html_testing_peer.h",
    ],
    deps = [
        ":html",
        "//pagespeed/kernel/base:kernel_test_util",
        "@re2//:benchmark",
    ]
)

pagespeed_cc_test(
    name = "html_test",
    srcs = glob(["*_test.cc"]),
    deps = [
        ":html",
        "//pagespeed/kernel/base:pagespeed_gtest",
        ":html_test_base"
    ],
)