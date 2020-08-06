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

# fetch html with pedantic filter enabled
start_test HTML check type attribute with pedantic
$WGET -O $WGET_OUTPUT $TEST_ROOT/type_attribute_pedantic.html\
?PageSpeedFilters=add_instrumentation,pedantic
# check for type attribute in html
check [ $(grep -c "<script type='text/javascript'>window.mod_pagespeed_start" $WGET_OUTPUT) = 1 ]
check [ $(grep -c "<script data-pagespeed-no-defer type=\"text/javascript\">" $WGET_OUTPUT) = 1 ]

# fetch html without pedantic filter
start_test HTML check type attribute
$WGET -O $WGET_OUTPUT $TEST_ROOT/type_attribute_pedantic.html\
?PageSpeedFilters=add_instrumentation
# check for type attribute
check [ $(grep -c "<script>window.mod_pagespeed_start" $WGET_OUTPUT) = 1 ]
check [ $(grep -c "<script data-pagespeed-no-defer>" $WGET_OUTPUT) = 1 ]

