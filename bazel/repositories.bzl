load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":zlib.bzl", "zlib_build_rule")
load(":hiredis.bzl", "hiredis_build_rule")
load(":jsoncpp.bzl", "jsoncpp_build_rule")
load(":icu.bzl", "icu_build_rule")

ENVOY_COMMIT = "master"
BROTLI_COMMIT = "882f41850b679c1ff4a3804d5515d142a5807376"
ZLIB_COMMIT = "cacf7f1d4e3d44d871b605da3b647f07d718623f"
HIREDIS_COMMIT = "010756025e8cefd1bc66c6d4ed3b1648ef6f1f95"
JSONCPP_COMMIT = "7165f6ac4c482e68475c9e1dac086f9e12fff0d0"
RE2_COMMIT = "848dfb7e1d7ba641d598cb66f81590f3999a555a"
ICU_COMMIT = "46a834e2ebcd7c5b60f49350a166d8b9e4a24c0e"

def mod_pagespeed_dependencies():
    http_archive(
        name = "envoy",
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
    )

    http_archive(
        name = "brotli",
        strip_prefix = "brotli-%s" % BROTLI_COMMIT,
        url = "https://github.com/google/brotli/archive/%s.tar.gz" % BROTLI_COMMIT,
    )

    http_archive(
        # TODO : Rename library as per bazel naming conventions
        name = "zlib",
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

