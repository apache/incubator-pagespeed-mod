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
# Runs 'siege' on a single ipro-optimized image with memcached.
#
# Usage:
#   devel/siege/siege_ipro_image_memcached.sh

this_dir=$(readlink -e "$(dirname "${BASH_SOURCE[0]}")")
root_dir=$(readlink -e "$this_dir/../..")
install_dir="$root_dir/install"

set -e
"$install_dir/run_program_with_memcached.sh" "$this_dir/siege_ipro_image.sh"
