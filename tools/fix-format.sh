#!/bin/bash

find -name '*.cc' -o -name '*.h' | xargs clang-format-10 -i -style=file