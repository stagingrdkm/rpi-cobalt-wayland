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

