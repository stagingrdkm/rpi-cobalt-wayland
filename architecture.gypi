# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
    'arm_version': 7,
    'armv7': 1,
    'arm_neon': 1,
    'arch_64bit': 'n',
    'arm_float_abi': 'hard',

    'compiler_flags': [
      '-march=armv7ve',
      '-mthumb',
      '-mcpu=cortex-a7',
      '-mfloat-abi=hard',
      '-mfpu=neon-vfpv4',
    ],
  },
}
