load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":hiredis.bzl", "hiredis_build_rule")
load(":jsoncpp.bzl", "jsoncpp_build_rule")
load(":libpng.bzl", "libpng_build_rule")
load(":libwebp.bzl", "libwebp_build_rule")
load(":google_sparsehash.bzl", "google_sparsehash_build_rule")
load(":drp.bzl", "drp_build_rule")
load(":giflib.bzl", "giflib_build_rule")
load(":optipng.bzl", "optipng_build_rule")
load(":libjpeg_turbo.bzl", "libjpeg_turbo_build_rule")
load(":apr.bzl", "apr_build_rule")
load(":aprutil.bzl", "aprutil_build_rule")
load(":serf.bzl", "serf_build_rule")
load(":closure_compiler.bzl", "closure_library_rules")

ENVOY_COMMIT = "08464ecdc0c93846f3d039d0f0c6fed935f5bdc8"    # July 24th, 2020
BROTLI_COMMIT = "d6d98957ca8ccb1ef45922e978bb10efca0ea541"
HIREDIS_COMMIT = "0.14.1" # July 24th, 2020
JSONCPP_COMMIT = "1.9.3" # July 24th, 2020
RE2_COMMIT = "2020-07-06" # July 24th, 2020
LIBPNG_COMMIT = "b78804f9a2568b270ebd30eca954ef7447ba92f7"
LIBWEBP_COMMIT = "v0.6.1"
GOOGLE_SPARSEHASH_COMMIT = "6ff8809259d2408cb48ae4fa694e80b15b151af3"
GLOG_COMMIT = "0a2e5931bd5ff22fd3bf8999eb8ce776f159cda6" # July 24th, 2020
GFLAGS_COMMIT = "f7388c6655e699f777a5a74a3c9880b9cfaabe59" # July 24th, 2020
DRP_COMMIT = "21a7a0f0513b7adad7889ee68edcff49601e4a3a"
GIFLIB_COMMIT = "5.2.1" # July 24th, 2020
OPTIPNG_COMMIT = "e9a5bd640c45e99000f633a0997df89fddd20026"
LIBJPEG_TURBO_COMMIT = "14eba7addfdcf0699970fcbac225499858a167f2"
APR_COMMIT = "901ece0cd7cec29c050c58451a801bb125d09b6e" # July 24th, 2020
APRUTIL_COMMIT = "13ed779e56669007dffe9a27ffab3790b59cbfaa"
SERF_COMMIT = "95cf7547361549e192ac34d94d44c01c7a57b642"
CLOSURE_LIBRARY_COMMIT = "cd0e79408e4ec90e0da2eaee846a3400fae30445"

