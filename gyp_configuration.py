# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Raspberry Pi platform configuration."""

import logging
import os
import sys

from starboard.build import clang
from starboard.build import platform_configuration
from starboard.tools import build
from starboard.tools.testing import test_filter


class RaspiwaylandPlatformConfig(platform_configuration.PlatformConfiguration):
  """Starboard Raspberry Pi platform configuration."""

  def __init__(self, platform):
    super(RaspiwaylandPlatformConfig, self).__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and qtcreator_ninja will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja'

  def GetVariables(self, configuration):
    variables = super(RaspiwaylandPlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 0,
        'javascript_engine': 'v8',
        'cobalt_enable_jit': 1,
    })

    return variables

  def GetEnvironmentVariables(self):
    env_variables = build.GetHostCompilerEnvironment(
        None, False)
    env_variables.update({
        'CC': os.environ['CC'],
        'CXX': os.environ['CXX'],
        'LD': os.environ['CXX']
    })
    return env_variables

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

def CreatePlatformConfig():
  return RaspiwaylandPlatformConfig('raspi-wayland')
