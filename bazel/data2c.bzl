def data2c_gen2(name, srcs):
    for f in srcs:
        native.genrule(
            name = name + "_" + f,
            srcs = [f + ".js"],
            outs = [f + ".cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=JS_" + f,
            tools = ["//net/instaweb/js:data2c"],
        )

# XXX(oschaaf): actually minify these.
def data2c_gen2_opt(name, srcs):
    for f in srcs:
        native.genrule(
            name = name + "_" + f + "_opt",
            srcs = [f + ".js"],
            outs = [f + "_opt.cc"],
            cmd = "$(location //net/instaweb/js:data2c) --data_file=$< --c_file=$@ --varname=JS_" + f + "_opt",
            tools = ["//net/instaweb/js:data2c"],
        )
