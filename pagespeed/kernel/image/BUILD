load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")

licenses(["notice"])  # Apache 2

proto_library(
    name = "image_optimizer_proto",
    srcs = ["image_optimizer.proto"],
)

cc_proto_library(
    name = "image_optimizer_proto_cc",
    deps = [":image_optimizer_proto"],
)

cc_library(
    name = "image",
    srcs = [
        "frame_interface_optimizer.cc",
        "gif_reader.cc",
        "gif_square.cc",
        "image_analysis.cc",
        "image_converter.cc",
        "image_frame_interface.cc",
        "image_optimizer.cc",
        "image_resizer.cc",
        "image_util.cc",
        "jpeg_optimizer.cc",
        "jpeg_reader.cc",
        "jpeg_utils.cc",
        "pixel_format_optimizer.cc",
        "png_optimizer.cc",
        "read_image.cc",
        "scanline_interface_frame_adapter.cc",
        "scanline_utils.cc",
        "webp_optimizer.cc",
    ],
    hdrs = [
        "frame_interface_optimizer.h",
        "gif_reader.h",
        "gif_square.h",
        "image_analysis.h",
        "image_converter.h",
        "image_frame_interface.h",
        "image_optimizer.h",
        "image_resizer.h",
        "image_util.h",
        "jpeg_optimizer.h",
        "jpeg_reader.h",
        "jpeg_utils.h",
        "pixel_format_optimizer.h",
        "png_optimizer.h",
        "read_image.h",
        "scanline_interface.h",
        "scanline_interface_frame_adapter.h",
        "scanline_status.h",
        "scanline_utils.h",
        "webp_optimizer.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":image_optimizer_proto_cc",
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/http",
        "@giflib//:dgiflib",
        "@giflib//:egiflib",
        "@libjpeg_turbo//:libjpeg",
        "@libpng",
        "@libwebp",
        "@optipng//:opngreduc",
    ],
)
