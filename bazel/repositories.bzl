load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":zlib.bzl", "zlib_build_rule")
load(":hiredis.bzl", "hiredis_build_rule")
load(":jsoncpp.bzl", "jsoncpp_build_rule")
load(":icu.bzl", "icu_build_rule")
load(":libpng.bzl", "libpng_build_rule")
load(":libwebp.bzl", "libwebp_build_rule")
load(":google_sparsehash.bzl", "google_sparsehash_build_rule")
load(":protobuf.bzl", "protobuf_build_rule")
load(":gurl.bzl", "gurl_build_rule")
load(":drp.bzl", "drp_build_rule")
load(":giflib.bzl", "giflib_build_rule")
load(":optipng.bzl", "optipng_build_rule")
load(":libjpeg_turbo.bzl", "libjpeg_turbo_build_rule")
load(":apr.bzl", "apr_build_rule")
load(":aprutil.bzl", "aprutil_build_rule")
load(":serf.bzl", "serf_build_rule")
load(":closure_compiler.bzl", "closure_library_rules")

ENVOY_COMMIT = "e1cd4cca46179562cf80de7f81774f4dc6354085"
BROTLI_COMMIT = "d6d98957ca8ccb1ef45922e978bb10efca0ea541"
ZLIB_COMMIT = "cacf7f1d4e3d44d871b605da3b647f07d718623f"
HIREDIS_COMMIT = "010756025e8cefd1bc66c6d4ed3b1648ef6f1f95"
JSONCPP_COMMIT = "7165f6ac4c482e68475c9e1dac086f9e12fff0d0"
RE2_COMMIT = "848dfb7e1d7ba641d598cb66f81590f3999a555a"
ICU_COMMIT = "46a834e2ebcd7c5b60f49350a166d8b9e4a24c0e"
LIBPNG_COMMIT = "b78804f9a2568b270ebd30eca954ef7447ba92f7"
LIBWEBP_COMMIT = "v0.6.1"
GOOGLE_SPARSEHASH_COMMIT = "6ff8809259d2408cb48ae4fa694e80b15b151af3"
GLOG_COMMIT = "96a2f23dca4cc7180821ca5f32e526314395d26a"
GFLAGS_COMMIT = "e171aa2d15ed9eb17054558e0b3a6a413bb01067"
GURL_COMMIT = "77.0.3855.1"
DRP_COMMIT = "f353346a9087538cd8e30a04b51e3f961c4a8fd8"
PROTOBUF_COMMIT = "e8ae137c96444ea313485ed1118c5e43b2099cf1"
GIFLIB_COMMIT = "c9a54afc6feb1e2cd0626a49b2c9e50015e96dbe"
OPTIPNG_COMMIT = "e9a5bd640c45e99000f633a0997df89fddd20026"
GRPC_COMMIT = "99fd5c391a435e2677b6caa7fd25089c484a32ab"
LIBJPEG_TURBO_COMMIT = "14eba7addfdcf0699970fcbac225499858a167f2"
APR_COMMIT = "a194e7afd16d2bd88c5394dfb6d066614161b1ed"
APRUTIL_COMMIT = "13ed779e56669007dffe9a27ffab3790b59cbfaa"
SERF_COMMIT = "95cf7547361549e192ac34d94d44c01c7a57b642"
CLOSURE_LIBRARY_COMMIT = "cd0e79408e4ec90e0da2eaee846a3400fae30445"

def mod_pagespeed_dependencies():
    http_archive(
        name = "envoy",
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
        sha256 = "16b63facab69d31accf06e6991627e7c6681749cae919c2d3c65217882127172",
    )

    http_archive(
        name = "brotli",
        strip_prefix = "brotli-%s" % BROTLI_COMMIT,
        url = "https://github.com/google/brotli/archive/%s.tar.gz" % BROTLI_COMMIT,
        sha256 = "",
    )

    http_archive(
        # TODO : Rename library as per bazel naming conventions
        name = "xzlib",
        strip_prefix = "zlib-%s" % ZLIB_COMMIT,
        url = "https://github.com/madler/zlib/archive/%s.tar.gz" % ZLIB_COMMIT,
        build_file_content = zlib_build_rule,
        sha256 = "6d4d6640ca3121620995ee255945161821218752b551a1a180f4215f7d124d45",
    )

    http_archive(
        name = "hiredis",
        strip_prefix = "hiredis-%s" % HIREDIS_COMMIT,
        url = "https://github.com/redis/hiredis/archive/%s.tar.gz" % HIREDIS_COMMIT,
        build_file_content = hiredis_build_rule,
        sha256 = "b239f8de6073e4eaea4be6ba4bf20516f33d7bedef1fc89287de60a0512f13bd",
    )

    http_archive(
        name = "jsoncpp",
        strip_prefix = "jsoncpp-%s" % JSONCPP_COMMIT,
        url = "https://github.com/open-source-parsers/jsoncpp/archive/%s.tar.gz" % JSONCPP_COMMIT,
        build_file_content = jsoncpp_build_rule,
        sha256 = "9757f515b42b86ebd08b13bdfde7c27ca7436186d9b01ef1fa5cbc194e1f2764",
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
    name = "darwin",
    values = {"cpu": "darwin"},
)

config_setting(
    name = "windows",
    values = {"cpu": "x64_windows"},
)

