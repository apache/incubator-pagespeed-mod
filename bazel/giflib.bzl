giflib_build_rule = """
cc_library(
    name = "giflib_core",
    srcs = [
        'gifalloc.c',
        'gif_err.c',
        'openbsd-reallocarray.c'
    ],
    hdrs = [
        'gif_lib.h',
        'gif_lib_private.h',
        'gif_hash.h',
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "dgiflib",
    srcs = [
        'dgif_lib.c',
    ],
    defines = [
        'UINT32=\\"unsigned int\\"',
        '_GBA_NO_FILEIO',
    ],
    copts = [
         # TODO(Sam): need to check xcode_settings for 'Wno-pointer-sign'
        '-Wno-pointer-sign',
    ],
    deps = ['@giflib//:giflib_core',],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "egiflib",
    srcs = [
        'egif_lib.c',
        'gif_hash.c'
    ],
    defines = [
        'UINT32=\\"unsigned int\\"',
        '_GBA_NO_FILEIO',
        'HAVE_FCNTL_H',
    ],
    copts = [
         # TODO(Sam): need to check xcode_settings for 'Wno-pointer-sign'
        '-Wno-pointer-sign',
    ],
    deps = ['@giflib//:giflib_core',],
    visibility = ["//visibility:public"],
)
"""

