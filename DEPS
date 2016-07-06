# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

vars = {
  # chromium.org and googlecode.com will redirect https URLs for directories
  # that don't end with a trailing slash to http. We therefore try to make sure
  # all https URLs include the trailing slash, but it's unclear if SVN actually
  # respects this.
  "chromium_trunk": "https://src.chromium.org/svn/trunk/",
  "chromium_git": "https://chromium.googlesource.com",
  # We don't include @ inside the revision here as is customary since
  # we want to pass this into a -D flag
  "chromium_revision_num": "256281",
  "chromium_deps_root": "src/third_party/chromium_deps",

  "libpagespeed_svn_root": "https://github.com/pagespeed/page-speed/trunk/",
  "libpagespeed_trunk": "https://github.com/pagespeed/page-speed/trunk/lib/trunk/",
  "libpagespeed_revision": "@2579",

  # Import libwebp 0.4.1 from the official repo.
  "libwebp_src": "https://chromium.googlesource.com/webm/libwebp.git",
  "libwebp_revision": "@8af2771813632e2007988c8df6ad7e68b28ad121",

  "modspdy_src": "https://svn.apache.org/repos/asf/httpd/mod_spdy/trunk",
  "modspdy_revision": "@1661925",

  "serf_src": "https://svn.apache.org/repos/asf/serf/tags/1.3.8/",
  "serf_revision": "@head",

  "apr_src": "https://svn.apache.org/repos/asf/apr/apr/tags/1.5.1/",
  "apr_revision": "@head",

  "aprutil_src": "https://svn.apache.org/repos/asf/apr/apr-util/tags/1.5.4/",
  "aprutil_revision": "@head",

  "apache_httpd_src":
    "https://svn.apache.org/repos/asf/httpd/httpd/tags/2.2.29/",
  "apache_httpd_revision": "@head",

  "apache_httpd24_src":
    "https://svn.apache.org/repos/asf/httpd/httpd/tags/2.4.10/",
  "apache_httpd24_revision": "@head",

  # Release v20160208.
  # If the closure library version is updated, the closure compiler version
  # listed in third_party/closure/download.sh should be updated as well.
  "closure_library": "https://github.com/google/closure-library.git",
  "closure_library_revision": "@f3fc47996f3576bd2f7f578c9ffc67b50a928d71",

  "jsoncpp_src": "https://github.com/open-source-parsers/jsoncpp.git",
  "jsoncpp_revision": "@7165f6ac4c482e68475c9e1dac086f9e12fff0d0",

  "gflags_src_revision": "@e7390f9185c75f8d902c05ed7d20bb94eb914d0c",
  "gflags_revision": "@cc7e9a4b374ff7b6a1cae4d76161113ea985b624",

  "google_sparsehash_root":
    "https://github.com/google/sparsehash.git",
  "google_sparsehash_revision": "@6ff8809259d2408cb48ae4fa694e80b15b151af3",

  "gtest_src": "https://github.com/google/googletest.git",
  "gtest_revision": "@c99458533a9b4c743ed51537e25989ea55944908",

  "gmock_src": "https://github.com/google/googlemock.git",
  "gmock_revision": "@c440c8fafc6f60301197720617ce64028e09c79d",

  # Comment this out to disable HTTPS fetching via serf.  See also the
  # references in src/third_party/serf/serf.gyp.
  #
  # TODO(jmarantz): create an easy way to choose this option from the
  # 'gclient' command, without having to edit the gyp & DEPS files.
  "boringssl_src": "https://boringssl.googlesource.com/boringssl.git",
  "boringssl_git_revision": "@chromium-stable",

  "domain_registry_provider_src":
     "https://github.com/pagespeed/domain-registry-provider.git",
  "domain_registry_provider_revision":
     "@e9b72eaef413335eb054a5982277cb2e42eaead7",

  "libpng_src": "https://github.com/glennrp/libpng.git",
  "libpng_revision": "@a36c4f3f165fb2dd1772603da7f996eb40326621",

  # Brotli v0.5.0-snapshot
  "brotli_src": "https://github.com/google/brotli.git",
  "brotli_revision": "@e4c891420c004581cba43d918e73f446affdbbbb",

  "proto_src": "https://github.com/google/protobuf.git",
  "protobuf_revision": "v3.0.0-beta-2",

  ## grpc uses nanopb as a git submodule, which gclient doesn't support.
  ## When updating grpc, you should check the nanopb submodule version
  ## specified by your branch.
  "grpc_src": "https://github.com/grpc/grpc.git",
  "grpc_revision": "release-0_15_0",
  "nanopb_src": "https://github.com/nanopb/nanopb.git",
  "nanopb_revision": "f8ac463766281625ad710900479130c7fcb4d63b",
}