config_setting(
    name = "windows_msvc",
    values = {"cpu": "x64_windows_msvc"},
)

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
        "re2/stringpiece.cc",
        "re2/tostring.cc",
        "re2/unicode_casefold.cc",
        "re2/unicode_casefold.h",
        "re2/unicode_groups.cc",
        "re2/unicode_groups.h",
        "re2/walker-inl.h",
        "util/flags.h",
        "util/logging.h",
        "util/mix.h",
        "util/mutex.h",
        "util/pod_array.h",
        "util/rune.cc",
        "util/sparse_array.h",
        "util/sparse_set.h",
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
        ":windows": [],
        ":windows_msvc": [],
        "//conditions:default": ["-pthread"],
    }),
    linkopts = select({
        # Darwin doesn't need `-pthread' when linking and it appears that
        # older versions of Clang will warn about the unused command line
        # argument, so just don't pass it.
        ":darwin": [],
        ":windows": [],
        ":windows_msvc": [],
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

load(":re2_test.bzl", "re2_test")

re2_test(
    "charclass_test",
    size = "small",
)

re2_test(
    "compile_test",
    size = "small",
)

re2_test(
    "filtered_re2_test",
    size = "small",
)

re2_test(
    "mimics_pcre_test",
    size = "small",
)

re2_test(
    "parse_test",
    size = "small",
)

re2_test(
    "possible_match_test",
    size = "small",
)

re2_test(
    "re2_arg_test",
    size = "small",
)

re2_test(
    "re2_test",
    size = "small",
)

re2_test(
    "regexp_test",
    size = "small",
)

re2_test(
    "required_prefix_test",
    size = "small",
)

re2_test(
    "search_test",
    size = "small",
)

re2_test(
    "set_test",
    size = "small",
)

re2_test(
    "simplify_test",
    size = "small",
)

re2_test(
    "string_generator_test",
    size = "small",
)

re2_test(
    "dfa_test",
    size = "large",
)

re2_test(
    "exhaustive1_test",
    size = "large",
)

re2_test(
    "exhaustive2_test",
    size = "large",
)

re2_test(
    "exhaustive3_test",
    size = "large",
)

re2_test(
    "exhaustive_test",
    size = "large",
)

re2_test(
    "random_test",
    size = "large",
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
        sha256 = "76a20451bec4e3767c3014c8e2db9ff93cbdda28e98e7bb36af41a52dc9c3dea",
    )

    http_archive(
        name = "icu",
        strip_prefix = "incubator-pagespeed-icu-%s" % ICU_COMMIT,
        url = "https://github.com/apache/incubator-pagespeed-icu/archive/%s.tar.gz" % ICU_COMMIT,
        build_file_content = icu_build_rule,
        # TODO(oschaaf): like the commits, it would be great to have the sha256 declarations at the top of the file.
        sha256 = "e596ba1ff6feb7179733b71cbc793a777a388d1f6882a4d8656b74cb381c8e22",
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
        sha256 = "a72da1c10f4f1f678ec3e77aeaf9b3026ec0d2f66d20ded33753ab5940ed218e",
    )

    http_archive(
        name = "com_github_gflags_gflags",
        strip_prefix = "gflags-%s" % GFLAGS_COMMIT,
        url = "https://github.com/gflags/gflags/archive/%s.tar.gz" % GFLAGS_COMMIT,
        sha256 = "b20f58e7f210ceb0e768eb1476073d0748af9b19dfbbf53f4fd16e3fb49c5ac8",
    )

    http_archive(
        name = "protobuf",
        strip_prefix = "protobuf-%s" % PROTOBUF_COMMIT,
        url = "https://github.com/protocolbuffers/protobuf/archive/%s.tar.gz" % PROTOBUF_COMMIT,
        build_file_content = protobuf_build_rule,
        sha256 = "c8d3fde0c3f4c5958f31f501f84d4d313f0577609aabce2ab6f58da2e4c6fbbc",
    )

    http_archive(
        name = "url",
        url = "https://chromium.googlesource.com/chromium/src/+archive/refs/tags/%s/url.tar.gz" % GURL_COMMIT,
        build_file_content = gurl_build_rule,
        sha256 = "",
    )


    http_archive(
        name = "drp",
        url = "https://github.com/apache/incubator-pagespeed-drp/archive/%s.tar.gz" % DRP_COMMIT,
        build_file_content = drp_build_rule,
        strip_prefix = "incubator-pagespeed-drp-%s" % DRP_COMMIT,
        sha256 = "027870bd54b4ee856c560955d5790257bacac8ae7d3f143ed8cd425674cc45fd",
    )

    
    http_archive(
        name = "giflib",
        strip_prefix = "giflib-mirror-%s" % GIFLIB_COMMIT,
        url = "https://github.com/We-Amp/giflib-mirror/archive/%s.tar.gz" % GIFLIB_COMMIT,
        build_file_content = giflib_build_rule,
        sha256 = "90452882419956d8005b0b6cc51e02488e3016e809431d794f42ab1173a5df02",
    )

    http_archive(
        name = "optipng",
        strip_prefix = "incubator-pagespeed-optipng-%s" % OPTIPNG_COMMIT,
        url = "https://github.com/apache/incubator-pagespeed-optipng/archive/%s.tar.gz" % OPTIPNG_COMMIT,
        build_file_content = optipng_build_rule,
        sha256 = "e7e937b8c3085ca82389018fcb6a8bf3cb4ba2556921826e634614e1c7e0edd2",
    )

    http_archive(
        name = "grpc",
        strip_prefix = "grpc-%s" % GRPC_COMMIT,
        url = "https://github.com/grpc/grpc/archive/%s.tar.gz" % GRPC_COMMIT,
        sha256 = "1528bc20e9bb2921964d354116e1650d6e14b88eef7a6263c4469705ceaef8aa",
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
        sha256 = "81f100b46670014b9ad62acaa2df653a225408a9d8d90c4727d592941952b0ec",
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

