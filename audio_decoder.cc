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

#include "third_party/starboard/raspi/wayland/audio_decoder.h"

#include "starboard/shared/starboard/media/media_support_internal.h"

#include <gst/gst.h>

#include <cstring>


namespace
{
const char* GetAudioCodecName(SbMediaAudioCodec codec)
{
  const char* name;
  switch (codec) {
#define AUDIO_CASE(k) case k: name = #k; break;
  AUDIO_CASE(kSbMediaAudioCodecNone)
  AUDIO_CASE(kSbMediaAudioCodecAac)
#undef AUDIO_CASE
  default:
    name = "UNKNOWN";
  }
  return name;
}
}

SB_EXPORT bool SbMediaIsAudioSupported(SbMediaAudioCodec audioCodec,
                                       int64_t bitrate) {
  // there are problems with Opus in 16.2
  const bool result = (audioCodec == kSbMediaAudioCodecAac)
    && bitrate >= 0ll
    && bitrate <= SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND;
  return result;
}

AudioDecoder::AudioDecoder(SbPlayerPrivate& player,
                           const SbMediaAudioSampleInfo* header)
  : AbstractDecoder(player, kSbMediaTypeAudio),
  Header(),
  AudioSpecificConfig(nullptr)
{
  if (header) {
    Header = *header;
    if (Header.audio_specific_config && Header.audio_specific_config_size) {
      gpointer copy = g_memdup(Header.audio_specific_config,
                               Header.audio_specific_config_size);
      AudioSpecificConfig
        = gst_buffer_new_wrapped(copy, Header.audio_specific_config_size);
    }
  }
}

GstCaps* AudioDecoder::CustomInitialize()
{
  g_object_set(Source, "min-percent", 60u, "max-bytes", 64ull << 10, nullptr);

  gchar* capsString;
  GstCaps* caps = nullptr;
  switch (Player.GetAudioCodec()) {
  case kSbMediaAudioCodecAac:
  if (AudioSpecificConfig) {
    caps = gst_caps_new_simple("audio/mpeg",
                               "mpegversion", G_TYPE_INT, 4,
                               "framed", G_TYPE_BOOLEAN, TRUE,
                               "stream-format", G_TYPE_STRING, "raw",
                               "codec_data", GST_TYPE_BUFFER, AudioSpecificConfig,
                               nullptr);
    gst_buffer_unref(AudioSpecificConfig);
  }
    break;
  default:
    SB_LOG(INFO) << "Unsupported audio codec "
      << GetAudioCodecName(Player.GetAudioCodec());
    return nullptr;
  }

  if (Header.number_of_channels) {
    gst_caps_set_simple(caps,
                        "channels", G_TYPE_INT, Header.number_of_channels,
                        nullptr);
  }
  if (Header.samples_per_second) {
    gst_caps_set_simple(caps,
                        "rate", G_TYPE_INT, Header.samples_per_second,
                        nullptr);
  }
  if (Header.average_bytes_per_second > 100) {
    gst_caps_set_simple(caps,
                        "bitrate", G_TYPE_INT,
                        Header.average_bytes_per_second * 8,
                        nullptr);
  }
  if (Header.block_alignment) {
    gst_caps_set_simple(caps,
                        "block_align", G_TYPE_INT, Header.block_alignment,
                        nullptr);
  }
  if (Header.bits_per_sample) {
    gst_caps_set_simple(caps,
                        "depth", G_TYPE_INT, Header.bits_per_sample,
                        nullptr);
  }

#if 0
  gchar* c = gst_caps_to_string(caps);
  SB_DLOG(INFO) << "audio caps are '" << c << "'";
  g_free(c);
#endif

  return caps;
}

