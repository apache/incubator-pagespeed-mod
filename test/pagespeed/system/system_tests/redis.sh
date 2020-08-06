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
start_test Redis database index respected

# the default redis DB is not used, dbsize should equal 0
OUT=$(echo -e 'DBSIZE' | redis-cli -p "${REDIS_PORT}")
check [ "0" -eq "$OUT" ]
# db #2 is primary, and should be non-0 at this point
OUT=$(echo -e 'select 2 \n DBSIZE' | redis-cli -p "${REDIS_PORT}")
check_from "$OUT" grep -Pzo 'OK\n[1-9]'

# TODO(oschaaf): attempts to get the redis tests to run with the
# secondary host available failed. The test above at least makes
# sure that the setting that specifies db index 3 isn't used in
# the primary host which uses db index 2.
# db #3 is secondary, and should be non-0 at this point
#RES=$(echo -e 'select 3 \n DBSIZE' | redis-cli -p "${REDIS_PORT}")
#check_from "$OUT" grep -Pzo 'OK\n[1-9]'
