#!/bin/bash
#
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
start_test Make sure Disallow/Allow overrides work in htaccess hierarchies
DISALLOWED=$($WGET_DUMP "$TEST_ROOT"/htaccess/purple.css)
check_from "$DISALLOWED" fgrep -q MediumPurple
fetch_until "$TEST_ROOT"/htaccess/override/purple.css \
    'fgrep -c background:#9370db' 1
