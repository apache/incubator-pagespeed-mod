#!/bin/bash

ADIR=$(bazel info bazel-bin)
ALIST=$(find $ADIR | grep \\.a$ | grep -v main | grep -v copy | xargs echo)

find $ADIR | grep \\.a$ | grep -v main | grep -v copy 

set -e
echo "merging libs"
./merge_libraries.sh ~/pagespeed_automatic.a.dirty $ALIST > merge.log

echo "renaming symbols"

# XXX(oschaaf): objcopy 2.31 doesn't like clang-7's output unless we pass in -fno-addrsig
# https://github.com/travitch/whole-program-llvm/issues/75
# not passing this in will make the scripts that rename symbols fail
./rename_c_symbols.sh ~/pagespeed_automatic.a.dirty ~/pagespeed_automatic.a > symbol-rename.log
rm ~/pagespeed_automatic.a.dirty
mv ~/pagespeed_automatic.a ~/incubator-pagespeed-ngx-latest-stable/psol/lib/Release/linux/x64
echo "done"
