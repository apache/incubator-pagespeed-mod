#!/bin/bash
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
# Scans source directories for _test.cc files to find ones that aren't mentioned
# in the test gyp file.  On success produces no output, otherwise prints the
# names of the unreferenced _test.cc files.

set -u
set -e

this_dir="$(dirname "${BASH_SOURCE[0]}")"
cd "$this_dir/.."

test_gyp="net/instaweb/test.gyp"
find net pagespeed -name *_test.cc | while read test_path; do
  if ! grep -q "$(basename "$test_path")" "$test_gyp"; then
    echo "$test_path ($(basename "$test_path")) missing from $test_gyp"
  fi
done
