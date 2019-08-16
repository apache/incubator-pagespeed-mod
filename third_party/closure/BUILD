licenses(["notice"])  # Apache 2

genrule(
  name = 'compiler_script',
  tools = ["download.sh"],
  outs  = ["compiler"],
  cmd  = ('./$(location download.sh) $@'),
  visibility = ["//visibility:public"],
)