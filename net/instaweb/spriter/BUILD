load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

licenses(["notice"])  # Apache 2

proto_library(
    name = "spriter_protos",
    srcs = ["public/image_spriter.proto"],
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "spriter_protos_cc",
    deps = [":spriter_protos"],
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
        "libpng_image_library.h",
        "public/image_spriter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":spriter_protos_cc",
        "//pagespeed/kernel/base:pagespeed_base",
        "@libpng",
    ],
)
