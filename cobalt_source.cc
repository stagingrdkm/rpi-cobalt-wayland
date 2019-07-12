#include "third_party/starboard/raspi/wayland/cobalt_source.h"
#include "third_party/starboard/raspi/wayland/abstract_decoder.h"
#include "starboard/log.h"

#include <gst/app/gstappsrc.h>

#define GST_COBALT_SRC_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE((obj), GST_COBALT_TYPE_SRC, GstCobaltSrcPrivate))

struct _GstCobaltSrcPrivate
{
  gchar* uri;
  guint pad_counter;
  gboolean configured;
};

enum
{
  PROP_0, PROP_LOCATION
};

static GstStaticPadTemplate SourceTemplate
  = GST_STATIC_PAD_TEMPLATE("src_%u", GST_PAD_SRC, GST_PAD_SOMETIMES,
                            GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_cobalt_src_debug);
#define GST_CAT_DEFAULT gst_cobalt_src_debug

static void UriHandlerInit(gpointer gIface, gpointer ifaceData);

static void CobaltSourceDispose(GObject*);
static void CobaltSourceFinalize(GObject*);
static void CobaltSourceSetProperty(
  GObject*, guint propertyID, const GValue*, GParamSpec*);
static void CobaltSourceGetProperty(
  GObject*, guint propertyID, GValue*, GParamSpec*);
static GstStateChangeReturn CobaltSourceStateChange(GstElement*, GstStateChange);
static void CobaltSourceHandleMessage(GstBin*, GstMessage*);
static gboolean CobaltSourceQueryWithParent(GstPad*, GstObject*, GstQuery*);

#define gst_cobalt_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE(GstCobaltSrc, gst_cobalt_src, GST_TYPE_BIN,
  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, UriHandlerInit);
  GST_DEBUG_CATEGORY_INIT(gst_cobalt_src_debug, "cobaltsrc", 0,
                          "cobalt source element"););


