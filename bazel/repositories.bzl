load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

ENVOY_COMMIT = "master"
BROTLI_COMMIT = "882f41850b679c1ff4a3804d5515d142a5807376"

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

