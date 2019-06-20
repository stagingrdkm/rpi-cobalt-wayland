# Copyright 2016 The Cobalt Authors. All Rights Reserved.
# Copyright 2019 RDK Management
# Copyright 2019 Liberty Global B.V.
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
    'target_arch': 'arm',
    'target_os': 'linux',
    'sysroot%': '/',
    'gl_type': 'system_gles2',
  },
 
  'target_defaults': {
   'default_configuration': 'raspi-wayland_debug',
    'configurations': {
      'raspi-wayland_debug': {
        'inherit_from': ['debug_base'],
      },
      'raspi-wayland_devel': {
        'inherit_from': ['devel_base'],
      },
      'raspi-wayland_qa': {
        'inherit_from': ['qa_base'],
      },
      'raspi-wayland_gold': {
        'inherit_from': ['gold_base'],
      },
    }, # end of configurations
  }, # end of target_defaults

  'includes': [
    'architecture.gypi',
    'compiler_flags.gypi',
  ],

}
