#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Sets common environment variables requires for building PageSpeed stuff.

devtoolset_dir=/opt/rh/devtoolset-2
devtoolset_enable="${devtoolset_dir}/enable"
if [ -f "$devtoolset_enable" ]; then
  # devtoolset_enable uses possibly unset vars, so disable set -u for it.
  set +u
  source "$devtoolset_enable"
  set -u
  : ${CC:="${devtoolset_dir}/root/usr/bin/gcc"}
  : ${CXX:="${devtoolset_dir}/root/usr/bin/g++"}
  export CC CXX

fi

export CHROOT_DIR=/var/chroot/centos_i386

export SSL_CERT_DIR=/etc/pki/tls/certs
export SSL_CERT_FILE=/etc/pki/tls/cert.pem

export CFLAGS="-DGPR_MANYLINUX1 -std=gnu99 ${CFLAGS:-}"

export PATH=$HOME/bin:/usr/local/bin:$PATH

PKG_EXTENSION=rpm
