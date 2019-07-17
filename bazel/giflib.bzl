giflib_build_rule = """
cc_library(
    name = "giflib_core",
    srcs = [
        'lib/gifalloc.c',
        'lib/gif_err.c',
        'lib/openbsd-reallocarray.c'
    ],
    hdrs = [
        'lib/gif_lib.h',
        'lib/gif_lib_private.h',
        'lib/gif_hash.h',
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "dgiflib",
    srcs = [
        'lib/dgif_lib.c',
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
        'lib/egif_lib.c',
        'lib/gif_hash.c'
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

