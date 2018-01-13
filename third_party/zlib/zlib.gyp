# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'conditions': [
      [ 'os_posix == 1 and OS != "mac" and OS != "openbsd"', {
        # Link to system .so since we already use it due to GTK.
        # TODO(pvalchev): OpenBSD is purposefully left out, as the system
        # zlib brings up an incompatibility that breaks rendering.
        'use_system_zlib%': 1,
      }, {  # os_posix != 1 or OS == "mac" or OS == "openbsd"
        'use_system_zlib%': 0,
      }],
    ],
  },
  'conditions': [
    ['use_system_zlib==0', {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'static_library',
          'sources': [
            'src/adler32.c',
            'src/compress.c',
            'src/crc32.c',
            'src/crc32.h',
            'src/deflate.c',
            'src/deflate.h',
            'src/infback.c',
            'src/inffast.c',
            'src/inffast.h',
            'src/inffixed.h',
            'src/inflate.c',
            'src/inflate.h',
            'src/inftrees.c',
            'src/inftrees.h',
            'src/trees.c',
            'src/trees.h',
      #      'src/srcuncompr.c',
            'src/zconf.h',
            'src/zlib.h',
            'src/zutil.c',
            'src/zutil.h',
          ],
          'include_dirs': [
            '<(DEPTH)',
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '<(DEPTH)',
              '.',
            ],
          },
        },
      ],
    }, {
      'targets': [
        {
          'target_name': 'zlib',
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_ZLIB',
            ],
          },
          'link_settings': {
            'libraries': [
              '-lz',
            ],
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
