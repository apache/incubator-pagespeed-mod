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
# Runs 'siege' on a single ipro-optimized image.
#
# Usage:
#   devel/siege/siege_ipro_image.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

echo "Waiting for the image to be IPRO-optimized..."
URL="http://localhost:8080/mod_pagespeed_example/images/Puzzle.jpg"

while true; do
  content_length=$(curl -sS -D- -o /dev/null "$URL" | \
     grep '^Content-Length: ' | \
     grep -o '[0-9]*')
  if [ "$content_length" -lt 100000 ]; then
    # the image is fully ipro optimized
    break
  fi
  sleep .1
  echo -n .
done

run_siege "$URL"
