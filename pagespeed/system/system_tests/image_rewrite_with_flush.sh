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
# Testing image rewrites with parent node as well as child node
# Also involved php flush routine
start_test Image rewrite with flush.
HOST_NAME="http://image-rewrite-with-flush.example.com"
URL="http://image-rewrite-with-flush.example.com/mod_pagespeed_test/image_rewrite_with_flush/image_rewrite_with_flush.php"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c .pagespeed.ic' 2
