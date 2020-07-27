licenses(["notice"])  # Apache 2

load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

proto_library(
    name = "spriter_protos",
    srcs = [
        "public/image_spriter.proto",
    ],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "spriter_protos_cc",
    deps = [
        ":spriter_protos",
    ]
)

cc_library(
    name = "spriter",
    srcs = [
        "image_library_interface.cc",
        "image_spriter.cc",
        "libpng_image_library.cc",
    ],
    hdrs = [
        "image_library_interface.h",
        "public/image_spriter.h",
        "libpng_image_library.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@libpng//:libpng",
        "//pagespeed/kernel/base:pagespeed_base",
        ":spriter_protos_cc",
    ],
)

pagespeed_cc_test_library (
    name = "spriter_test_base",
    srcs = [
        "mock_image_library_interface.h",
    ],
    deps = [
        ":spriter",
    ]
)

pagespeed_cc_test(
    name = "spriter_test",
    srcs = [
        "image_spriter_test.cc",
        "libpng_image_library_test.cc",
    ],
    deps = [
        ":spriter_test_base",
    ],
    data = ["//net/instaweb/rewriter:testdata"],
)