static void gst_cobalt_src_class_init(GstCobaltSrcClass* klass)
{
  GObjectClass* oklass = G_OBJECT_CLASS(klass);
  GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
  GstBinClass* bklass = GST_BIN_CLASS(klass);

  oklass->dispose = CobaltSourceDispose;
  oklass->finalize = CobaltSourceFinalize;
  oklass->set_property = CobaltSourceSetProperty;
  oklass->get_property = CobaltSourceGetProperty;

  gst_element_class_add_pad_template(
    eklass, gst_static_pad_template_get(&SourceTemplate));
  gst_element_class_set_metadata(eklass, "Cobalt source element", "Source",
    "cobaltSrc", "Handles data incoming from the Cobalt");

  /* Allows setting the uri using the 'location' property, which is used
   * for example by gst_element_make_from_uri() */
  g_object_class_install_property(oklass, PROP_LOCATION,
    g_param_spec_string("location", "location", "Location to read from", 0,
      (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  eklass->change_state = GST_DEBUG_FUNCPTR(CobaltSourceStateChange);
  bklass->handle_message = GST_DEBUG_FUNCPTR(CobaltSourceHandleMessage);
  g_type_class_add_private(klass, sizeof(GstCobaltSrcPrivate));
}

static void gst_cobalt_src_init(GstCobaltSrc* src)
{
  GstCobaltSrcPrivate* priv = GST_COBALT_SRC_GET_PRIVATE(src);
  src->priv = priv;
  src->priv->configured = FALSE;
  src->priv->pad_counter = 0;
  src->priv->uri = nullptr;
  g_object_set(GST_BIN(src), "message-forward", TRUE, nullptr);
}

static void CobaltSourceDispose(GObject* object)
{
  GST_CALL_PARENT(G_OBJECT_CLASS, dispose, (object));
}

static void CobaltSourceFinalize(GObject* object)
{
  GstCobaltSrc* src = GST_COBALT_SRC(object);
  GstCobaltSrcPrivate* priv = src->priv;
  g_free(priv->uri);
  priv->uri = nullptr;
  GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static void CobaltSourceSetProperty(GObject* object, guint propID,
        const GValue* value, GParamSpec* pspec)
{
  GstCobaltSrc* src = GST_COBALT_SRC(object);

  switch (propID)
  {
  case PROP_LOCATION:
    gst_uri_handler_set_uri(reinterpret_cast<GstURIHandler*>(src),
                            g_value_get_string(value), 0);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
    break;
  }
}

static void CobaltSourceGetProperty(GObject* object, guint propID,
        GValue* value, GParamSpec* pspec)
{
    GstCobaltSrc* src = GST_COBALT_SRC(object);
    GstCobaltSrcPrivate* priv = src->priv;

    GST_OBJECT_LOCK(src);
    switch (propID)
    {
    case PROP_LOCATION:
        g_value_set_string(value, priv->uri);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
    GST_OBJECT_UNLOCK(src);
}

static GstStateChangeReturn CobaltSourceStateChange(
  GstElement* element, GstStateChange transition)
{
  return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

// uri handler interface
static GstURIType UriGetType(GType)
{
  return GST_URI_SRC;
}

static const gchar* const * UriGetProtocols(GType)
{
  static const gchar* const protocols[] = { "cobalt", 0 };
  return protocols;
}

static gchar* UriGetUri(GstURIHandler* handler)
{
  GstCobaltSrc* src = GST_COBALT_SRC(handler);
  gchar* ret;

  GST_OBJECT_LOCK(src);
  ret = g_strdup(src->priv->uri);
  GST_OBJECT_UNLOCK(src);
  return ret;
}

static gboolean UriSetUri(GstURIHandler* handler, const gchar* uri,
        GError** error)
{
    GstCobaltSrc* src = GST_COBALT_SRC(handler);
    GstCobaltSrcPrivate* priv = src->priv;

    if (GST_STATE(src) >= GST_STATE_PAUSED)
    {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    GST_OBJECT_LOCK(src);

    g_free(priv->uri);
    priv->uri = 0;

    if (!uri)
    {
        GST_OBJECT_UNLOCK(src);
        return TRUE;
    }

    priv->uri = g_strdup(uri);
    GST_OBJECT_UNLOCK(src);
    return TRUE;
}

static void UriHandlerInit(gpointer interface, gpointer)
{
  GstURIHandlerInterface& i
    = *reinterpret_cast<GstURIHandlerInterface *>(interface);

  i.get_type = UriGetType;
  i.get_protocols = UriGetProtocols;
  i.get_uri = UriGetUri;
  i.set_uri = UriSetUri;
}

static gboolean CobaltSourceQueryWithParent(
  GstPad* pad, GstObject* parent, GstQuery* query)
{
  gboolean result = FALSE;

  GstPad* target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad));
  // Forward the query to the proxy target pad.
  if (target) {
    result = gst_pad_query(target, query);
  }
  gst_object_unref(target);
  return result;
}

static void CobaltSourceHandleMessage(GstBin* bin, GstMessage* message)
{
  GstCobaltSrc* src = GST_COBALT_SRC(GST_ELEMENT(bin));

  switch (GST_MESSAGE_TYPE(message))
  {
  case GST_MESSAGE_EOS:
    {
      gboolean emit_eos = TRUE;
      GstPad* pad = gst_element_get_static_pad(
        GST_ELEMENT(GST_MESSAGE_SRC(message)), "src");

      GST_DEBUG_OBJECT(src, "EOS received from %s",
                       GST_MESSAGE_SRC_NAME(message));
      g_object_set_data(G_OBJECT(pad), "is-eos", GINT_TO_POINTER(1));
      gst_object_unref(pad);
      for (guint i = 0; i < src->priv->pad_counter; ++i) {
        gchar* name = g_strdup_printf("src_%u", i);
        GstPad* src_pad = gst_element_get_static_pad(GST_ELEMENT(src), name);
        GstPad* target = gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(src_pad));
        gint is_eos
          = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(target), "is-eos"));
        gst_object_unref(target);
        gst_object_unref(src_pad);
        g_free(name);

        if (!is_eos)
        {
          emit_eos = FALSE;
          break;
        }
      }
      gst_message_unref(message);
      if (emit_eos) {
        GST_DEBUG_OBJECT(src,
                         "All appsrc elements are EOS, emitting event now.");
        gst_element_send_event(GST_ELEMENT(bin), gst_event_new_eos());
      }
      break;
    }
  default:
    GST_BIN_CLASS(parent_class)->handle_message(bin, message);
    break;
  }
}

// visible interfaces
void CobaltSourceRegisterPlayer(GstElement* element,
                                AbstractDecoder* audioDecoder,
                                AbstractDecoder* videoDecoder)
{
  GstCobaltSrc* src = GST_COBALT_SRC(element);
  AbstractDecoder* const decoders[] = { audioDecoder, videoDecoder };

  for (AbstractDecoder* d : decoders) {
    //register audio buffer player
    gchar* name = g_strdup_printf("src_%u", src->priv->pad_counter);
    ++(src->priv->pad_counter);

    SB_DLOG(INFO) << "Registering player " << reinterpret_cast<void*>(d)
      << " on pad " << name;

    GstElement* appsrc = d->GetElement();
    gst_bin_add(GST_BIN(element), appsrc);
    GstPad* target = gst_element_get_static_pad(appsrc, "src");
    GstPad* pad = gst_ghost_pad_new(name, target);

    gst_pad_set_query_function(pad, CobaltSourceQueryWithParent);
    gst_pad_set_active(pad, TRUE);

    gst_element_add_pad(element, pad);
    GST_OBJECT_FLAG_SET(pad, GST_PAD_FLAG_NEED_PARENT);

    gst_element_sync_state_with_parent(appsrc);

    g_free(name);
    gst_object_unref(target);
  }
  gst_element_no_more_pads(element);
  src->priv->configured = TRUE;
}

void CobaltSourceUnregisterPlayer(GstElement* element,
                                  AbstractDecoder* audioDecoder,
                                  AbstractDecoder* videoDecoder)
{
  GstCobaltSrc* src = GST_COBALT_SRC(element);
  AbstractDecoder* const decoders[] = { audioDecoder, videoDecoder };

  for (AbstractDecoder* d : decoders) {
    if (!d) {
      continue;
    }
    //unregister audio buffer player
    GstElement* appsrc = d->GetElement();
    GstPad* pad = gst_element_get_static_pad(appsrc, "src");
    GstPad* peer = gst_pad_get_peer(pad);

    SB_DLOG(INFO) << "Unregistering audio_player from pad "
      << GST_PAD_NAME(pad) << ", appsrc: "
      << reinterpret_cast<void*>(appsrc);

    if (!peer) {
      continue;
    }

    GstPad* ghost
      = GST_PAD_CAST(gst_proxy_pad_get_internal(GST_PROXY_PAD(peer)));

    if (!ghost) {
      continue;
    }
    gst_ghost_pad_set_target(GST_GHOST_PAD_CAST(ghost), nullptr);
    gst_element_remove_pad(element, ghost);
    gst_object_unref(ghost);
    gst_object_unref(peer);

    gst_element_set_state(appsrc, GST_STATE_NULL);
    gst_bin_remove(GST_BIN(src), appsrc);

    if (src->priv->pad_counter > 0) {
      --(src->priv->pad_counter);
    }
  }

  if (GST_BIN_NUMCHILDREN(src) == 0) {
    GST_DEBUG_OBJECT(src, "No player left, unconfiguring");
    src->priv->configured = FALSE;
  }
}

gboolean CobaltSourceIsConfigured(GstElement* element)
{
  GstCobaltSrc* src = GST_COBALT_SRC(element);
  return src->priv->configured;
}
