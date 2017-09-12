#!/bin/bash
#
# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Testing css minify for retained unit in calc function and with 0 value.

start_test CSS minify with calc function and value 0.

URL="$TEST_ROOT/css_minify_calc_function_value_zero.html?PageSpeedFilters=rewrite_css"
RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)

# checking for minified css with unit retained for 0 value 
MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c "width:calc(50ex - 0px)")
check [ $MATCHES -eq 1 ]
