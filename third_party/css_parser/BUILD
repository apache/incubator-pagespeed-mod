load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
)

licenses(["notice"])  # Apache 2

cc_library(
    name = "utf",
    srcs = [
        "src/third_party/utf/rune.c",
        "src/third_party/utf/runestrcat.c",
        "src/third_party/utf/runestrchr.c",
        "src/third_party/utf/runestrcmp.c",
        "src/third_party/utf/runestrcpy.c",
        "src/third_party/utf/runestrecpy.c",
        "src/third_party/utf/runestrlen.c",
        "src/third_party/utf/runestrncat.c",
        "src/third_party/utf/runestrncmp.c",
        "src/third_party/utf/runestrncpy.c",
        "src/third_party/utf/runestrrchr.c",
        "src/third_party/utf/runestrstr.c",
        "src/third_party/utf/runetype.c",
        "src/third_party/utf/utf.h",
        "src/third_party/utf/utfdef.h",
        "src/third_party/utf/utfecpy.c",
        "src/third_party/utf/utflen.c",
        "src/third_party/utf/utfnlen.c",
        "src/third_party/utf/utfrrune.c",
        "src/third_party/utf/utfrune.c",
        "src/third_party/utf/utfutf.c",
    ],
    hdrs = [
        "src/third_party/utf/runetypebody.c",
        "src/third_party/utf/utf.h",
        "src/third_party/utf/utfdef.h",
    ]
)

genrule(
    name = "css_parser_identifier_gperf",
    srcs = ["src/webutil/css/identifier.gperf"],
    outs = ["src/webutil/css/identifier.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

genrule(
    name = "css_parser_property_gperf",
    srcs = ["src/webutil/css/property.gperf"],
    outs = ["src/webutil/css/property.gp.cc"],
    cmd = ("gperf -m10 $< > $@"),
)

pagespeed_cc_test(
    name = "css_parser_test",
    srcs = [
        "src/webutil/css/identifier_test.cc",
        "src/webutil/css/parser_unittest.cc",
        "src/webutil/css/property_test.cc",
        "src/webutil/css/tostring_test.cc",
        "src/webutil/css/util_test.cc",
    ],
    deps = [
        ":css_parser",
    ],
)

cc_library(
    name = "css_parser",
    srcs = [
        ":css_parser_identifier_gperf",
        ":css_parser_property_gperf",
        "src/webutil/css/media.cc",
        "src/webutil/css/parser.cc",
        "src/webutil/css/selector.cc",
        "src/webutil/css/string_util.cc",
        "src/webutil/css/tostring.cc",
        "src/webutil/css/util.cc",
        "src/webutil/css/value.cc",
        "src/webutil/html/htmlcolor.cc",
        "src/webutil/html/htmltagenum.cc",
        "src/webutil/html/htmltagindex.cc",

        # UnicodeText
        "src/util/utf8/internal/unicodetext.cc",
        "src/util/utf8/internal/unilib.cc",

        # Supporting interfaces.
        "src/strings/ascii_ctype.cc",
        "src/strings/stringpiece_utils.cc",
    ],
    hdrs = [
        "src/string_using.h",
        "src/strings/memutil.h",
        "src/strings/ascii_ctype.h",
        "src/strings/stringpiece_utils.h",
        "src/strings/stringpiece.h",
        "src/strings/strutil.h",
        "src/strings/join.h",
        "src/strings/case.h",
        "src/strings/escaping.h",
        "src/util/utf8/public/unicodetext.h",
        "src/util/utf8/public/unilib.h",
        "src/util/utf8/public/config.h",
        "src/webutil/css/media.h",
        "src/webutil/css/tostring.h",
        "src/webutil/css/identifier.h",
        "src/webutil/css/util.h",
        "src/webutil/css/string.h",
        "src/webutil/css/property.h",
        "src/webutil/css/value.h",
        "src/webutil/css/selector.h",
        "src/webutil/css/fallthrough_intended.h",
        "src/webutil/html/htmltagenum.h",
        "src/webutil/html/htmltagindex.h",
        "src/webutil/html/htmlcolor.h",
        "src/webutil/css/string_util.h",
        "src/webutil/css/parser.h",
        "src/base/googleinit.h",
        "src/base/integral_types.h",
        "src/util/utf8/public/unilib_utf8_utils.h",
    ] + glob(["src/util/gtl/*.h"]),
    copts = [
        "-funsigned-char",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":utf",
        "//pagespeed/kernel/base:gtest_prod",
        "//third_party/google-sparsehash:google_sparsehash_gen",
        "@com_google_absl//absl/strings:str_format",
        "@mod_pagespeed//base:log_shim",
    ],
)
