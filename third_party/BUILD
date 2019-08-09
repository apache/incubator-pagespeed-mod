licenses(["notice"])  # Apache 2

cc_library(
    name = "all_third_party",
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/base64:base64",
        "//third_party/rdestl:rdestl",
        #"@brotli//:brotli_common",
        #"@brotli//:brotli_enc",
        #"@brotli//:brotli_dec",
        "@zlib//:zlib",
        "@hiredis//:hiredis",
        "@jsoncpp//:jsoncpp",
        "@re2//:re2",
        "@icu//:icu",
        "@libpng//:libpng",
        "@libwebp//:libwebp",
        "@google_sparsehash//:google_sparsehash",
        #"@protobuf//:protobuf_lite",
        #"@protobuf//:protobuf_full_do_not_use",
        #"@protobuf//:protobuf_message_differencer",
        "@giflib//:giflib_core",
        "@giflib//:dgiflib",
        "@giflib//:egiflib",
        "@optipng//:opngreduc",
        "@grpc//:grpc++",        # grpc++ target corresponds to grpc_cpp target of gyp
        "@grpc//:grpc",          # grpc target corresponds to grpc_core target of gyp
        "@grpc//:gpr",
        "@grpc//:grpc_cpp_plugin",
    ],
)
