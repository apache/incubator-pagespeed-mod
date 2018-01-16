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

this_dir=$(dirname "${BASH_SOURCE[0]}")

sieges=(extended_css extended_js html_high_entropy html instant_ipro \
  ipro_image ipro_image_memcached rewritten_css rewritten_js)

echo "$(date): Starting ${#sieges[*]} sieges ..."

# So we can make the transactions/second line up, figure out the padding
# we'll need for smaller siege-names.
max_len=0
for siege in "${sieges[@]}"; do
  len=${#siege}
  if [ "$len" -gt "$max_len" ]; then
    max_len=$len
  fi
done

status=0
for siege in ${sieges[*]}; do
  out="/tmp/siege_$siege.out"

  # Make the columns line up by padding with spaces before the >& and "."
  # before the transactions-per-second.
  dots=$(eval printf "%0.s." {0..$((max_len - ${#siege}))})
  spaces=$(echo "$dots" | sed -e 's/./ /g')
  echo -n "$this_dir/siege_$siege.sh$spaces >& $out$dots.."
  "$this_dir/siege_$siege.sh" &> "$out"
  if [ $? -eq 0 ]; then
    grep "Transaction rate:" "$out" |cut -f2 -d: | \
        sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//'
  else
    echo FAILED
    status=1
  fi
done
echo "$(date): Finished sieges, exit status $status"
exit $status
