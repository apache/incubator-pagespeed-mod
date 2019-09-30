libpng_build_rule = """

# TODO(oschaaf): we need to revisit this for linking against the system library
# later on.

genrule(
  name = "copy_prebuild_header",
  srcs = ["scripts/pnglibconf.h.prebuilt"],
  outs = ["pnglibconf.h"],
  cmd = "cp $< $@",
)

cc_library(
    name = "libpng",
    srcs = [
            "png.c",
            "pngerror.c",
            "pngget.c",
            "pngmem.c",
            "pngpread.c",
            "pngread.c",
            "pngrio.c",
            "pngrtran.c",
            "pngrutil.c",
            "pngset.c",
            "pngtrans.c",
            "pngwio.c",
            "pngwrite.c",
            "pngwtran.c",
            "pngwutil.c",
    ],
    hdrs = [
            "pngpriv.h",
            "pnginfo.h",
            "png.h",
            "pngconf.h",
            "pngdebug.h",
            "pngstruct.h",
            ":copy_prebuild_header",
    ],
    deps = ["@envoy//bazel/foreign_cc:zlib"],
    defines = [              
              # We end up including setjmp.h directly, but libpng
              # doesn't like that. This define tells libpng to not
              # complain about our inclusion of setjmp.h.
              'PNG_SKIP_SETJMP_CHECK',

              # The PNG_FREE_ME_SUPPORTED define was dropped in libpng
              # 1.4.0beta78, with its behavior becoming the default
              # behavior.
              # Hence, we define it ourselves for version >= 1.4.0
              'PNG_FREE_ME_SUPPORTED',
            ],
    visibility = ["//visibility:public"],
)
"""