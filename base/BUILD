licenses(["notice"])  # Apache 2

cc_library(
    name = "log_shim",
    hdrs = [
        "logging.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@glog",
    ],
)

cc_library(
    name = "base",
    srcs = [
        "debug/alias.cc",
        "strings/string16.cc",
        "strings/string_piece.cc",
        "strings/string_util.cc",
        "strings/string_util_constants.cc",
        "strings/stringprintf.cc",
        "strings/utf_string_conversion_utils.cc",
        "strings/utf_string_conversions.cc",
        "third_party/icu/icu_utf.cc",
    ],
    hdrs = [
        "base_export.h",
        "compiler_specific.h",
        "component_export.h",
        "debug/alias.h",
        "debug/leak_annotations.h",
        "export_template.h",
        "immediate_crash.h",
        "macros.h",
        "no_destructor.h",
        "numerics/safe_conversions.h",
        "numerics/safe_conversions_impl.h",
        "optional.h",
        "scoped_clear_last_error.h",
        "stl_util.h",
        "strings/char_traits.h",
        "strings/string16.h",
        "strings/string_number_conversions.h",
        "strings/string_piece.h",
        "strings/string_piece_forward.h",
        "strings/string_util.h",
        "strings/string_util_posix.h",
        "strings/stringprintf.h",
        "strings/utf_string_conversion_utils.h",
        "strings/utf_string_conversions.h",
        "template_util.h",
        "third_party/icu/icu_utf.h",
        "thread_annotations.h",
    ],
    visibility = ["//visibility:public"],
    copts = [
        "-std=c++14",
    ],
    deps = [
        "//build",
        "@icu",
        "@mod_pagespeed//base:log_shim",
    ],
)
