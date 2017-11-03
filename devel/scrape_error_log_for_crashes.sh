#!/bin/bash
#
# Copyright 2012 Google Inc.
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

if [ $# -lt 2 ]; then
  echo Usage: $0 error_log_filename stop_filename
  exit 1
fi

patterns="$1"
error_log="$2"
stop_file="$3"
cmd="tail -f $error_log"

# Figure out what the PIDs are of outstanding calls to tail the error
# log.  Note: this would this kill those manually opened as well, but
# I couldn't find up with a better way to find the one we want because
# it becomes detached the way we run it.
function get_tail_pids() {
  ps auxw | grep "$cmd" | grep -v grep | awk '{print $2}'
}

# Kill all the outstanding calls to 'tail'.  Note that "killall tail"
# would kill unrelated 'tail' calls, and "killall tail $error_log"
# does not work.
function kill_tails() {
  echo Killing \'tail\' commands ...
  for pid in $(get_tail_pids); do
    (set -x; kill "$pid")
  done
}

($cmd | egrep "$patterns") &
trap kill_tails EXIT
while [ ! -e "$stop_file" ]; do sleep 10; done

rm -f "$stop_file"
