//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
// Copyright 2017 Arris
// Copyright 2019 RDK Management
// Copyright 2019 Liberty Global B.V.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#pragma once

#include "third_party/starboard/raspi/wayland/player_private.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

//#include "third_party/starboard/wayland/gstreamer_helpers.h"

class AbstractDecoder
{
public:
  explicit AbstractDecoder(SbPlayerPrivate& Player, SbMediaType type);
  AbstractDecoder() = delete;
  AbstractDecoder(const AbstractDecoder&) = delete;
  AbstractDecoder& operator=(const AbstractDecoder&) = delete;
  virtual ~AbstractDecoder();

  bool Initialize();

  void PushWorker(GstBuffer* buffer);
  void EosWorker();

  GstElement* GetElement() const {
    return Source;
  }

protected:
  virtual GstCaps* CustomInitialize() = 0;

  static void NeedData(GstAppSrc*, guint, gpointer userData);
  static void EnoughData(GstAppSrc*, gpointer userData);
  static gboolean SeekData(GstAppSrc*, guint64, gpointer userData);

  SbPlayerPrivate& Player;
  const SbMediaType Type;

  // not owning this element, just a reference
  GstElement* Source;
};

