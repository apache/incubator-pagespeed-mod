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
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
        "-Wno-error=unused-parameter",
        "-Wno-error=unused-private-field",
        "-Wno-error=missing-field-initializers",
    ],
    deps = [
        ":image_optimizer_proto_cc",
        "@libpng//:libpng",
        "//pagespeed/kernel/base:pagespeed_base",
    ],
)

envoy_cc_test_library(
    name = "image_test_base",
    repository = "@envoy",
    srcs = [

    ],
    hdrs = [
        "test_utils.h",
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
    data = glob(["testdata/*"]),
    copts = ["-Wno-error=unused-parameter","-Wno-error=unused-private-field","-Wno-error=unused-function"],
    deps = [
        ":image_test_base",
    ],
)