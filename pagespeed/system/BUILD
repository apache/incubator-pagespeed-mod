load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_cc_test_library",
    "envoy_package",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_cc_library(
    name = "system",
    srcs = [
    ],
    hdrs = [
        "controller_process.h",
    ],
    copts = [
#        "-Wno-error=unused-parameter",
#        "-Wno-error=inconsistent-missing-override",
#        "-Wno-error=old-style-cast",  # rdestl
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        #        "//pagespeed/opt/http:http",
        #        "//pagespeed/kernel/http:http",
 #       ":controller_proto_grpc",
 #       "//pagespeed/kernel/base:pagespeed_base",
 #       "//pagespeed/kernel/thread",
 #       "//pagespeed/kernel/util",
 #       "//third_party/rdestl",
    ],
)

envoy_cc_test_library(
    name = "system_test_base",
    srcs = [
    ],
    hdrs = [
    ],
    copts = [
#        "-Wno-error=unused-parameter",
    ],
    data = [
    ],
    repository = "@envoy",
    deps = [
        ":system",
        #        "//pagespeed/kernel/base:kernel_test_util",
        #"//pagespeed/kernel/base:pagespeed_gtest",
        #"//pagespeed/kernel/util",
        #"//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/thread:thread_test_base",
        #        "//pagespeed/kernel/sharedmem:shared_mem_test_base",
    ],
)

envoy_cc_test(
    name = "system_test",
    srcs = [
    ],
    copts = [
        #"-Wno-error=unused-parameter",
        #        "-Wno-error=unused-private-field",
        #        "-Wno-error=unused-function",
        #"-Wno-error=inconsistent-missing-override",
        #"-Wno-error=old-style-cast",  # rdestl
        #        "-Wno-error=missing-field-initializers",
    ],
    repository = "@envoy",
    deps = [
        ":system_test_base",
#        ":grpc_test_proto_grpc",
    ],
)
