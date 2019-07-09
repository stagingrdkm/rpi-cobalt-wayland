#pragma once

#include "abstract_decoder.h"

class VideoDecoder : public AbstractDecoder
{
public:
  VideoDecoder(SbPlayerPrivate& player);
  virtual ~VideoDecoder() {}

private:
  virtual GstCaps* CustomInitialize() override;
};

