closure_library_rules = """
sh_library(
    name = "runfiles",
    srcs = [],
    visibility = ["//visibility:public"],
)
"""

def closure_compiler_gen(name, js_src, js_includes = [], js_dir = [], entry_points = [], externs = [], opt = True):
    js_include_str = ""
    for str in js_includes:
        js_include_str += " --js $$(find ../../../../../execroot/mod_pagespeed/" + str + " )"

    js_entry_points = ""
    for str in entry_points:
        js_entry_points += " --entry_point " + str

    js_externs = ""
    for str in externs:
        js_externs += " --externs $$(find ../../../../../execroot/mod_pagespeed/" + str + ")"

    if opt == True:
        BUILD_FLAGS = " --compilation_level=ADVANCED"
    else:
        BUILD_FLAGS = "  --compilation_level=SIMPLE --formatting=PRETTY_PRINT "

    native.genrule(
        name = name,
        srcs = ["@closure_library//:runfiles"],
        outs = [name + ".js"],
        cmd = ("java -jar $(location //third_party/closure:compiler_script)/compiler.jar" +
               " --js $$(find ../../../../../execroot/mod_pagespeed/" + js_src + " )" +
               " --js_output_file $@" +
               js_include_str +
               BUILD_FLAGS +
               js_entry_points +
               js_externs +
                " --dependency_mode STRICT" +
               #" --warning_level VERBOSE" +
               " --jscomp_off=checkVars" +
               " --generate_exports" +
               " --output_wrapper=\"(function(){%output%})();\"" +
               " $$(find ../../../../../external/closure_library/closure ../../../../../external/closure_library/third_party -type f -name \"*.js\"| grep -v _test.js | sort | sed \"s/^/--js /\")"),
        tools = [
            "//third_party/closure:compiler_script",
        ],
    )

def closure_compiler_without_dependency_mode(name, js_src, js_includes = [], js_dir = [], externs = [], opt = True):
    for js_file in js_src:
        js_include_str = ""
        for str in js_includes:
            js_include_str += " --js $$(find ../../../../../execroot/mod_pagespeed/" + str + " )"

        js_externs = ""
        for str in externs:
            js_externs += " --externs $$(find ../../../../../execroot/mod_pagespeed/" + str + ")"

        if opt == True:
            BUILD_FLAGS = " --compilation_level=ADVANCED "
            name = js_file.split("/")[len(js_file.split("/")) - 1].split(".js")[0] + "_opt"
        else:
            BUILD_FLAGS = "  --compilation_level=SIMPLE --formatting=PRETTY_PRINT "
            name = js_file.split("/")[len(js_file.split("/")) - 1].split(".js")[0] + "_dbg"

        native.genrule(
            name = name,
            srcs = ["@closure_library//:runfiles"],
            outs = [name + ".js"],
            cmd = ("java -jar $(location //third_party/closure:compiler_script)/compiler.jar" +
                   " --js $$(find ../../../../../execroot/mod_pagespeed/" + js_file + " )" +
                   " --js_output_file $@" +
                   js_include_str +
                   BUILD_FLAGS +
                   js_externs +
                   #" --warning_level VERBOSE" +
                   " --jscomp_off=checkVars" +
                   " --generate_exports" +
                   " --output_wrapper=\"(function(){%output%})();\""),
            tools = [
                "//third_party/closure:compiler_script",
            ],
        )
