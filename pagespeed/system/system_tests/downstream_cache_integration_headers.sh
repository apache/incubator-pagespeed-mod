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
start_test Downstream cache integration caching headers.
URL="http://downstreamcacheresource.example.com/mod_pagespeed_example/images/"
URL+="xCuppa.png.pagespeed.ic.0.png"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
check_from "$OUT" egrep -iq $'^Cache-Control: .*\r$'
check_from "$OUT" egrep -iq $'^Expires: .*\r$'
check_from "$OUT" egrep -iq $'^Last-Modified: .*\r$'
