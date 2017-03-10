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


# Are you adding a JS file for open-source compilation? If so, just follow these
# simple steps.
# First, add a target for compiling the JS.
# * If your file has NO dependencies (does not use goog.require, any closure
#   files, js_utils.js, etc.) then just add it to js_files_no_deps.
# * If your file has a dependency ONLY on js/js_utils.js (no closure deps), then
#   add it js_files_utils_dep.
# * If your file has a dependency on another file but NOT on closure, add a pair
#   of new targets (opt and dbg) and add a js_includes field with the other
#   dependencies.
# * If you made it this far, you have a closure dependency. Add a pair of new
#   targets (opt and dbg) and specify the required extra_closure_flags
#   (--entry_point and '<@(include_closure_library)'). Also add
#   js_includes if you need it (needed if your file uses js_utils.js). See
#   critical_images_beacon for an example.
# Then add data2c targets for the dbg and opt builds. No trickery here, these
# are just boilerplate.

{
  'variables': {
    'instaweb_root': '../..',
    'protoc_out_dir': '<(SHARED_INTERMEDIATE_DIR)/protoc_out/instaweb',
    'protoc_executable':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)protoc<(EXECUTABLE_SUFFIX)',
    'compiled_js_dir': '<(SHARED_INTERMEDIATE_DIR)/closure_out/instaweb',
    'data2c_out_dir': '<(SHARED_INTERMEDIATE_DIR)/data2c_out/instaweb',
    'data2c_exe':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)instaweb_data2c<(EXECUTABLE_SUFFIX)',
    'js_minify':
        '<(PRODUCT_DIR)/<(EXECUTABLE_PREFIX)js_minify<(EXECUTABLE_SUFFIX)',
    # The logical way to include the closure library would be to pass
    #   '--js', '<(instaweb_root)/third_party/closure_library',
    # to the closure compiler, but unfortunately this leads the compiler to
    # concatenate dependencies in an order that varies by machine.  This doesn't
    # cause functional problems, but it means git thinks the file has changed
    # when it hasn't actually.  To get deterministic behavior you can use the
    # Java API, which lets you setDependencySorting(true), but with the command
    # line client the best you can do is find all the js files that make up the
    # closure library and pass them to the compiler in a deterministic order.
    'include_closure_library':
        '<!(echo --dependency_mode=STRICT'
        '    $(find <(instaweb_root)/third_party/closure_library/closure '
        '           <(instaweb_root)/third_party/closure_library/third_party '
        '           -name "*.js"'
        '           | grep -v _test.js | sort | sed "s/^/--js /"))',
    # Setting chromium_code to 1 turns on extra warnings. Also, if the compiler
    # is whitelisted in our common.gypi, those warnings will get treated as
    # errors.
    'chromium_code': 1,
    'js_files_no_deps': [
      'rewriter/add_instrumentation.js',
      'rewriter/client_domain_rewriter.js',
      'rewriter/dedup_inlined_images.js',
      'rewriter/defer_iframe.js',
      'rewriter/delay_images_inline.js',
      'rewriter/deterministic.js',
      'rewriter/extended_instrumentation.js',
    ],
    'js_files_utils_dep' : [
      'rewriter/critical_css_beacon.js',
      'rewriter/lazyload_images.js',
      'rewriter/delay_images.js',
      'rewriter/js_defer.js',
      'rewriter/local_storage_cache.js',
    ],
  },
  'targets': [
    {
      'target_name': 'js_dbg',
      'variables': {
        'js_dir': 'rewriter',
        'closure_build_type': 'dbg',
      },
      'sources': [ '<@(js_files_no_deps)' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'js_opt',
      'variables': { 'js_dir': 'rewriter', },
      'sources': [ '<@(js_files_no_deps)' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'js_utils_dep_dbg',
      'variables': {
        'js_dir': 'rewriter',
        'closure_build_type': 'dbg',
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ '<@(js_files_utils_dep)', ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'js_utils_dep_opt',
      'variables': {
        'js_dir': 'rewriter',
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ '<@(js_files_utils_dep)', ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'caches_js_dbg',
      'variables': {
        'js_dir': 'system',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Caches',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/caches.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'caches_js_opt',
      'variables': {
        'js_dir': 'system',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Caches',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/caches.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'console_js_dbg',
      'variables': {
        'js_dir': 'system',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--externs=<(DEPTH)/pagespeed/system/js_externs.js',
          '--externs=<(DEPTH)/third_party/closure/externs/google_visualization_api.js',
          '--entry_point=goog:pagespeed.Console',
          '--entry_point=goog:pagespeed.statistics',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/console.js' ],
      'js_includes': [ '<(DEPTH)/pagespeed/system/console_start.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'console_js_opt',
      'variables': {
        'js_dir': 'system',
        'extra_closure_flags': [
          '--externs=<(DEPTH)/pagespeed/system/js_externs.js',
          '--externs=<(DEPTH)/third_party/closure/externs/google_visualization_api.js',
          '--entry_point=goog:pagespeed.Console',
          '--entry_point=goog:pagespeed.statistics',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/console.js' ],
      'js_includes': [ '<(DEPTH)/pagespeed/system/console_start.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'critical_css_loader_js_dbg',
      'variables': {
        'js_dir': 'rewriter',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.CriticalCssLoader',
          '<@(include_closure_library)',
        ],
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ 'rewriter/critical_css_loader.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'critical_css_loader_js_opt',
      'variables': {
        'js_dir': 'rewriter',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.CriticalCssLoader',
          '<@(include_closure_library)',
        ],
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ 'rewriter/critical_css_loader.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'critical_images_beacon_js_dbg',
      'variables': {
        'js_dir': 'rewriter',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.CriticalImages',
          '<@(include_closure_library)',
        ],
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ 'rewriter/critical_images_beacon.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'critical_images_beacon_js_opt',
      'variables': {
        'js_dir': 'rewriter',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.CriticalImages',
          '<@(include_closure_library)',
        ],
        'js_includes': [ 'js/js_utils.js' ],
      },
      'sources': [ 'rewriter/critical_images_beacon.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'graphs_js_dbg',
      'variables': {
        'js_dir': 'system',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--externs=<(DEPTH)/third_party/closure/externs/google_visualization_api.js',
          '--entry_point=goog:pagespeed.Graphs',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/graphs.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'graphs_js_opt',
      'variables': {
        'js_dir': 'system',
        'extra_closure_flags': [
          '--externs=<(DEPTH)/third_party/closure/externs/google_visualization_api.js',
          '--entry_point=goog:pagespeed.Graphs',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/graphs.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'messages_js_dbg',
      'variables': {
        'js_dir': 'system',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Messages',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/messages.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'messages_js_opt',
      'variables': {
        'js_dir': 'system',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Messages',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/messages.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'statistics_js_dbg',
      'variables': {
        'js_dir': 'system',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Statistics',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/statistics.js' ],
      'includes': [ 'closure.gypi', ],
    },
    {
      'target_name': 'statistics_js_opt',
      'variables': {
        'js_dir': 'system',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Statistics',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ '<(DEPTH)/pagespeed/system/statistics.js' ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'responsive_js_dbg',
      'variables': {
        'js_dir': 'rewriter',
        'closure_build_type': 'dbg',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Responsive',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ 'rewriter/responsive.js', ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'responsive_js_opt',
      'variables': {
        'js_dir': 'rewriter',
        'extra_closure_flags': [
          '--entry_point=goog:pagespeed.Responsive',
          '<@(include_closure_library)',
        ],
      },
      'sources': [ 'rewriter/responsive.js', ],
      'includes': [ 'closure.gypi', ],
    },

    {
      'target_name': 'instaweb_data2c',
      'type': 'executable',
      'sources': [
         'js/data_to_c.cc',
       ],
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'instaweb_add_instrumentation_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'add_instrumentation',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/add_instrumentation_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_add_instrumentation_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'add_instrumentation_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/add_instrumentation_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_extended_instrumentation_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'extended_instrumentation',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/extended_instrumentation_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_extended_instrumentation_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'extended_instrumentation_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/extended_instrumentation_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_client_domain_rewriter_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'client_domain_rewriter',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/client_domain_rewriter_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_client_domain_rewriter_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'client_domain_rewriter_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/client_domain_rewriter_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_beacon_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_css_beacon',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_css_beacon_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_beacon_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_css_beacon_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_css_beacon_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_loader_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_css_loader',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_css_loader_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_css_loader_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_css_loader_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_css_loader_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_images_beacon_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_images_beacon',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_images_beacon_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_critical_images_beacon_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'critical_images_beacon_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/critical_images_beacon_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_dedup_inlined_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'dedup_inlined_images',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/dedup_inlined_images_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_dedup_inlined_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'dedup_inlined_images_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/dedup_inlined_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_defer_iframe_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'defer_iframe',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/defer_iframe_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_defer_iframe_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'defer_iframe_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/defer_iframe_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'delay_images',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/delay_images_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'delay_images_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/delay_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_inline_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'delay_images_inline',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/delay_images_inline_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_delay_images_inline_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'delay_images_inline_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/delay_images_inline_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_deterministic_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'deterministic',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/deterministic_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_deterministic_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'deterministic_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/deterministic_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_js_defer_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'js_defer',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/js_defer_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_js_defer_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'js_defer_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/js_defer_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_messages_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'messages_js',
      },
      'sources': [
        '<(compiled_js_dir)/system/messages_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_messages_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'messages_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/system/messages_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_caches_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'caches_js',
      },
      'sources': [
        '<(compiled_js_dir)/system/caches_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_caches_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'caches_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/system/caches_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_graphs_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'graphs_js',
      },
      'sources': [
        '<(compiled_js_dir)/system/graphs_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_graphs_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'graphs_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/system/graphs_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_statistics_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'statistics_js',
      },
      'sources': [
        '<(compiled_js_dir)/system/statistics_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_statistics_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'statistics_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/system/statistics_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_lazyload_images_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'lazyload_images',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/lazyload_images_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_lazyload_images_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'lazyload_images_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/lazyload_images_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_local_storage_cache_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'local_storage_cache',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/local_storage_cache_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_local_storage_cache_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'local_storage_cache_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/local_storage_cache_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_responsive_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'responsive_js',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/responsive_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_responsive_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'net/instaweb/rewriter',
        'instaweb_js_subdir': '<(compiled_js_dir)/rewriter',
        'var_name': 'responsive_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/rewriter/responsive_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_console_js_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'console_js',
      },
      'sources': [
        '<(compiled_js_dir)/system/console_dbg.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_console_js_opt_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': '<(compiled_js_dir)/system',
        'var_name': 'console_js_opt',
      },
      'sources': [
        '<(compiled_js_dir)/system/console_opt.js',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_admin_site_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': 'system',
        'var_name': 'admin_site_css',
      },
      'sources': [
        '<(DEPTH)/pagespeed/system/admin_site.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_caches_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': 'system',
        'var_name': 'caches_css',
      },
      'sources': [
        '<(DEPTH)/pagespeed/system/caches.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_console_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': 'system',
        'var_name': 'console_css',
      },
      'sources': [
        '<(DEPTH)/pagespeed/system/console.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_graphs_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': 'system',
        'var_name': 'graphs_css',
      },
      'sources': [
        '<(DEPTH)/pagespeed/system/graphs.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_statistics_css_data2c',
      'variables': {
        'instaweb_data2c_subdir': 'pagespeed/system',
        'instaweb_js_subdir': 'system',
        'var_name': 'statistics_css',
      },
      'sources': [
        '<(DEPTH)/pagespeed/system/statistics.css',
      ],
      'includes': [
        'data2c.gypi',
      ]
    },
    {
      'target_name': 'instaweb_spriter_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/spriter/public',
      },
      'sources': [
        'spriter/public/image_spriter.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/image_spriter.pb.cc',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_spriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter_pb',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      ],
      'sources': [
          'spriter/libpng_image_library.cc',
          'spriter/image_library_interface.cc',
          'spriter/image_spriter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_spriter_test',
      'type': '<(library)',
      'dependencies': [
        'instaweb_spriter',
        'instaweb_spriter_pb',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
        '<(DEPTH)/third_party/protobuf/protobuf.gyp:protoc#host',
      ],
      'sources': [
        'spriter/image_spriter_test.cc',
        'spriter/libpng_image_library_test.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
      ],
    },
    {
      'target_name': 'instaweb_dependencies_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/dependencies.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/dependencies.pb.cc',
      ],
      'dependencies': [
        'instaweb_input_info_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_flush_early_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/flush_early.proto',
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/flush_early.pb.cc',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rendered_image_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/rendered_image.pb.cc',
        'rewriter/rendered_image.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_images_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_images.pb.cc',
        'rewriter/critical_images.proto',
      ],
      'dependencies': [
        'instaweb_critical_keys_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_critical_keys_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/critical_keys.pb.cc',
        'rewriter/critical_keys.proto',
      ],
      'dependencies': [
      ],
      'includes': [
        'protoc.gypi',
      ],
    },

    {
      'target_name': 'instaweb_http',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
      ],
    },
    {
      'target_name': 'instaweb_input_info_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/input_info.pb.cc',
        'rewriter/input_info.proto',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/cached_result.pb.cc',
        'rewriter/cached_result.proto',
      ],
      'dependencies': [
        'instaweb_dependencies_pb',
        'instaweb_input_info_pb',
        'instaweb_spriter_pb',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_html_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/rewrite_filter_names.gperf',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:util',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'includes': [
        'gperf.gypi',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_csp_gperf',
      'variables': {
        'instaweb_gperf_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        'rewriter/csp_directive.gperf',
      ],
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:util',
      ],
      'includes': [
        'gperf.gypi',
      ],
    },
    {
      'target_name': 'instaweb_static_asset_config_pb',
      'variables': {
        'instaweb_protoc_subdir': 'net/instaweb/rewriter',
      },
      'sources': [
        '<(protoc_out_dir)/<(instaweb_protoc_subdir)/static_asset_config.pb.cc',
        'rewriter/static_asset_config.proto',
      ],
      'includes': [
        'protoc.gypi',
      ],
    },
    {
      # TODO(lsong): break this up into sub-libs (mocks, real, etc)
      'target_name': 'instaweb_util',
      'type': '<(library)',
      'dependencies': [
        '<(instaweb_root)/third_party/base64/base64.gyp:base64',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_cache',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_sharedmem',
        '<(DEPTH)/pagespeed/kernel.gyp:util',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/pagespeed/opt.gyp:pagespeed_logging',
        '<(DEPTH)/pagespeed/opt.gyp:pagespeed_opt_http',
        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
      ],
      'sources': [
        # TODO(sligocki): Move http/ files to pagespeed_fetch.
        'http/async_fetch.cc',
        'http/async_fetch_with_lock.cc',
        'http/cache_url_async_fetcher.cc',
        'http/external_url_fetcher.cc',
        'http/http_cache.cc',
        'http/http_cache_failure.cc',
        'http/http_dump_url_async_writer.cc',
        'http/http_dump_url_fetcher.cc',
        'http/http_response_parser.cc',
        'http/http_value.cc',
        'http/http_value_writer.cc',
        'http/inflating_fetch.cc',
        'http/rate_controller.cc',
        'http/rate_controlling_url_async_fetcher.cc',
        'http/sync_fetcher_adapter_callback.cc',
        'http/url_async_fetcher.cc',
        'http/url_async_fetcher_stats.cc',
        'http/wait_url_async_fetcher.cc',
        'http/wget_url_fetcher.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
      'export_dependent_settings': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
      ],
    },
    { 'target_name': 'http_value_explorer',
      'type': 'executable',
      'sources': [
        'http/http_value_explorer.cc'
      ],
      'dependencies': [
        ':instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_base_core',
        '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'instaweb_htmlparse',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'htmlparse/file_driver.cc',
        'htmlparse/file_statistics_log.cc',
        'htmlparse/logging_html_filter.cc',
        'htmlparse/statistics_log.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
      'export_dependent_settings': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
      ],
    },
    {
      'target_name': 'instaweb_http_test',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'http/counting_url_async_fetcher.cc',
        '<(DEPTH)/pagespeed/kernel.gyp:kernel_test_util'
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_base',
      'type': '<(library)',
      'dependencies': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
        'instaweb_critical_keys_pb',
        'instaweb_flush_early_pb',
        'instaweb_rendered_image_pb',
        'instaweb_rewriter_html_gperf',
        'instaweb_rewriter_pb',
        'instaweb_static_asset_config_pb',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_cache',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'sources': [
        'config/rewrite_options_manager.cc',
        'config/measurement_proxy_rewrite_options_manager.cc',
        'rewriter/beacon_critical_images_finder.cc',
        'rewriter/critical_images_finder.cc',
        'rewriter/device_properties.cc',
        'rewriter/domain_lawyer.cc',
        'rewriter/downstream_cache_purger.cc',
        'rewriter/downstream_caching_directives.cc',
        'rewriter/inline_output_resource.cc',
        'rewriter/output_resource.cc',
        'rewriter/request_properties.cc',
        'rewriter/resource.cc',
        'rewriter/resource_namer.cc',
        'rewriter/rewrite_options.cc',
        'rewriter/server_context.cc',
        'rewriter/static_asset_manager.cc',
        'rewriter/url_namer.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
      'export_dependent_settings': [
        'instaweb_core.gyp:instaweb_htmlparse_core',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_image',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_image_processing',
      ],
      'sources': [
        'rewriter/image.cc',
        'rewriter/image_url_encoder.cc',
        'rewriter/webp_optimizer.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(DEPTH)/third_party/libwebp/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'js_minify',
      'type': 'executable',
      'sources': [
         'rewriter/js_minify_main.cc',
       ],
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
        '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
    },
    {
      'target_name': 'instaweb_rewriter_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
      ],
      'sources': [
        'rewriter/javascript_code_block.cc',
        'rewriter/javascript_filter.cc',
        'rewriter/javascript_library_identification.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter_css',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter_base',
        'instaweb_util',
        'instaweb_spriter',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/association_transformer.cc',
        'rewriter/css_absolutify.cc',
        'rewriter/css_combine_filter.cc',
        'rewriter/css_filter.cc',
        'rewriter/css_hierarchy.cc',
        'rewriter/css_image_rewriter.cc',
        'rewriter/css_inline_import_to_link_filter.cc',
        'rewriter/css_minify.cc',
        'rewriter/css_resource_slot.cc',
        'rewriter/css_summarizer_base.cc',
        'rewriter/css_url_counter.cc',
        'rewriter/css_url_encoder.cc',
        'rewriter/css_util.cc',
        'rewriter/image_combine_filter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
        '<(DEPTH)/third_party/css_parser/src',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_javascript',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_javascript_gperf',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_rewriter',
      'type': '<(library)',
      'dependencies': [
        'instaweb_add_instrumentation_data2c',
        'instaweb_add_instrumentation_opt_data2c',
        'instaweb_admin_site_css_data2c',
        'instaweb_caches_css_data2c',
        'instaweb_caches_js_data2c',
        'instaweb_caches_js_opt_data2c',
        'instaweb_client_domain_rewriter_data2c',
        'instaweb_client_domain_rewriter_opt_data2c',
        'instaweb_console_css_data2c',
        'instaweb_console_js_data2c',
        'instaweb_console_js_opt_data2c',
        'instaweb_core.gyp:instaweb_rewriter_html',
        'instaweb_critical_css_beacon_data2c',
        'instaweb_critical_css_beacon_opt_data2c',
        'instaweb_critical_css_loader_data2c',
        'instaweb_critical_css_loader_opt_data2c',
        'instaweb_critical_images_beacon_data2c',
        'instaweb_critical_images_beacon_opt_data2c',
        'instaweb_critical_images_pb',
        'instaweb_critical_keys_pb',
        'instaweb_dedup_inlined_images_data2c',
        'instaweb_dedup_inlined_images_opt_data2c',
        'instaweb_defer_iframe_data2c',
        'instaweb_defer_iframe_opt_data2c',
        'instaweb_delay_images_data2c',
        'instaweb_delay_images_inline_data2c',
        'instaweb_delay_images_inline_opt_data2c',
        'instaweb_delay_images_opt_data2c',
        'instaweb_dependencies_pb',
        'instaweb_deterministic_data2c',
        'instaweb_deterministic_opt_data2c',
        'instaweb_extended_instrumentation_data2c',
        'instaweb_extended_instrumentation_opt_data2c',
        'instaweb_flush_early_pb',
        'instaweb_graphs_css_data2c',
        'instaweb_graphs_js_data2c',
        'instaweb_graphs_js_opt_data2c',
        'instaweb_js_defer_data2c',
        'instaweb_js_defer_opt_data2c',
        'instaweb_lazyload_images_data2c',
        'instaweb_lazyload_images_opt_data2c',
        'instaweb_local_storage_cache_data2c',
        'instaweb_local_storage_cache_opt_data2c',
        'instaweb_messages_js_data2c',
        'instaweb_messages_js_opt_data2c',
        'instaweb_responsive_js_data2c',
        'instaweb_responsive_js_opt_data2c',
        'instaweb_rewriter_base',
        'instaweb_rewriter_csp_gperf',
        'instaweb_rewriter_css',
        'instaweb_rewriter_image',
        'instaweb_rewriter_javascript',
        'instaweb_rewriter_pb',
        'instaweb_spriter',
        'instaweb_statistics_css_data2c',
        'instaweb_statistics_js_data2c',
        'instaweb_statistics_js_opt_data2c',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/controller.gyp:pagespeed_controller',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/pagespeed/opt.gyp:pagespeed_ads_util',
        '<(DEPTH)/third_party/css_parser/css_parser.gyp:css_parser',
      ],
      'sources': [
        'rewriter/add_head_filter.cc',
        'rewriter/add_ids_filter.cc',
        'rewriter/add_instrumentation_filter.cc',
        'rewriter/base_tag_filter.cc',
        'rewriter/cache_extender.cc',
        'rewriter/cacheable_resource_base.cc',
        'rewriter/collect_dependencies_filter.cc',
        'rewriter/common_filter.cc',
        'rewriter/critical_css_beacon_filter.cc',
        'rewriter/critical_finder_support_util.cc',
        'rewriter/critical_images_beacon_filter.cc',
        'rewriter/critical_selector_filter.cc',
        'rewriter/critical_selector_finder.cc',
        'rewriter/css_inline_filter.cc',
        'rewriter/css_move_to_head_filter.cc',
        'rewriter/css_outline_filter.cc',
        'rewriter/css_tag_scanner.cc',
        'rewriter/csp.cc',
        'rewriter/data_url_input_resource.cc',
        'rewriter/debug_filter.cc',
        'rewriter/decode_rewritten_urls_filter.cc',
        'rewriter/dedup_inlined_images_filter.cc',
        'rewriter/defer_iframe_filter.cc',
        'rewriter/dependency_tracker.cc',
        'rewriter/responsive_image_filter.cc',
        'rewriter/delay_images_filter.cc',
        'rewriter/deterministic_js_filter.cc',
        'rewriter/dom_stats_filter.cc',
        'rewriter/domain_rewrite_filter.cc',
        'rewriter/experiment_matcher.cc',
        'rewriter/experiment_util.cc',
        'rewriter/file_input_resource.cc',
        'rewriter/file_load_mapping.cc',
        'rewriter/file_load_policy.cc',
        'rewriter/file_load_rule.cc',
        'rewriter/fix_reflow_filter.cc',
        'rewriter/flush_html_filter.cc',
        'rewriter/google_analytics_filter.cc',
        'rewriter/google_font_css_inline_filter.cc',
        'rewriter/google_font_service_input_resource.cc',
        'rewriter/handle_noscript_redirect_filter.cc',
        'rewriter/image_rewrite_filter.cc',
        'rewriter/in_place_rewrite_context.cc',
        'rewriter/inline_attribute_slot.cc',
        'rewriter/inline_resource_slot.cc',
        'rewriter/inline_rewrite_context.cc',
        'rewriter/input_info_utils.cc',
        'rewriter/insert_amp_link_filter.cc',
        'rewriter/insert_dns_prefetch_filter.cc',
        'rewriter/insert_ga_filter.cc',
        'rewriter/js_combine_filter.cc',
        'rewriter/js_defer_disabled_filter.cc',
        'rewriter/js_disable_filter.cc',
        'rewriter/js_inline_filter.cc',
        'rewriter/js_outline_filter.cc',
        'rewriter/js_replacer.cc',
        'rewriter/lazyload_images_filter.cc',
        'rewriter/local_storage_cache_filter.cc',
        'rewriter/measurement_proxy_url_namer.cc',
        'rewriter/make_show_ads_async_filter.cc',
        'rewriter/meta_tag_filter.cc',
        'rewriter/pedantic_filter.cc',
        'rewriter/property_cache_util.cc',
        'rewriter/push_preload_filter.cc',
        'rewriter/redirect_on_size_limit_filter.cc',
        'rewriter/resource_combiner.cc',
        'rewriter/resource_fetch.cc',
        'rewriter/resource_slot.cc',
        'rewriter/resource_tag_scanner.cc',
        'rewriter/rewrite_context.cc',
        'rewriter/rewrite_driver.cc',
        'rewriter/rewrite_driver_factory.cc',
        'rewriter/rewrite_driver_pool.cc',
        'rewriter/rewrite_filter.cc',
        'rewriter/rewrite_query.cc',
        'rewriter/rewrite_stats.cc',
        'rewriter/rewritten_content_scanning_filter.cc',
        'rewriter/scan_filter.cc',
        'rewriter/script_tag_scanner.cc',
        'rewriter/simple_text_filter.cc',
        'rewriter/single_rewrite_context.cc',
        'rewriter/srcset_slot.cc',
        'rewriter/strip_scripts_filter.cc',
        'rewriter/strip_subresource_hints_filter.cc',
        'rewriter/support_noscript_filter.cc',
        'rewriter/url_input_resource.cc',
        'rewriter/url_left_trim_filter.cc',
        'rewriter/url_partnership.cc',
        'rewriter/usage_data_reporter.cc',
      ],
      'include_dirs': [
        '<(instaweb_root)',
        '<(DEPTH)/third_party/css_parser/src',
        '<(DEPTH)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(instaweb_root)',
          '<(DEPTH)',
        ],
      },
    },
    {
      'target_name': 'instaweb_automatic',
      'type': '<(library)',
      'dependencies': [
        'instaweb_critical_keys_pb',
        'instaweb_flush_early_pb',
        'instaweb_rewriter',
        'instaweb_util',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/pagespeed/kernel.gyp:jsminify',
      ],
      'sources': [
        '<(DEPTH)/pagespeed/automatic/html_detector.cc',
        '<(DEPTH)/pagespeed/automatic/proxy_fetch.cc',
        '<(DEPTH)/pagespeed/automatic/proxy_interface.cc',
      ],
    },
    {
      'target_name': 'instaweb_system',
      'type': '<(library)',
      'dependencies': [
        'instaweb_util',
        '<(DEPTH)/build/build_util.gyp:mod_pagespeed_version_header',
        '<(DEPTH)/third_party/serf/serf.gyp:serf',
        '<(DEPTH)/pagespeed/kernel.gyp:pagespeed_http',
        '<(DEPTH)/third_party/apr/apr.gyp:include',
        '<(DEPTH)/third_party/aprutil/aprutil.gyp:include',
        '<(DEPTH)/third_party/domain_registry_provider/src/domain_registry/domain_registry.gyp:init_registry_tables_lib',
        '<(DEPTH)/third_party/grpc/grpc.gyp:grpc_cpp',
        '<(DEPTH)/third_party/hiredis/hiredis.gyp:hiredis',
      ],
      'sources': [
        '<(DEPTH)/pagespeed/system/add_headers_fetcher.cc',
        '<(DEPTH)/pagespeed/system/admin_site.cc',
        '<(DEPTH)/pagespeed/system/apr_mem_cache.cc',
        '<(DEPTH)/pagespeed/system/redis_cache.cc',
        '<(DEPTH)/pagespeed/system/apr_thread_compatible_pool.cc',
        '<(DEPTH)/pagespeed/system/controller_manager.cc',
        '<(DEPTH)/pagespeed/system/external_server_spec.cc',
        '<(DEPTH)/pagespeed/system/in_place_resource_recorder.cc',
        '<(DEPTH)/pagespeed/system/loopback_route_fetcher.cc',
        '<(DEPTH)/pagespeed/system/serf_url_async_fetcher.cc',
        '<(DEPTH)/pagespeed/system/system_cache_path.cc',
        '<(DEPTH)/pagespeed/system/system_caches.cc',
        '<(DEPTH)/pagespeed/system/system_message_handler.cc',
        '<(DEPTH)/pagespeed/system/system_request_context.cc',
        '<(DEPTH)/pagespeed/system/system_rewrite_driver_factory.cc',
        '<(DEPTH)/pagespeed/system/system_rewrite_options.cc',
        '<(DEPTH)/pagespeed/system/system_server_context.cc',
        '<(DEPTH)/pagespeed/system/system_thread_system.cc',
        '<(DEPTH)/third_party/aprutil/apr_memcache2.c',
      ],
    },
    {
      # TODO(sligocki): Why is this called "automatic" util?
      'target_name': 'automatic_util',
      'type': '<(library)',
      'dependencies': [
        '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
        'instaweb_util',
       ],
      'sources': [
        'rewriter/rewrite_gflags.cc',
        '<(DEPTH)/pagespeed/kernel/util/gflags.cc',
      ],
    },
    {
      'target_name': 'process_context',
      'type': '<(library)',
      'dependencies': [
        'instaweb_rewriter',
        '<(DEPTH)/pagespeed/kernel.gyp:proto_util',
        '<(DEPTH)/pagespeed/kernel.gyp:util_gflags',
       ],
      'sources': [
        'rewriter/process_context.cc',
      ],
    },
  ],
}
