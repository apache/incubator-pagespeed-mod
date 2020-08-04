load("@rules_cc//cc:defs.bzl", "cc_binary")

licenses(["notice"])  # Apache 2

cc_binary(
    name = "mod_pagespeed",
    deps = [
        "//net/instaweb:net_instaweb_lib",
        "//net/instaweb/rewriter:html_minifier_main_lib",
    ],
)

cc_binary(
    name = "libmod_pagespeed.so",
    linkshared = 1,
    linkstatic = 0,
    visibility = ["//visibility:public"],
    deps = ["//pagespeed/apache"],
)
