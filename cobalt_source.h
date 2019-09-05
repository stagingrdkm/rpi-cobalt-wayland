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

#include <gst/gst.h>
#include "third_party/starboard/raspi/wayland/abstract_decoder.h"

G_BEGIN_DECLS

#define GST_COBALT_TYPE_SRC            (gst_cobalt_src_get_type ())
#define GST_COBALT_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_COBALT_TYPE_SRC, GstCobaltSrc))
#define GST_COBALT_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_COBALT_TYPE_SRC, GstCobaltSrcClass))
#define GST_IS_COBALT_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_COBALT_TYPE_SRC))
#define GST_IS_COBALT_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_COBALT_TYPE_SRC))

typedef struct _GstCobaltSrc GstCobaltSrc;
typedef struct _GstCobaltSrcClass GstCobaltSrcClass;
typedef struct _GstCobaltSrcPrivate GstCobaltSrcPrivate;

struct _GstCobaltSrc
{
  GstBin parent;
  GstCobaltSrcPrivate *priv;
};

struct _GstCobaltSrcClass
{
  GstBinClass parentClass;
};

GType gst_cobalt_src_get_type(void);

void CobaltSourceRegisterPlayer(GstElement*,
                                AbstractDecoder*, AbstractDecoder*);
void CobaltSourceUnregisterPlayer(GstElement*,
                                  AbstractDecoder*, AbstractDecoder*);
gboolean CobaltSourceIsConfigured(GstElement*);
G_END_DECLS

