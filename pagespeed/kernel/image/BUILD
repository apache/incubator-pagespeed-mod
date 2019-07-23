licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

envoy_package()

proto_library(
    name = "image_optimizer_proto",
    srcs = ["image_optimizer.proto"],
)

cc_proto_library(
    name = "image_optimizer_proto_cc",
    deps = [":image_optimizer_proto"],
)

envoy_cc_library(
    name = "image",
    srcs = [
        "image_analysis.cc",
        "image_resizer.cc",
        "image_optimizer.cc",
        "gif_reader.cc",
        "image_util.cc",
        "image_frame_interface.cc",
        "frame_interface_optimizer.cc",
        "jpeg_utils.cc",
        "read_image.cc",
        "jpeg_reader.cc",
        "pixel_format_optimizer.cc",
        "jpeg_optimizer.cc",
        "image_converter.cc",
        "scanline_utils.cc",
        "png_optimizer.cc",
        "scanline_interface_frame_adapter.cc",
        "webp_optimizer.cc",
        "gif_square.cc",
    ],
    hdrs = [
        "image_analysis.h",
        "image_resizer.h",
        "image_optimizer.h",
        "gif_reader.h",
        "image_util.h",
        "image_frame_interface.h",
        "scanline_status.h",
        "frame_interface_optimizer.h",
        "jpeg_utils.h",
        "read_image.h",
        "jpeg_reader.h",
        "pixel_format_optimizer.h",
        "jpeg_optimizer.h",
        "image_converter.h",
        "scanline_interface.h",
        "scanline_utils.h",
        "png_optimizer.h",
        "scanline_interface_frame_adapter.h",
        "webp_optimizer.h",
        "gif_square.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=missing-field-initializers",
        "-Wno-error=old-style-cast",
        "-Wno-error=sign-compare",
    ],
    deps = [
        ":image_optimizer_proto_cc",
        "//pagespeed/kernel/http:http",
        "@libpng//:libpng",
        "@optipng//:opngreduc",
        "@giflib//:dgiflib",
        "@giflib//:egiflib",
        "@libwebp//:libwebp",
        "@libjpeg_turbo//:libjpeg",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

envoy_cc_test_library(
    name = "image_test_base",
    repository = "@envoy",
    srcs = [
        "test_utils.cc",
        "jpeg_optimizer_test_helper.cc",
    ],
    hdrs = [
        "test_utils.h",
        "jpeg_optimizer_test_helper.h",
    ],
    data = [
        
    ],
    copts = ["-Wno-error=unused-parameter","-Wno-error=unused-private-field","-Wno-error=unused-function"],
    deps = [
        ":image",
        "//pagespeed/kernel/base:kernel_test_util",
        "//pagespeed/kernel/base:pagespeed_gtest",
    ],
)

envoy_cc_test(
    name = "image_test",
    repository = "@envoy",
    srcs = glob(["*_test.cc"]),
    data = glob([
        "testdata/gif/*",
        "testdata/jpeg/*",
        "testdata/png/*",
        "testdata/pngsuite/*",
        "testdata/pngsuite/gif/*",
        "testdata/resized/*",
        "testdata/webp/*",
    ]),
    copts = ["-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=unused-function",
        "-Wno-error=old-style-cast",
        "-Wno-error=missing-field-initializers",
    ],
    deps = [
        ":image_test_base",
    ],
)