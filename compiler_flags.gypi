# Copyright 2016 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

{
 
  'variables': {
    # Define platform specific compiler and linker flags.
    # Refer to base.gypi for a list of all available variables.
    'compiler_flags_host': [
      '-O2',
    ],
    'compiler_flags': [
      # We'll pretend not to be Linux, but Starboard instead.
      '-U__linux__',

      # Force char to be signed.
      '-fsigned-char',
      # Disable strict aliasing.
      '-fno-strict-aliasing',

      '-fno-omit-frame-pointer',
      '-fno-optimize-sibling-calls',

      # Suppress some warnings that will be hard to fix.
      '-Wno-unused-local-typedefs',
      '-Wno-unused-result',
      '-Wno-deprecated-declarations',
      '-Wno-missing-field-initializers',
      '-Wno-comment',
      '-Wno-narrowing',
      '-Wno-unknown-pragmas',
      '-Wno-sign-compare',
      '-Wno-unused-function',
      '-Wno-nonnull-compare',
      '-Wno-type-limits',  # TODO: We should actually look into these.
      # It's OK not to use some input parameters. Note that the order
      # matters: Wall implies Wunused-parameter and Wno-unused-parameter
      # has no effect if specified before Wall.
      '-Wno-unused-parameter',
      
      '-I=/usr/include',
      '-I=/usr/include/interface/vcos/pthreads',
      '-I=/usr/include/interface/vmcs_host/linux',
      '<!@(pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)'
    ],
    'linker_flags': [
      '-Wl,-rpath /usr/lib',
      '-L/usr/lib',

      # We don't wrap these symbols, but this ensures that they aren't
      # linked in.
      '-Wl,--wrap=malloc',
      '-Wl,--wrap=calloc',
      '-Wl,--wrap=realloc',
      '-Wl,--wrap=memalign',
      '-Wl,--wrap=reallocalign',
      '-Wl,--wrap=free',
      '-Wl,--wrap=strdup',
      '-Wl,--wrap=malloc_usable_size',
      '-Wl,--wrap=malloc_stats_fast',
      '-Wl,--wrap=__cxa_demangle',
      '-Wl,--wrap=eglGetDisplay'
    ],
    'compiler_flags_debug': [
      '-O0',
    ],
    'compiler_flags_cc_debug': [
      '-frtti',
    ],
    'compiler_flags_devel': [
      '-O2',
    ],
    'compiler_flags_cc_devel': [
      '-frtti',
    ],
    'compiler_flags_qa': [
      '-O2',
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_cc_qa': [
      '-fno-rtti',
    ],
    'compiler_flags_gold': [
      '-O2',
      '-Wno-unused-but-set-variable',
    ],
    'compiler_flags_cc_gold': [
      '-fno-rtti',
    ],
    'platform_libraries': [
      '-lpthread',
      '-lrt',
      '-lEGL',
      '-lGLESv2',
      '-lwayland-egl',
      '-lwayland-client',
      '<!@(pkg-config --libs alsa gstreamer-1.0 gstreamer-app-1.0)'
    ],
    'conditions': [
      ['cobalt_fastbuild==0', {
        'compiler_flags_debug': [
          '-g',
        ],
        'compiler_flags_devel': [
          '-g',
        ],
        'compiler_flags_qa': [
        ],
        'compiler_flags_gold': [
        ],
      }],
    ],
  },

  'target_defaults': {
   'defines': [
      # Cobalt on Linux flag
      'COBALT_LINUX',
      '__STDC_FORMAT_MACROS', # so that we get PRI*
      # Enable GNU extensions to get prototypes like ffsl.
     # '_GNU_SOURCE=1',
    ],
    'cflags_c': [
      '-std=gnu11',
    ],
    'cflags_cc': [
      '-std=gnu++11',
      '-Wno-literal-suffix',
    ],
    'target_conditions': [
      ['sb_pedantic_warnings==1', {
        'cflags': [
          '-Wall',
          '-Wextra',
          '-Wunreachable-code',
          # Turn warnings into errors.
          '-Werror',
        ],
      },{
        'cflags': [
          # Do not warn for implicit type conversions that may change a value.
          '-Wno-conversion',
        ],
      }],
    ],
  }, # end of target_defaults

}
