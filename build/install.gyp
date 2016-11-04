# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'install_path': '<(DEPTH)/install',
    'version_py_path': '<(DEPTH)/build/version.py',
    'version_path': '<(DEPTH)/net/instaweb/public/VERSION',
    'lastchange_path': '<(SHARED_INTERMEDIATE_DIR)/build/LASTCHANGE',
    'branding_dir': '<(install_path)/common',
  },
  'conditions': [
    ['OS=="linux"', {
      'variables': {
        'version' : '<!(python <(version_py_path) -f <(version_path) -t "@MAJOR@.@MINOR@.@BUILD@.@PATCH@")',
        'revision' : '<!(if [ -f <(DEPTH)/LASTCHANGE.in ]; then cat <(DEPTH)/LASTCHANGE.in | cut -d= -f2; else git rev-list --all --count; fi)',
        'packaging_files_common': [
          '<(install_path)/common/apt.include',
          '<(install_path)/common/mod-pagespeed/mod-pagespeed.info',
          '<(install_path)/common/installer.include',
          '<(install_path)/common/repo.cron',
          '<(install_path)/common/rpm.include',
          '<(install_path)/common/rpmrepo.cron',
          '<(install_path)/common/updater',
          '<(install_path)/common/variables.include',
          '<(install_path)/common/BRANDING',
          '<(install_path)/common/pagespeed.load.template',
          '<(install_path)/common/pagespeed.conf.template',
        ],
        'packaging_files_deb': [
          '<(install_path)/debian/build.sh',
          '<(install_path)/debian/changelog.template',
          '<(install_path)/debian/conffiles',
          '<(install_path)/debian/control.template',
          '<(install_path)/debian/postinst',
          '<(install_path)/debian/postrm',
          '<(install_path)/debian/prerm',
        ],
        'packaging_files_rpm': [
          '<(install_path)/rpm/build.sh',
          '<(install_path)/rpm/mod-pagespeed.spec.template',
        ],
        'packaging_files_binaries': [
          '<(PRODUCT_DIR)/libmod_pagespeed.so',
          '<(PRODUCT_DIR)/libmod_pagespeed_ap24.so',
        ],
        'flock_bash': ['flock', '--', '/tmp/linux_package_lock', 'bash'],
        'deb_build': '<(PRODUCT_DIR)/install/debian/build.sh',
        'rpm_build': '<(PRODUCT_DIR)/install/rpm/build.sh',
        'deb_cmd': ['<@(flock_bash)', '<(deb_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'rpm_cmd': ['<@(flock_bash)', '<(rpm_build)', '-o' '<(PRODUCT_DIR)',
                    '-b', '<(PRODUCT_DIR)', '-a', '<(target_arch)'],
        'conditions': [
          ['target_arch=="ia32"', {
            'deb_arch': 'i386',
            'rpm_arch': 'i386',
          }],
          ['target_arch=="x64"', {
            'deb_arch': 'amd64',
            'rpm_arch': 'x86_64',
          }],
        ],
      },
      'targets': [
        {
          'target_name': 'linux_installer_configs',
          'type': 'none',
          # Add these files to the build output so the build archives will be
          # "hermetic" for packaging.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/install/debian/',
              'files': [
                '<@(packaging_files_deb)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/install/rpm/',
              'files': [
                '<@(packaging_files_rpm)',
              ]
            },
            {
              'destination': '<(PRODUCT_DIR)/install/common/',
              'files': [
                '<@(packaging_files_common)',
              ]
            },
          ],
          'actions': [
            {
              'action_name': 'save_build_info',
              'inputs': [
                '<(branding_dir)/BRANDING',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(PRODUCT_DIR)/installer/version.txt',
              ],
              # Just output the default version info variables.
              'action': [
                'python', '<(version_py_path)',
                '-f', '<(branding_dir)/BRANDING',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '-o', '<@(_outputs)'
              ],
            },
          ],
        },
        {
          'target_name': 'linux_package_deb_stable',
          'suppress_wildcard': 1,
          'variables': {
            'channel': 'stable',
          },
          'includes': [
            'linux_package_deb.gypi',
          ],
        },
        {
          'target_name': 'linux_package_deb_beta',
          'suppress_wildcard': 1,
          'variables': {
            'channel': 'beta',
          },
          'includes': [
            'linux_package_deb.gypi',
          ],
        },
        {
          'target_name': 'linux_package_rpm_stable',
          'suppress_wildcard': 1,
          'variables': {
            'channel': 'stable',
          },
          'includes': [
            'linux_package_rpm.gypi',
          ],
        },
        {
          'target_name': 'linux_package_rpm_beta',
          'suppress_wildcard': 1,
          'variables': {
            'channel': 'beta',
          },
          'includes': [
            'linux_package_rpm.gypi',
          ],
        },
      ],
    },{
      'targets': [
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
