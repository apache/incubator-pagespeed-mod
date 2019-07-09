hiredis_build_rule = """
cc_library(
    name = "hiredis",
    srcs = [
        "async.c",
        "hiredis.c",
        "net.c",
        "read.c",
        "sds.c",
    ],
    hdrs = [
        "dict.c",
        "async.h",
        "dict.h",
        "hiredis.h",
        "net.h",
        "read.h",
        "sds.h",
        "fmacros.h",
    ],
    visibility = ["//visibility:public"],
)
"""