deps = {

  # Fetch dependent DEPS so we can sync our other dependencies relative
  # to them.
  Var("chromium_deps_root"):
    File(Var("chromium_trunk") + "src/DEPS@" + Var("chromium_revision_num")),

  # Other dependencies.
  "src/build/temp_gyp":
    Var("libpagespeed_trunk") + "src/build/temp_gyp/" +
        Var("libpagespeed_revision"),

  # We check 'build/android' out of Chromium repo to get
  # 'android/cpufeatures.gypi', which is needed to compile libwebp.
  "src/build/android":
    Var("chromium_trunk") + "src/build/android/@" +
        Var("chromium_revision_num"),
  "src/build/ios":
    Var("chromium_trunk") + "src/build/ios/@" + Var("chromium_revision_num"),
  "src/build/internal":
    Var("chromium_trunk") + "src/build/internal/@" +
        Var("chromium_revision_num"),
  "src/build/linux":
    Var("chromium_trunk") + "src/build/linux/@" + Var("chromium_revision_num"),
  "src/build/mac":
    Var("chromium_trunk") + "src/build/mac/@" + Var("chromium_revision_num"),
  "src/build/win":
    Var("chromium_trunk") + "src/build/win/@" + Var("chromium_revision_num"),

  # TODO(huibao): Remove references to libpagespeed.
  "src/third_party/giflib":
    Var("libpagespeed_svn_root") + "deps/giflib-4.1.6/",
  "src/third_party/icu": Var("libpagespeed_svn_root") + "deps/icu461/",
  "src/third_party/optipng":
    Var("libpagespeed_svn_root") + "deps/optipng-0.7.4/",
  "src/third_party/zlib": Var("libpagespeed_svn_root") + "deps/zlib-1.2.5/",

  # Yasm assember is required for libjpeg_turbo.
  "src/third_party/libjpeg_turbo/yasm":
    Var("chromium_trunk") + "src/third_party/yasm/@" +
        Var("chromium_revision_num"),

  "src/third_party/libjpeg_turbo/yasm/source/patched-yasm":
    Var("chromium_trunk") + "deps/third_party/yasm/patched-yasm/@" +
        Var("chromium_revision_num"),

  "src/third_party/libjpeg_turbo/src":
    Var("chromium_trunk") + "deps/third_party/libjpeg_turbo/@" +
        Var("chromium_revision_num"),

  "src/testing":
    Var("chromium_trunk") + "src/testing/@" + Var("chromium_revision_num"),
  "src/testing/gtest": Var("gtest_src") + Var("gtest_revision"),
  "src/testing/gmock": Var("gmock_src") + Var("gmock_revision"),


  "src/third_party/apr/src":
    Var("apr_src") + Var("apr_revision"),

  "src/third_party/aprutil/src":
    Var("aprutil_src") + Var("aprutil_revision"),

  "src/third_party/httpd/src/include":
    Var("apache_httpd_src") + "include/" + Var("apache_httpd_revision"),

  "src/third_party/httpd/src/os":
    Var("apache_httpd_src") + "os/" + Var("apache_httpd_revision"),

  "src/third_party/httpd24/src/include":
    Var("apache_httpd24_src") + "include/" + Var("apache_httpd24_revision"),

  "src/third_party/httpd24/src/os":
    Var("apache_httpd24_src") + "os/" + Var("apache_httpd24_revision"),

  "src/third_party/chromium/src/base":
    Var("chromium_trunk") + "src/base/@" + Var("chromium_revision_num"),

  "src/third_party/chromium/src/build":
    Var("chromium_trunk") + "src/build/@" + Var("chromium_revision_num"),

  "src/third_party/chromium/src/net/base":
     Var("chromium_trunk") + "src/net/base@" + Var("chromium_revision_num"),

  "src/third_party/chromium/src/url":
    Var("chromium_trunk") + "src/url@" + Var("chromium_revision_num"),

  "src/third_party/closure_library":
    Var("closure_library") + Var("closure_library_revision"),

  "src/third_party/gflags":
    Var('chromium_git') + '/external/webrtc/trunk/third_party/gflags' +
    Var('gflags_revision'),
  "src/third_party/gflags/src":
    Var('chromium_git') + '/external/gflags/src' + Var("gflags_src_revision"),

  "src/third_party/google-sparsehash":
    Var("google_sparsehash_root") + Var("google_sparsehash_revision"),

  "src/third_party/protobuf/src":
    Var("proto_src") + '@' + Var("protobuf_revision"),

  # Json cpp.
  "src/third_party/jsoncpp/src":
    Var("jsoncpp_src") + Var("jsoncpp_revision"),

  # Serf
  "src/third_party/serf/src": Var("serf_src") + Var("serf_revision"),

  "src/third_party/mod_spdy/src": Var("modspdy_src") + Var("modspdy_revision"),

  "src/third_party/libwebp": Var("libwebp_src") + Var("libwebp_revision"),

  "src/tools/clang":
    Var("chromium_trunk") + "src/tools/clang/@" + Var("chromium_revision_num"),

  # This is the same commit as the version from svn included from chromium_deps,
  # but the svn is down, so we take it from chromium-git.
  "src/tools/gyp":
    Var("chromium_git") + "/external/gyp@" + "0fb31294ae844bbf83eba05876b7a241b66f1e99",

  "src/third_party/modp_b64":
    Var("chromium_trunk") + "src/third_party/modp_b64/@" +
        Var("chromium_revision_num"),

  # RE2.
  "src/third_party/re2/src":
    Var("chromium_trunk") + "src/third_party/re2/@" +
        Var("chromium_revision_num"),

  # Comment to disable HTTPS fetching via serf.  See also the
  # references in src/third_party/serf/serf.gyp.
  "src/third_party/boringssl/src":
    Var("boringssl_src") + Var("boringssl_git_revision"),

  # Domain Registry Provider gives us the Public Suffix List.
  "src/third_party/domain_registry_provider":
    Var("domain_registry_provider_src") +
        Var("domain_registry_provider_revision"),

  "src/third_party/libpng/src": Var("libpng_src") + Var("libpng_revision"),

  "src/third_party/brotli/src": Var("brotli_src") + Var("brotli_revision"),

  "src/third_party/grpc/src": Var("grpc_src") + '@' + Var("grpc_revision"),

  "src/third_party/grpc/src/third_party/nanopb":
    Var("nanopb_src") + '@' + Var("nanopb_revision"),
}


