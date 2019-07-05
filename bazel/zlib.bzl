zlib_build_rule = """
cc_library(
    name = "zlib",
    srcs = glob([
        "src/adler32.c",
        "src/compress.c",
        "src/crc32.c",
        "src/deflate.c",
        "src/infback.c",
        "src/inffast.c",
        "src/inflate.c",
        "src/inftrees.c",
        "src/trees.c",
        "src/zutil.c",
    ]),
    hdrs = glob([
        "src/crc32.h",
        "src/deflate.h",
        "src/inffast.h",
        "src/inffixed.h",
        "src/inflate.h",
        "src/inftrees.h",
        "src/trees.h",
        "src/zconf.h",
        "src/zlib.h",
        "src/zutil.h",
        "src/gzguts.h",
    ]),
    copts = [
        "-std=gnu99",
        "-Wno-implicit-function-declaration",
        "-Wno-error",
    ],
    visibility = ["//visibility:public"],
)
"""