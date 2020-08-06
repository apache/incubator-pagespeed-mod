load(
    "//bazel:pagespeed_test.bzl",
    "pagespeed_cc_test",
    "pagespeed_cc_test_library",
)

licenses(["notice"])  # Apache 2

pagespeed_cc_test_library(
    name = "image_test_base",
    srcs = [
        "jpeg_optimizer_test_helper.cc",
        "test_utils.cc",
    ],
    hdrs = [
        "jpeg_optimizer_test_helper.h",
        "test_utils.h",
    ],
    deps = [
        "//pagespeed/kernel/image",
        "//test/pagespeed/kernel/base:kernel_test_util",
    ],
)

pagespeed_cc_test(
    name = "image",
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
    deps = [
        ":image_test_base",
    ],
)
