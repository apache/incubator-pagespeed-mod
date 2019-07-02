# Copyright 2010 Google Inc.
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
    'pagespeed_root': '../..',
    'giflib_root': '<(pagespeed_root)/third_party/giflib/src/',
    'giflib_src_root': '<(giflib_root)/lib',
    'giflib_gen_arch_root': '<(giflib_root)/gen/arch/<(OS)/<(target_arch)',
  },
  'targets': [
    {
      'target_name': 'giflib_core',
      'type': '<(library)',
      'sources': [
        'src/lib/gifalloc.c',
        'src/lib/gif_err.c',
        'src/lib/openbsd-reallocarray.c'
      ]
    },
    {
      'target_name': 'dgiflib',
      'type': '<(library)',
      'sources': [
        'src/lib/dgif_lib.c',
      ],
      'dependencies': [
        'giflib_core',
      ],
      'include_dirs': [
        '<(giflib_src_root)',
        '<(giflib_gen_arch_root)/include',
        '<(giflib_gen_arch_root)/include/private',
      ],
      'defines': [
        # We assume that int is 32bit on all platforms. This is the
        # same assumption made in basictypes.h.
        'UINT32=unsigned int',
        '_GBA_NO_FILEIO',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(giflib_src_root)',
          '<(giflib_gen_arch_root)/include',
        ],
      },
      'xcode_settings': {
        'WARNING_CFLAGS': [
          '-Wno-pointer-sign',
        ],
      },
      'cflags': [
        '-Wno-pointer-sign',
      ],
    },
    {
      'target_name': 'egiflib',
      'type': '<(library)',
      'sources': [
        'src/lib/egif_lib.c',
        'src/lib/gif_hash.c'
      ],
      'dependencies': [
        'giflib_core',
      ],
      'include_dirs': [
        '<(giflib_src_root)',
        '<(giflib_gen_arch_root)/include',
        '<(giflib_gen_arch_root)/include/private',
      ],
      'defines': [
        # We assume that int is 32bit on all platforms. This is the
        # same assumption made in basictypes.h.
        'UINT32=unsigned int',
        '_GBA_NO_FILEIO',
        'HAVE_FCNTL_H',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(giflib_src_root)',
          '<(giflib_gen_arch_root)/include',
        ],
      },
      'xcode_settings': {
        'WARNING_CFLAGS': [
          '-Wno-pointer-sign',
        ],
      },
      'cflags': [
        '-Wno-pointer-sign',
      ],
    },
  ],
}
