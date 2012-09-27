# Copyright 2009 Google Inc.
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

{
  'variables': {
    # Make sure we link statically so everything gets linked into a
    # single shared object.
    'library': 'static_library',

    # We're building a shared library, so everything needs to be built
    # with Position-Independent Code.
    'linux_fpic': 1,

    'instaweb_src_root': 'net/instaweb',

    # Define the overridable use_system_libs variable in its own
    # nested block, so it's available for use in the conditions block
    # below.
    'variables': {
      'use_system_libs%': 0,
    },

    # Which version development is usually done with. This version will
    # get -Werror
    'gcc_devel_version%': '44',

    # This variable is required to build a recent webp gyp given an older
    # surrounding Chromium checkout.
    'order_profiling': 0,

    # We need inter-process mutexes to support POSIX shared memory, and they're
    # unfortunately not supported on some common systems.
    'support_posix_shared_mem%': 0,

    'conditions': [
      # TODO(morlovich): AIX, Solaris, FreeBSD10?
      ['OS == "linux"', {
        'support_posix_shared_mem': 1
      }],
      ['use_system_libs==1', {
        'use_system_apache_dev': 1,
        'use_system_libjpeg': 1,
        'use_system_libpng': 1,
        'use_system_opencv': 1,
        'use_system_zlib': 1,
      },{
        'use_system_apache_dev%': 0,
      }],
    ],
  },
  'includes': [
    '../third_party/libpagespeed/src/build/common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['support_posix_shared_mem == 1', {
        'defines': [ 'PAGESPEED_SUPPORT_POSIX_SHARED_MEM', ],
      }],
      ['OS == "linux"', {
        # Disable -Werror when not using the version of gcc that development
        # is generally done with, to avoid breaking things for users with
        # something older or newer (which produces different warnings).
        'conditions': [
          ['<(gcc_version) != <(gcc_devel_version)', {
          'cflags!': ['-Werror']
          }],
        ],
        'cflags': [
          # Our dependency on OpenCV need us to turn on exceptions.
          '-fexceptions',
          # Now we are using exceptions. -fno-asynchronous-unwind-tables is
          # set in libpagespeed's common.gypi. Now enable it.
          '-fasynchronous-unwind-tables',
          # We'd like to add '-Wtype-limits', but this does not work on
          # earlier versions of g++ on supported operating systems.
        ],
        'cflags_cc': [
          '-frtti',  # Hardy's g++ 4.2 <trl/function> uses typeid
        ],
        # Disable -z,defs in linker.
        # This causes mod_pagespeed.so to fail because it doesn't link apache
        # libraries.
        'ldflags!': [
          '-Wl,-z,defs',
        ],
      }],
      ['OS == "mac"', {
        'xcode_settings':{
          'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',        # -fexceptions
          'GCC_ENABLE_CPP_RTTI': 'YES',              # -frtti

          # The Google CSS parser escapes from functions without
          # returning anything.  Only with flow analysis that is,
          # evidently, beyond the scope of the g++ configuration in
          # MacOS, do we see those paths cannot be reached.
          'OTHER_CFLAGS': ['-funsigned-char', '-Wno-error'],
        },
      }],
    ],

    # We don't want -std=gnu++0x (enabled by some versions of libpagespeed)
    # since it can cause binary compatibility problems; see issue 453.
    'cflags!': [
      '-std=gnu++0x'
    ],

    # Permit building us with coverage information
    'configurations': {
      'Debug_Coverage': {
        'inherit_from': ['Debug'],
        'cflags': [
          '-ftest-coverage',
          '-fprofile-arcs',
        ],
        'ldflags': [
          # takes care of -lgcov for us, but can be in a build configuration
          '-ftest-coverage -fprofile-arcs',
        ],
      },
    },
  },
}
