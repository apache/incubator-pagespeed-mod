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
# Author: cheesy@google.com (Steve Hill)
#
# Stand up a gcloud VM, run a script, then take down the VM if necessary.
# Intended to be run by sourcing.
#
# Expects the following variables:
#   delete_existing_machine  If the VM already exists, delete it
#   image_family             Image family used to create VM
#                            See: gcloud compute images list
#   keep_machine             Don't delete the machine on success
#   machine_name             VM name to create
#   use_existing_machine     Re-run script on existing VM
#
# Calls the following hooks:
#   machine_ready            Bash function that's run after the machine is up.
#
# You should set CLOUDSDK_COMPUTE_REGION and/or CLOUDSDK_CORE_PROJECT, or set
# your gcloud defaults befor running this:
# https://cloud.google.com/sdk/gcloud/reference/config/set

# TODO(jefftk): The way this handles argument parsing is not ideal, since any
# script using run_on_vm.sh is going to have some amount of argument overlap,
# but fixing this with hooks for specifying additional arguments is (a) awkward
# and (b) a lot of engineering work for the current two callers.

set -u
set -e

if ! type gcloud >/dev/null 2>&1; then
  echo "gcloud is not in your PATH. See: https://cloud.google.com/sdk/" >&2
  exit 1
fi

case "$image_family" in
  centos-*) image_project=centos-cloud ;;
  ubuntu-*) image_project=ubuntu-os-cloud ;;
  *) echo "This script doesn't recognize image family '$image_family'" >&2;
     exit 1 ;;
esac

instances=$(gcloud compute instances list -q "$machine_name")
if [ -n "$instances" ]; then
  if $delete_existing_machine; then
    echo "Deleting existing $machine_name instance..."
    gcloud -q compute instances delete "$machine_name"
    instances=
  elif ! $use_existing_machine; then
    echo "Instance '$machine_name' already exists." >&2
    exit 1
  fi
fi

if [ -z "$instances" ] || ! $use_existing_machine; then
  echo "Creating new $machine_name instance..."
  gcloud compute instances create "$machine_name" \
    --image-family="$image_family" --image-project="$image_project"
fi

# Display an error including the machine name if we die in the script below.
trap '[ $? -ne 0 ] && echo -e "\nScript failed on $machine_name"' EXIT

echo "Checking whether $machine_name is up; you may see some errors..."
while ! gcloud compute ssh "$machine_name" -- true; do
  echo "Waiting for $machine_name to come up..."
done

machine_ready

if ! $keep_machine; then
  echo "Deleting $machine_name instance..."
  gcloud -q compute instances delete "$machine_name"
fi
