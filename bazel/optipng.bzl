optipng_build_rule = """
cc_library(
    name = "opngreduc",
    srcs = [
        "src/opngreduc/opngreduc.c",
    ],
    hdrs = [
        "src/opngreduc/opngreduc.h"
    ],
    deps = ["@libpng//:libpng",],
    visibility = ["//visibility:public"],
)
"""