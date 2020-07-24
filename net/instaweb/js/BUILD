cc_library(
    name = "data2c_lib",
    srcs = [
        "data_to_c.cc",
    ],
    hdrs = [
        "public/js_minify.h",
    ],
    deps = [
        "//pagespeed/kernel/base:pagespeed_base",
        "//pagespeed/kernel/util:util", # XXx just gflags
    ],
)

cc_binary(
    name = "data2c",
    deps = [
        ":data2c_lib",
    ],
    visibility = ["//visibility:public"],
)