def mod_pagespeed_dependencies():
    http_archive(
        name = "envoy",
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
        sha256 = "027e0a01c5edf8ecc6a478396308bb2189d3bed920945ca50e6995d5b8289d69",
    )

    http_archive(
        name = "brotli",
        strip_prefix = "brotli-%s" % BROTLI_COMMIT,
        url = "https://github.com/google/brotli/archive/%s.tar.gz" % BROTLI_COMMIT,
        sha256 = "",
    )

    http_archive(
        name = "hiredis",
        strip_prefix = "hiredis-%s" % HIREDIS_COMMIT,
        url = "https://github.com/redis/hiredis/archive/v%s.tar.gz" % HIREDIS_COMMIT,
        build_file_content = hiredis_build_rule,
        sha256 = "2663b2aed9fd430507e30fc5e63274ee40cdd1a296026e22eafd7d99b01c8913",
    )

    http_archive(
        name = "jsoncpp",
        strip_prefix = "jsoncpp-%s" % JSONCPP_COMMIT,
        url = "https://github.com/open-source-parsers/jsoncpp/archive/%s.tar.gz" % JSONCPP_COMMIT,
        build_file_content = jsoncpp_build_rule,
        sha256 = "8593c1d69e703563d94d8c12244e2e18893eeb9a8a9f8aa3d09a327aa45c8f7d",
    )

    http_archive(
        name = "re2",
        strip_prefix = "re2-%s" % RE2_COMMIT,
        url = "https://github.com/google/re2/archive/%s.tar.gz" % RE2_COMMIT,
        build_file_content = """
# Copyright 2009 The RE2 Authors.  All Rights Reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

# Bazel (http://bazel.io/) BUILD file for RE2.

licenses(["notice"])

exports_files(["LICENSE"])

config_setting(
    name = "macos",
    values = {"cpu": "darwin"},
)

config_setting(
    name = "wasm",
    values = {"cpu": "wasm32"},
)

config_setting(
    name = "windows",
    values = {"cpu": "x64_windows"},
)

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "re2",
    srcs = [
        "re2/bitmap256.h",
        "re2/bitstate.cc",
        "re2/compile.cc",
        "re2/dfa.cc",
        "re2/filtered_re2.cc",
        "re2/mimics_pcre.cc",
        "re2/nfa.cc",
        "re2/onepass.cc",
        "re2/parse.cc",
        "re2/perl_groups.cc",
        "re2/pod_array.h",
        "re2/prefilter.cc",
        "re2/prefilter.h",
        "re2/prefilter_tree.cc",
        "re2/prefilter_tree.h",
        "re2/prog.cc",
        "re2/prog.h",
        "re2/re2.cc",
        "re2/regexp.cc",
        "re2/regexp.h",
        "re2/set.cc",
        "re2/simplify.cc",
        "re2/sparse_array.h",
        "re2/sparse_set.h",
        "re2/stringpiece.cc",
        "re2/tostring.cc",
        "re2/unicode_casefold.cc",
        "re2/unicode_casefold.h",
        "re2/unicode_groups.cc",
        "re2/unicode_groups.h",
        "re2/walker-inl.h",
        "util/logging.h",
        "util/mix.h",
        "util/mutex.h",
        "util/rune.cc",
        "util/strutil.cc",
        "util/strutil.h",
        "util/utf.h",
        "util/util.h",
    ],
    hdrs = [
        "re2/filtered_re2.h",
        "re2/re2.h",
        "re2/set.h",
        "re2/stringpiece.h",
    ],
    copts = select({
        ":wasm": [],
        ":windows": [],
        "//conditions:default": ["-pthread"],
    }),
    linkopts = select({
        # macOS doesn't need `-pthread' when linking and it appears that
        # older versions of Clang will warn about the unused command line
        # argument, so just don't pass it.
        ":macos": [],
        ":wasm": [],
        ":windows": [],
        "//conditions:default": ["-pthread"],
    }),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "testing",
    testonly = 1,
    srcs = [
        "re2/testing/backtrack.cc",
        "re2/testing/dump.cc",
        "re2/testing/exhaustive_tester.cc",
        "re2/testing/null_walker.cc",
        "re2/testing/regexp_generator.cc",
        "re2/testing/string_generator.cc",
        "re2/testing/tester.cc",
        "util/pcre.cc",
    ],
    hdrs = [
        "re2/testing/exhaustive_tester.h",
        "re2/testing/regexp_generator.h",
        "re2/testing/string_generator.h",
        "re2/testing/tester.h",
        "util/benchmark.h",
        "util/flags.h",
        "util/malloc_counter.h",
        "util/pcre.h",
        "util/test.h",
    ],
    deps = [":re2"],
)

cc_library(
    name = "test",
    testonly = 1,
    srcs = ["util/test.cc"],
    deps = [":testing"],
)

cc_test(
    name = "charclass_test",
    size = "small",
    srcs = ["re2/testing/charclass_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "compile_test",
    size = "small",
    srcs = ["re2/testing/compile_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "filtered_re2_test",
    size = "small",
    srcs = ["re2/testing/filtered_re2_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "mimics_pcre_test",
    size = "small",
    srcs = ["re2/testing/mimics_pcre_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "parse_test",
    size = "small",
    srcs = ["re2/testing/parse_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "possible_match_test",
    size = "small",
    srcs = ["re2/testing/possible_match_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "re2_arg_test",
    size = "small",
    srcs = ["re2/testing/re2_arg_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "re2_test",
    size = "small",
    srcs = ["re2/testing/re2_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "regexp_test",
    size = "small",
    srcs = ["re2/testing/regexp_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "required_prefix_test",
    size = "small",
    srcs = ["re2/testing/required_prefix_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "search_test",
    size = "small",
    srcs = ["re2/testing/search_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "set_test",
    size = "small",
    srcs = ["re2/testing/set_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "simplify_test",
    size = "small",
    srcs = ["re2/testing/simplify_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "string_generator_test",
    size = "small",
    srcs = ["re2/testing/string_generator_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "dfa_test",
    size = "large",
    srcs = ["re2/testing/dfa_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "exhaustive1_test",
    size = "large",
    srcs = ["re2/testing/exhaustive1_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "exhaustive2_test",
    size = "large",
    srcs = ["re2/testing/exhaustive2_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "exhaustive3_test",
    size = "large",
    srcs = ["re2/testing/exhaustive3_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "exhaustive_test",
    size = "large",
    srcs = ["re2/testing/exhaustive_test.cc"],
    deps = [":test"],
)

cc_test(
    name = "random_test",
    size = "large",
    srcs = ["re2/testing/random_test.cc"],
    deps = [":test"],
)

genrule(
  name = "renamed_main",
  srcs = ["util/benchmark.cc"],
  outs = ["util/benchmark_renamed_main.cc"],
  cmd = "sed 's/main/renamed_main/g' $< > $@",
)

cc_library(
    name = "benchmark",
    testonly = 1,
    srcs = [":renamed_main"],
    deps = [":testing"],
    visibility = ["//visibility:public"], # XXX(oschaaf): this line is modified from the original version
)

cc_binary(
    name = "regexp_benchmark",
    testonly = 1,
    srcs = ["re2/testing/regexp_benchmark.cc"],
    deps = [":benchmark"],
)

""",        
        sha256 = "",
    )

    http_archive(
        name = "libpng",
        strip_prefix = "libpng-%s" % LIBPNG_COMMIT,
        url = "https://github.com/glennrp/libpng/archive/%s.tar.gz" % LIBPNG_COMMIT,
        build_file_content = libpng_build_rule,
        sha256 = "b82a964705b5f32fa7c0b2c5a78d264c710f8c293fe7e60763b3381f8ff38d42",
    )

    http_archive(
        name = "libwebp",
        url = "https://chromium.googlesource.com/webm/libwebp/+archive/refs/tags/%s.tar.gz" % LIBWEBP_COMMIT,
        build_file_content = libwebp_build_rule,
        # TODO(oschaaf): fix sha256, fails in CI
        # sha256 = "b350385fe4d07bb95ce72259ce4cef791fb2d1ce1d77af1acea164c6c53f2907",
    )

    http_archive(
        name = "google_sparsehash",
        strip_prefix = "sparsehash-%s" % GOOGLE_SPARSEHASH_COMMIT,
        url = "https://github.com/sparsehash/sparsehash/archive/%s.tar.gz" % GOOGLE_SPARSEHASH_COMMIT,
        build_file_content = google_sparsehash_build_rule,
        sha256 = "4ae105acb6b53f957b6005fa103a9fd342c39dbc7c87673663e782325b8296b3",
    )

    http_archive(
        name = "glog",
        strip_prefix = "glog-%s" % GLOG_COMMIT,
        url = "https://github.com/google/glog/archive/%s.tar.gz" % GLOG_COMMIT,
        sha256 = "bae42ec37b50e156071f5b92d2ff09aa5ece56fd8c58d2175fc1ffea85137664",
    )

    http_archive(
        name = "com_github_gflags_gflags",
        strip_prefix = "gflags-%s" % GFLAGS_COMMIT,
        url = "https://github.com/gflags/gflags/archive/%s.tar.gz" % GFLAGS_COMMIT,
        sha256 = "ed82ef64389409e378fc6ae55b8b60f11a0b4bbb7e004d5ef9e791f40af19a6e",
    )

    http_archive(
        name = "drp",
        url = "https://github.com/apache/incubator-pagespeed-drp/archive/%s.tar.gz" % DRP_COMMIT,
        build_file_content = drp_build_rule,
        strip_prefix = "incubator-pagespeed-drp-%s" % DRP_COMMIT,
        sha256 = "9cc8b9a34a73d0e00ff404a4a75a5f386edd9f6d70a9afee5a76a2f41536fab1",
    )

    http_archive(
        name = "giflib",
        strip_prefix = "giflib-%s" % GIFLIB_COMMIT,
        url = "https://downloads.sourceforge.net/project/giflib/giflib-%s.tar.gz" % GIFLIB_COMMIT,
        build_file_content = giflib_build_rule,
        sha256 = "31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd",
    )

    http_archive(
        name = "optipng",
        strip_prefix = "incubator-pagespeed-optipng-%s" % OPTIPNG_COMMIT,
        url = "https://github.com/apache/incubator-pagespeed-optipng/archive/%s.tar.gz" % OPTIPNG_COMMIT,
        build_file_content = optipng_build_rule,
        sha256 = "e7e937b8c3085ca82389018fcb6a8bf3cb4ba2556921826e634614e1c7e0edd2",
    )

    http_archive(
        name = "libjpeg_turbo",
        url = "https://chromium.googlesource.com/chromium/deps/libjpeg_turbo/+archive/%s.tar.gz" % LIBJPEG_TURBO_COMMIT,
        build_file_content = libjpeg_turbo_build_rule,
        #sha256 = "ae529ea414c45045153e97a579163bcba677175029ee0245df7b0d5190fc9b28",
    )

    http_archive(
        name = "apr",
        strip_prefix = "apr-%s" % APR_COMMIT,
        url = "https://github.com/apache/apr/archive/%s.tar.gz" % APR_COMMIT,
        build_file_content = apr_build_rule,
        patches = [ "apr.patch" ],
        patch_args = ["-p1"],
        sha256 = "372b6a3424d8a3abbbf216bf6058e949f7b9da95e9caa57a9f5e82fe7528ca40",
    )

    http_archive(
        name = "aprutil",
        strip_prefix = "apr-util-%s" % APRUTIL_COMMIT,
        url = "https://github.com/apache/apr-util/archive/%s.tar.gz" % APRUTIL_COMMIT,
        build_file_content = aprutil_build_rule,
        sha256 = "9cf6d0e6fcc4783228dcee722897dadaadc601aef894c43a1e1514436eb4471a",
    )

    http_archive(
        name = "serf",
        strip_prefix = "serf-%s" % SERF_COMMIT,
        url = "https://github.com/apache/serf/archive/%s.tar.gz" % SERF_COMMIT,
        build_file_content = serf_build_rule,
        sha256 = "bcc7ddc4b82bf76ba862261cdb580db044ff62dcd523f8eb6acde87518b10257",
    )

    http_archive(
        name = "closure_library",
        strip_prefix = "closure-library-%s" % CLOSURE_LIBRARY_COMMIT,
        url = "https://github.com/google/closure-library/archive/%s.tar.gz" % CLOSURE_LIBRARY_COMMIT,
        sha256 = "bd5966814e6fdced42e97f8461fcbae52849cf589292a1c589585fcd9fdb3cd2",
        build_file_content = closure_library_rules,
    )

