#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Sets common environment variables requires for building PageSpeed stuff.

compiler_path=/usr/lib/gcc-mozilla
if [ -x "${compiler_path}/bin/gcc" ]; then
  export PATH="${compiler_path}/bin:$PATH"
  export LD_LIBRARY_PATH="${compiler_path}/lib:${LD_LIBRARY_PATH:-}"
fi

export PATH=$HOME/bin:/usr/local/bin:$PATH
