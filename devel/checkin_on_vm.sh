#!/bin/bash
#
# Copyright 2016 Google Inc.
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
#
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs mod_pagespeed checkin tests on a gcloud VM.
#
# You may want to set CLOUDSDK_COMPUTE_REGION and/or CLOUDSDK_CORE_PROJECT,
# or set your gcloud defaults befor running this:
# https://cloud.google.com/sdk/gcloud/reference/config/set

set -u

this_dir=$(dirname $0)

branch=
delete_existing_machine=true
image_family=ubuntu-1404-lts
keep_machine=false
machine_name=
use_existing_machine=false

options="$(getopt --long branch:,machine_name:,use_existing_machine \
    -o '' -- "$@")"
if [ $? -ne 0 ]; then
  echo "Usage: $(basename "$0") [options]" >&2
  echo "  --branch=<branch>               mod_pagespeed branch to test" >&2
  echo "  --machine_name=<machine_name>   VM name to create (optional)" >&2
  echo "  --use_existing_machine          Re-run script on existing VM" >&2
  exit 1
fi

set -e
eval set -- "$options"

while [ $# -gt 0 ]; do
  case "$1" in
    --branch) branch="$2"; shift 2 ;;
    --machine_name) machine_name="$2"; shift 2 ;;
    --use_existing_machine) use_existing_machine=true;
                            delete_existing_machine=false; shift ;;
    --) shift; break ;;
    *) echo "getopt error" >&2; exit 1 ;;
  esac
done

if [ -z "$branch" ]; then
  echo "Must set --branch" >&2
  exit 1
fi

if [ -z "$machine_name" ]; then
  sanitized_branch="$(tr _ - <<< "$branch" | tr -d .)"
  machine_name="${USER}-checkin-${sanitized_branch}"
fi

# Empty final argument is to placate -u
remaining_arguments=( "$@" "" )

# Hook for run_on_vm.sh to call.
function machine_ready() {
  gcloud compute ssh "$machine_name" -- bash << EOF
  set -e
  set -x
  sudo apt-get -y update
  sudo apt-get -y upgrade
  sudo apt-get -y install git
  if ! [ -d mod_pagespeed ]; then
    git clone -b "$branch" https://github.com/pagespeed/mod_pagespeed.git
  fi
  cd mod_pagespeed
  git submodule update --init --recursive
  if ! [ -d ~/apache2 ]; then
    install/build_development_apache.sh 2.2 prefork
  fi
  devel/checkin ${remaining_arguments[@]}
EOF
}

source "$this_dir/../install/run_on_vm.sh"
