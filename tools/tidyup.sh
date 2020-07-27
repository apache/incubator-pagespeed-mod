#!/bin/bash

cd "$(bazel info workspace)" &&
tools/gen_compilation_database.py && \
run-clang-tidy-10.py -header-filter='-external' \
  -checks='-*,cppcoreguidelines-pro-type-cstyle-cast,google-upgrade-googletest-case' \
  -fix pagespeed/ net/ && \
  fix-format.sh && \
echo "done"
