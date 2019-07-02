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
# Runs 'siege' on a single cache-extended URL cache-extended CSS file
# scraped from rewrite_css.html.
#
# Usage:
#   devel/siege/siege_extended_css.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# Fetch the rewrite_css example in cache-extend mode so we can get a small
# cache-extended CSS file.
EXAMPLE="http://localhost:8080/mod_pagespeed_example"

# The format of the 'link' HTML line we get is this:
# <link rel="stylesheet" type="text/css"
#     href="styles/yellow.css.pagespeed.ce.lzJ8VcVi1l.css">
# The line-break before 'href' is added here to avoid exceeding 80 cols
# in this script but is not in the HTML.
#
# Splitting this by quotes seems a little fragile but it gets us the
# URL in the 6th token.
extract_pagespeed_url $EXAMPLE/rewrite_css.html 'link rel=' 6 extend_cache

run_siege "$EXAMPLE/$url"
