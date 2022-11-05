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

ENVOY_COMMIT = "9ae348cc43e0341ec7eadd6a4addce587afe1b27"  # Nov 5th, 2022
ENVOY_SHA = "f7215f2e2014c2d4123c5f7a2ba7054fdd33d8fa4ad2977c5bf45d9e231dea6d"

BROTLI_COMMIT = "d6d98957ca8ccb1ef45922e978bb10efca0ea541"
BROTLI_SHA = "ba8be5d701b369f86d14f3701c81d6bf6c6c34015c183ff98352c12ea5f5226b"
HIREDIS_COMMIT = "0.14.1"  # July 24th, 2020
HIREDIS_SHA = "2663b2aed9fd430507e30fc5e63274ee40cdd1a296026e22eafd7d99b01c8913"
JSONCPP_COMMIT = "1.9.3"  # July 24th, 2020
JSONCPP_SHA = "8593c1d69e703563d94d8c12244e2e18893eeb9a8a9f8aa3d09a327aa45c8f7d"
LIBPNG_COMMIT = "1.6.37"  # July 24th, 2020
LIBPNG_SHA = "ca74a0dace179a8422187671aee97dd3892b53e168627145271cad5b5ac81307"
LIBWEBP_COMMIT = "1.1.0"  # July 24th, 2020
LIBWEBP_SHA = "424faab60a14cb92c2a062733b6977b4cc1e875a6398887c5911b3a1a6c56c51"
GOOGLE_SPARSEHASH_COMMIT = "6ff8809259d2408cb48ae4fa694e80b15b151af3"
GOOGLE_SPARSEHASH_SHA = "4ae105acb6b53f957b6005fa103a9fd342c39dbc7c87673663e782325b8296b3"
GLOG_COMMIT = "0a2e5931bd5ff22fd3bf8999eb8ce776f159cda6"  # July 24th, 2020
GLOG_SHA = "bae42ec37b50e156071f5b92d2ff09aa5ece56fd8c58d2175fc1ffea85137664"
GFLAGS_COMMIT = "f7388c6655e699f777a5a74a3c9880b9cfaabe59"  # July 24th, 2020
GFLAGS_SHA = "ed82ef64389409e378fc6ae55b8b60f11a0b4bbb7e004d5ef9e791f40af19a6e"
DRP_COMMIT = "21a7a0f0513b7adad7889ee68edcff49601e4a3a"
DRP_SHA = "9cc8b9a34a73d0e00ff404a4a75a5f386edd9f6d70a9afee5a76a2f41536fab1"
GIFLIB_COMMIT = "5.2.1"  # July 24th, 2020
GIFLIB_SHA = "31da5562f44c5f15d63340a09a4fd62b48c45620cd302f77a6d9acf0077879bd"
OPTIPNG_COMMIT = "0.7.7"  # July 24th, 2020
OPTIPNG_SHA = "4f32f233cef870b3f95d3ad6428bfe4224ef34908f1b42b0badf858216654452"
LIBJPEG_TURBO_COMMIT = "ab7cd970a83609f98e8542cea8b81e8d92ddab83"  # July 24th, 2020

# TODO(oschaaf): this diverges when run in Travis?!
#LIBJPEG_TURBO_SHA = "e1dfd8f73dd146f44d0dc79709ff2f3270e8def1090682d4878bff96d47e2238"
LIBJPEG_TURBO_SHA = ""
APR_COMMIT = "901ece0cd7cec29c050c58451a801bb125d09b6e"  # July 24th, 2020
APR_SHA = "372b6a3424d8a3abbbf216bf6058e949f7b9da95e9caa57a9f5e82fe7528ca40"
APRUTIL_COMMIT = "13ed779e56669007dffe9a27ffab3790b59cbfaa"
APRUTIL_SHA = "9cf6d0e6fcc4783228dcee722897dadaadc601aef894c43a1e1514436eb4471a"
SERF_COMMIT = "3a37fc11c49d4fa91c559ee0b387f7a23705d999"  # July 24th, 2020
SERF_SHA = "0599b9a8ec8ea3ae260337fa84d8d335bd95ce54a236f7be24a8bddfd04a4840"

# NOTE: Closure isn't at the latest, because of that introcucing top level comments which
# break tests, but more importantly, make generated js files appear as if they're apache
# licenced. We're at the last revision before that change gets introduced.
# Full context at:
# https://github.com/google/closure-compiler/issues/3551, which got introcuded via
# https://github.com/google/closure-library/commit/1fe1bd873b1b772cca7de983cbaf72ef4011de0b
CLOSURE_LIBRARY_COMMIT = "20191111"  # July 27th, 2020 (latest release was 20200719)

