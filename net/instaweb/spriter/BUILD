licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

proto_library(
    name = "spriter_protos",
    srcs = [
        "public/image_spriter.proto",
    ],
)

cc_proto_library(
    name = "spriter_protos_cc",
    deps = [
        ":spriter_protos",
    ]
)

envoy_cc_library(
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
    repository = "@envoy",
    visibility = ["//visibility:public"],
    copts = [
#        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
#        "//gurl:url",
        "@libpng//:libpng",
        "//pagespeed/kernel/base:pagespeed_base",
        #"//net/instaweb/http:http",
#        "//pagespeed/kernel/thread:thread",
#        "//third_party/rdestl:rdestl",
        ":spriter_protos_cc",
    ],
)

envoy_cc_test_library (
    name = "spriter_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    repository = "@envoy",
    copts = [
#        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
#        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":spriter",
#        "//pagespeed/kernel/base:kernel_test_util",
#        "//pagespeed/kernel/util:util_test_base",
#        "//pagespeed/kernel/base:benchmark",
    ]
)

envoy_cc_test(
    name = "spriter_test",
    repository = "@envoy",
    srcs = [
    ],
    copts = [
#        "-Wno-error=old-style-cast",
#        "-Wno-error=missing-field-initializers",
#        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-inconsistent-missing-override",
#        "-Wno-missing-field-initializers",
    ],
    deps = [
        ":spriter_test_base",
    ],
)