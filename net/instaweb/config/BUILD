load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "config",
    srcs = [
        #"rewrite_options_manager.cc",
        #"measurement_proxy_rewrite_options_manager.cc",
    ],
    hdrs = [
        "rewrite_options_manager.h",
        "measurement_proxy_rewrite_options_manager.h",
    ],
    copts = [
#        "-Wno-error=unused-parameter",
#        "-Wno-error=old-style-cast",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-error=unused-private-field",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
 #       "//net/instaweb/rewriter:rewriter"
 #       "//pagespeed/kernel/cache",
 #       "//pagespeed/opt/logging",
    ],
)

envoy_cc_test_library(
    name = "config_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    copts = [
#        "-Wno-error=unused-parameter",
#        "-Wno-error=old-style-cast",
    ],
    repository = "@envoy",
    deps = [
        ":ads",
#        "//pagespeed/kernel/base:kernel_test_util",
#        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)


envoy_cc_test(
    name = "config_test",
#    srcs = glob(["*_test.cc"]),
    copts = [
 #       "-Wno-error=unused-parameter",
 #       "-Wno-error=old-style-cast",
 #       "-Wno-error=unused-const-variable",
    ],
    repository = "@envoy",
    deps = [
#        ":config_test_base",    
    ],
)