def mod_pagespeed_dependencies():
    http_archive(
        name = "envoy",
        strip_prefix = "envoy-%s" % ENVOY_COMMIT,
        url = "https://github.com/envoyproxy/envoy/archive/%s.tar.gz" % ENVOY_COMMIT,
        sha256 = ENVOY_SHA,
    )

    http_archive(
        name = "brotli",
        strip_prefix = "brotli-%s" % BROTLI_COMMIT,
        url = "https://github.com/google/brotli/archive/%s.tar.gz" % BROTLI_COMMIT,
        sha256 = BROTLI_SHA,
        patch_args = ["-p1"],
        patches = ["brotli.patch"],
    )

    http_archive(
        name = "hiredis",
        strip_prefix = "hiredis-%s" % HIREDIS_COMMIT,
        url = "https://github.com/redis/hiredis/archive/v%s.tar.gz" % HIREDIS_COMMIT,
        build_file_content = hiredis_build_rule,
        sha256 = HIREDIS_SHA,
    )

    http_archive(
        name = "jsoncpp",
        strip_prefix = "jsoncpp-%s" % JSONCPP_COMMIT,
        url = "https://github.com/open-source-parsers/jsoncpp/archive/%s.tar.gz" % JSONCPP_COMMIT,
        build_file_content = jsoncpp_build_rule,
        sha256 = JSONCPP_SHA,
    )

    http_archive(
        name = "libpng",
        strip_prefix = "libpng-%s" % LIBPNG_COMMIT,
        url = "https://github.com/glennrp/libpng/archive/v%s.tar.gz" % LIBPNG_COMMIT,
        build_file_content = libpng_build_rule,
        sha256 = LIBPNG_SHA,
    )

    http_archive(
        name = "libwebp",
        urls = [
            "https://github.com/webmproject/libwebp/archive/v%s.tar.gz" % LIBWEBP_COMMIT,
        ],
        sha256 = LIBWEBP_SHA,
        strip_prefix = "libwebp-%s" % LIBWEBP_COMMIT,
        build_file_content = libwebp_build_rule,
    )

    http_archive(
        name = "google_sparsehash",
        strip_prefix = "sparsehash-%s" % GOOGLE_SPARSEHASH_COMMIT,
        url = "https://github.com/sparsehash/sparsehash/archive/%s.tar.gz" % GOOGLE_SPARSEHASH_COMMIT,
        build_file_content = google_sparsehash_build_rule,
        sha256 = GOOGLE_SPARSEHASH_SHA,
    )

    http_archive(
        name = "glog",
        strip_prefix = "glog-%s" % GLOG_COMMIT,
        url = "https://github.com/google/glog/archive/%s.tar.gz" % GLOG_COMMIT,
        sha256 = GLOG_SHA,
    )

    http_archive(
        name = "com_github_gflags_gflags",
        strip_prefix = "gflags-%s" % GFLAGS_COMMIT,
        url = "https://github.com/gflags/gflags/archive/%s.tar.gz" % GFLAGS_COMMIT,
        sha256 = GFLAGS_SHA,
    )

    http_archive(
        name = "drp",
        url = "https://github.com/apache/incubator-pagespeed-drp/archive/%s.tar.gz" % DRP_COMMIT,
        build_file_content = drp_build_rule,
        strip_prefix = "incubator-pagespeed-drp-%s" % DRP_COMMIT,
        sha256 = DRP_SHA,
    )

    http_archive(
        name = "giflib",
        strip_prefix = "giflib-%s" % GIFLIB_COMMIT,
        url = "https://downloads.sourceforge.net/project/giflib/giflib-%s.tar.gz" % GIFLIB_COMMIT,
        build_file_content = giflib_build_rule,
        sha256 = GIFLIB_SHA,
    )

    http_archive(
        name = "optipng",
        strip_prefix = "optipng-%s" % OPTIPNG_COMMIT,
        url = "https://prdownloads.sourceforge.net/optipng/optipng-%s.tar.gz?download" % OPTIPNG_COMMIT,
        build_file_content = optipng_build_rule,
        sha256 = OPTIPNG_SHA,
    )

    http_archive(
        name = "libjpeg_turbo",
        url = "https://chromium.googlesource.com/chromium/deps/libjpeg_turbo/+archive/%s.tar.gz" % LIBJPEG_TURBO_COMMIT,
        build_file_content = libjpeg_turbo_build_rule,
        # TODO(oschaaf): somehow sha's diverage between here and Travis.
        sha256 = LIBJPEG_TURBO_SHA,
    )

    http_archive(
        name = "apr",
        strip_prefix = "apr-%s" % APR_COMMIT,
        url = "https://github.com/apache/apr/archive/%s.tar.gz" % APR_COMMIT,
        build_file_content = apr_build_rule,
        patches = ["apr.patch"],
        patch_args = ["-p1"],
        sha256 = APR_SHA,
    )

    http_archive(
        name = "aprutil",
        strip_prefix = "apr-util-%s" % APRUTIL_COMMIT,
        url = "https://github.com/apache/apr-util/archive/%s.tar.gz" % APRUTIL_COMMIT,
        build_file_content = aprutil_build_rule,
        sha256 = APRUTIL_SHA,
    )

    http_archive(
        name = "serf",
        strip_prefix = "serf-%s" % SERF_COMMIT,
        url = "https://github.com/apache/serf/archive/%s.tar.gz" % SERF_COMMIT,
        build_file_content = serf_build_rule,
        sha256 = SERF_SHA,
    )

    http_archive(
        name = "closure_library",
        strip_prefix = "closure-library-%s" % CLOSURE_LIBRARY_COMMIT,
        url = "https://github.com/google/closure-library/archive/v%s.tar.gz" % CLOSURE_LIBRARY_COMMIT,
        sha256 = "21400f56c5b8f9e2548facb30658e4a09fe1cbaba39440735441735d2f900e55",
        build_file_content = closure_library_rules,
    )
