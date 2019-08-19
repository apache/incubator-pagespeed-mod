def data2c_gen2(name, srcs):
    for f in srcs:
        native.genrule(
            name = name + "_" + f,
            srcs = [f + "_dbg.js"],
            outs = [f + ".cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=JS_" + f,
            tools = ["//net/instaweb/js:data2c"],
        )

# XXX(oschaaf): actually minify these.
def data2c_gen2_opt(name, srcs):
    for f in srcs:
        native.genrule(
            name = name + "_" + f + "_opt",
            srcs = [f + "_opt.js"],
            outs = [f + "_opt.cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=JS_" + f + "_opt",
            tools = ["//net/instaweb/js:data2c"],
        )


def data2c_gen2_admin_js(name, srcs, opt):
    for f in srcs:
        native.genrule(
            name = "js_" + name + "_" + f + ("_opt" if opt else ""),
            srcs = [f + ("_opt" if opt else "_dbg") + ".js"],
            outs = ["js_" + f + ("_opt" if opt else "") + ".cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=JS_" + f + "_js" + ("_opt" if opt else ""),
            tools = ["//net/instaweb/js:data2c"],
        )

def data2c_gen2_admin_css(name, srcs, opt):
    for f in srcs:
        native.genrule(
            name = "css_" + name + "_" + f + ("_opt" if opt else ""),
            srcs = [f + ".css"],
            outs = ["css_" + f + ("_opt" if opt else "") + ".cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=CSS_" + f + "_css" + ("_opt" if opt else ""),
            tools = ["//net/instaweb/js:data2c"],
        )
