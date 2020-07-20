jsoncpp_build_rule = """
cc_library(
    name = "jsoncpp",
    srcs = [
        "src/lib_json/json_internalarray.inl",
        "src/lib_json/json_internalmap.inl",
        "src/lib_json/json_reader.cpp",
        "src/lib_json/json_value.cpp",
        "src/lib_json/json_valueiterator.inl",
        "src/lib_json/json_writer.cpp",
    ],
    hdrs = [
        "src/lib_json/json_batchallocator.h",
        "src/lib_json/json_tool.h",
        "include/json/assertions.h",
        "include/json/config.h",
        "include/json/value.h",
        "include/json/writer.h",
        "include/json/reader.h",
        "include/json/forwards.h",
        "include/json/features.h",
        "include/json/json.h",
        "include/json/autolink.h",
    ],
    includes = ["include"],
    visibility = ["//visibility:public"],
)
"""