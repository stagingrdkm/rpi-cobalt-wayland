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

#include "abstract_decoder.h"
#include "third_party/starboard/raspi/wayland/abstract_decoder.h"
#include "third_party/starboard/raspi/wayland/player_private.h"

#include <cmath>

GST_DEBUG_CATEGORY_STATIC (ABSTRACT_DECODER);
#define GST_CAT_DEFAULT ABSTRACT_DECODER

AbstractDecoder::AbstractDecoder(SbPlayerPrivate& player, SbMediaType type)
  : Player(player),
    Type(type),
    Source(gst_element_factory_make("appsrc", nullptr))
{
}

AbstractDecoder::~AbstractDecoder()
{
  gst_app_src_end_of_stream(GST_APP_SRC(Source));
  Player.ReportDecoderState(Type, kSbPlayerDecoderStateDestroyed);
}

bool AbstractDecoder::Initialize()
{
  GST_DEBUG_CATEGORY_INIT(ABSTRACT_DECODER, "ABSTRACT_DECODER", 0,
                          "Cobal Gstreamer Decoder");
  // common source setup
  GstAppSrcCallbacks callbacks
    = { AbstractDecoder::NeedData,
      AbstractDecoder::EnoughData,
      AbstractDecoder::SeekData };
  gst_app_src_set_callbacks(GST_APP_SRC(Source), &callbacks, this, nullptr);
  g_object_set(GST_APP_SRC(Source), "format", GST_FORMAT_TIME, "stream-type",
               GST_APP_STREAM_TYPE_SEEKABLE, nullptr);

  // custom part, includeng Caps initialization
  GstCaps* caps = CustomInitialize();
  if (caps == nullptr) {
    return false;
  }

  // so we do not need to supply them with each chunk
  gst_app_src_set_caps(GST_APP_SRC(Source), caps);
  gst_caps_unref(caps);
  return true;
}

void AbstractDecoder::PushWorker(GstBuffer* buffer)
{
  const size_t delta = gst_buffer_get_size(buffer);
  if (delta == 0) {
    return;
  }
  if (GST_FLOW_OK != gst_app_src_push_buffer(GST_APP_SRC(Source), buffer)) {
    gst_buffer_unref(buffer);
    return;
  }
}


void AbstractDecoder::EosWorker()
{
  gst_app_src_end_of_stream(GST_APP_SRC(Source));
}

// static
void AbstractDecoder::NeedData(GstAppSrc*, guint, gpointer userData)
{
  AbstractDecoder& decoder = *reinterpret_cast<AbstractDecoder*>(userData);
  decoder.Player.ReportDecoderState(decoder.Type,
                                    kSbPlayerDecoderStateNeedsData);
}

// static
void AbstractDecoder::EnoughData(GstAppSrc*, gpointer userData)
{
  return;
}

// static
gboolean AbstractDecoder::SeekData(GstAppSrc*, guint64, gpointer userData)
{
  AbstractDecoder& decoder = *reinterpret_cast<AbstractDecoder*>(userData);
  decoder.Player.ReportDecoderState(decoder.Type,
                                    kSbPlayerDecoderStateNeedsData);
  return TRUE;
}
