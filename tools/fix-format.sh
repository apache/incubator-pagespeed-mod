#!/bin/bash

cd "$(bazel info workspace)" &&
find -name '*.cc' -o -name '*.h' | xargs clang-format-10 -i -style=file