deps_os = {
  "win": {
    "src/third_party/cygwin": From(Var("chromium_deps_root")),
    "src/third_party/python_26":
      Var("chromium_trunk") + "tools/third_party/python_26/@" +
          Var("chromium_revision_num"),
  },
  "mac": {
  },
  "unix": {
  },
}


include_rules = [
  # Everybody can use some things.
  "+base",
  "+build",
]


# checkdeps.py shouldn't check include paths for files in these dirs:
skip_child_includes = [
   "testing",
]


hooks = [
  {
    # Generate a gyp file for grpc.
    # Must happen before we actually run gyp below.
    'pattern': '.',
    'action': ['src/third_party/grpc/generate_grpc_gyp',
               'src/third_party/grpc/src',
               'src/third_party/grpc/grpc.gyp'],
  },
  {
    # Pull clang on mac. If nothing changed, or on non-mac platforms, this takes
    # zero seconds to run. If something changed, it downloads a prebuilt clang,
    # which takes ~20s, but clang speeds up builds by more than 20s.
    "pattern": ".",
    "action": ["python", "src/tools/clang/scripts/update.py", "--mac-only"],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "src/build/gyp_chromium",
               "-Dchromium_revision=" + Var("chromium_revision_num")],
  },
  {
    "pattern": ".",
    "action": ["src/third_party/closure/download.sh"],
  },
]
