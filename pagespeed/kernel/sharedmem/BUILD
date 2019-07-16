licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

#envoy_cc_library(
#    name = "shared_mem",
#    srcs = [
#        "inprocess_shared_mem.cc",
#        "shared_mem_statistics.cc",
#    ],
#    hdrs = [
#        "inprocess_shared_mem.h",
#        "shared_mem_statistics.h",
#    ],
#    repository = "@envoy",
#    visibility = ["//visibility:public"],
#    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
#    deps = [
#        "@glog",
#    ],
#)

#envoy_cc_test_library(
#    name = "pagespeed_gtest",
#    srcs = ["gtest.cc"],
#    hdrs = ["gtest.h"],
#    repository = "@envoy",
#    deps = [
#        ":shared_mem",
#        "@com_google_googletest//:gtest"
#    ],
#)

#envoy_cc_test_library (
#    name = "shared_mem_test_base",
#    hdrs = [
#        "message_handler_test_base.h",
#        "file_system_test_base.h",
#        ] 
#        + glob(["mock_*.h"]),
#    srcs = 
#        [
#            "message_handler_test_base.cc",
#            "file_system_test_base.cc",
#        ] 
#        + glob(["mock_*.cc"]),
#    repository = "@envoy",
#    copts = ["-Wno-error=unused-parameter", "-Wno-unused-parameter"],
#    deps = [
#        ":shared_mem",
#    ]
#)

#envoy_cc_test(
    #name = "shared_mem_test",
    #srcs = glob(["*_test.cc"]),
    #repository = "@envoy",
    #copts = ["-Wno-error=unused-private-field", "-Wno-error=unused-parameter", "-Wno-unused-parameter"],
    #deps = [
    #    ":shared_mem",
    #    ":pagespeed_gtest",
    #    ":shared_mem_test_base"
    #],
#)
