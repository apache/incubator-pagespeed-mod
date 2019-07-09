load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load(":zlib.bzl", "zlib_build_rule")
load(":hiredis.bzl", "hiredis_build_rule")

ENVOY_COMMIT = "master"
BROTLI_COMMIT = "882f41850b679c1ff4a3804d5515d142a5807376"
ZLIB_COMMIT = "cacf7f1d4e3d44d871b605da3b647f07d718623f"
HIREDIS_COMMIT = "010756025e8cefd1bc66c6d4ed3b1648ef6f1f95"

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

