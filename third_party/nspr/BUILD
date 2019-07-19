licenses(["notice"])  # Apache 2

# XXX(oschaaf): nspr was coped over and modified a bit to work with our project
# from https://chromium.googlesource.com/chromium/src/+/master/base/third_party/nspr/

cc_library(
    name = "prtime",
    srcs = [
        'prtime.cc',
    ],
    hdrs = [
        "prtime.h",
    ],
    visibility = ["//visibility:public"],
    deps = ["//gurl:url"]
)

