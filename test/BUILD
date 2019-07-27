licenses(["notice"])  # Apache 2

cc_library(
    name = "main",
    srcs = [
        "main.cc",
    ],
    hdrs = [
        "test_runner.h",
    ],
    testonly = True,
    visibility = ["//visibility:public"],
    deps = [
        "//pagespeed/kernel/base:pagespeed_gtest"
    ]
#    external_deps = [
#        "abseil_symbolize",
#    ]
)