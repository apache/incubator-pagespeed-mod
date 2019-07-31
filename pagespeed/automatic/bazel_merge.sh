#!/bin/bash

ADIR=$(bazel info bazel-bin -c dbg)
ALIST=$(find $ADIR | grep \\.a$ | grep -v main | grep -v copy | xargs echo)
rm ~/test.a.dirty
rm ~/test.a

set -e
echo "merging libs"
./merge_libraries.sh ~/pagespeed_automatic.a.dirty $ALIST > merge.log

echo "renaming symbols"

# XXX(oschaaf): objcopy 2.31 doesn't like clang-7's output unless we pass in -fno-addrsig
# https://github.com/travitch/whole-program-llvm/issues/75
# not passing this in will make the scripts that rename symbols fail
./rename_c_symbols.sh ~/pagespeed_automatic.a.dirty ~/pagespeed_automatic.a > symbol-rename.log
rm ~/pagespeed_automatic.a.dirty
echo "done"
