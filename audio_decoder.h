#pragma once

#include "abstract_decoder.h"

#include <cstdint>
#include <memory>
#include <vector>

class AudioDecoder : public AbstractDecoder
{
public:
  AudioDecoder(SbPlayerPrivate& player, const SbMediaAudioSampleInfo* header);
  virtual ~AudioDecoder() {};

private:
  virtual GstCaps* CustomInitialize() override;
  SbMediaAudioSampleInfo Header;
  GstBuffer* AudioSpecificConfig;
};

