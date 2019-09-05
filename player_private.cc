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

#include "third_party/starboard/raspi/wayland/player_private.h"
#include "third_party/starboard/raspi/wayland/audio_decoder.h"
#include "third_party/starboard/raspi/wayland/video_decoder.h"
#include "third_party/starboard/raspi/wayland/cobalt_source.h"

//#include "westeros-sink.h"

#include "starboard/shared/starboard/media/media_support_internal.h"

#include "base/logging.h"

#include <cmath>
#include <map>
#include <algorithm>

GST_DEBUG_CATEGORY_STATIC (COBALT_MEDIA_BACKEND);
#define GST_CAT_DEFAULT COBALT_MEDIA_BACKEND

namespace {

SbTime GstTimeToSbTime(gint64 time)
{
  return static_cast<SbTime>(time / 1000);
}

gint64 SbTimeToGstTime(SbTime time)
{
  return static_cast<gint64>(time * 1000);
}

std::map<std::string, gint> Flags;

unsigned GetFlagValue(const std::string& name)
{
  auto i = Flags.find(name);
  if (i != Flags.end()) {
    return i->second;
  }
  static GFlagsClass* flagsClass
    = static_cast<GFlagsClass*>(g_type_class_ref(g_type_from_name("GstPlayFlags")));
  GFlagsValue* flag = g_flags_get_value_by_nick(flagsClass, name.c_str());
  if (!flag) {
    SB_DLOG(ERROR) << "no flag value for " << name;
    return 0;
  }
  gint value = flag->value;
  Flags.insert(std::map<std::string, gint>::value_type(name, value));
  return value;
}

void DoCall(void* data)
{
  std::unique_ptr<Call> call(reinterpret_cast<Call*>(data));
  (*call)();
}

// adapter for SbCreateThread
void* ThreadStarter(void* context)
{
  DoCall(context);
  return nullptr;
}


// adapter for GSourceFunc
gboolean SafeCaller(gpointer userData)
{
  DoCall(userData);
  return G_SOURCE_REMOVE;
}

//adapter for GDestroyNotify
void DestroyBufferAdapter(gpointer data)
{
  DoCall(data);
}

void DisplayGraph(GstBin* bin, const char* fileName)
{
  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(bin, GST_DEBUG_GRAPH_SHOW_ALL, fileName);
}

} // anonymous namespace

// here we do all the validation, as there might be no exceprions
SbPlayer SbPlayerPrivate::CreatePlayer(SbWindow window,
  SbMediaVideoCodec video_codec, SbMediaAudioCodec audio_codec,
  SbTime duration_pts, SbDrmSystem drm_system,
  const SbMediaAudioSampleInfo* audio_header,
  SbPlayerDeallocateSampleFunc sample_deallocate_func,
  SbPlayerDecoderStatusFunc decoder_status_func,
  SbPlayerStatusFunc player_status_func,
  void* context,
  SbPlayerOutputMode output_mode,
  SbDecodeTargetGraphicsContextProvider* context_provider)
{
  if (window == nullptr
      || !SbPlayerPrivate::OutputModeSupported(output_mode, video_codec, drm_system)
      || false) {
    return kSbPlayerInvalid;
  }
  std::unique_ptr<SbPlayerPrivate> player(
    new SbPlayerPrivate(window, video_codec, audio_codec,
      duration_pts, drm_system, audio_header, sample_deallocate_func,
      decoder_status_func, player_status_func, context, output_mode,
      context_provider));
  if (!player->Initialize()) {
    return kSbPlayerInvalid;
  }
  return player.release();
}

bool SbPlayerPrivate::OutputModeSupported(SbPlayerOutputMode outputMode,
                                          SbMediaVideoCodec codec,
                                          SbDrmSystem drmSystem)
{
 int profile = -1;
  int level = -1;
  int bit_depth = 8;
  SbMediaPrimaryId primary_id = kSbMediaPrimaryIdUnspecified;
  SbMediaTransferId transfer_id = kSbMediaTransferIdUnspecified;
  SbMediaMatrixId matrix_id = kSbMediaMatrixIdUnspecified;
  const bool result = (outputMode == kSbPlayerOutputModePunchOut)
    && SbMediaIsVideoSupported(codec, profile, level, bit_depth, primary_id, \
                               transfer_id, matrix_id,0, 0, 0, 0, 0);
  SB_UNREFERENCED_PARAMETER(outputMode);
  SB_UNREFERENCED_PARAMETER(codec);
  SB_UNREFERENCED_PARAMETER(drmSystem);
  return result;
}

SbPlayerPrivate::~SbPlayerPrivate()
{
  gst_element_set_state(Playbin, GST_STATE_NULL);
  if (PositionUpdateSource) {
    g_source_destroy(PositionUpdateSource);
    PositionUpdateSource = nullptr;
  }
  if (g_main_loop_is_running(WorkerLoop)) {
    g_main_loop_quit(WorkerLoop);
  }
  if (g_main_loop_is_running(DefaultLoop)) {
    g_main_loop_quit(DefaultLoop);
  }
  SB_DLOG(INFO) << "destroying player " << this;

  SbThreadJoin(WorkerThreadHandle, nullptr);
  WorkerThreadHandle = kSbThreadInvalid;
  SbThreadJoin(DefaultThreadHandle, nullptr);
  DefaultThreadHandle = kSbThreadInvalid;
  Audio.reset();
  Video.reset();
//  g_free(WindowPosition);
  gst_object_unref(Playbin);
  g_main_loop_unref(WorkerLoop);
  g_main_context_unref(WorkerContext);
  g_main_loop_unref(DefaultLoop);
}

void SbPlayerPrivate::Seek(SbTime seekToPts, int ticket)
{
  // convert micro seconds units to nano seconds
  gint64 time = SbTimeToGstTime(seekToPts);
  SafeCall(std::bind(&SbPlayerPrivate::DoSeek, this, time, ticket));
}

void SbPlayerPrivate::WriteSample(
  SbMediaType sample_type,
#if SB_API_VERSION >= 6
  const void* const* sample_buffers,
  const int* sample_buffer_sizes,
#else   // SB_API_VERSION >= 6
  const void** sample_buffers,
  int* sample_buffer_sizes,
#endif  // SB_API_VERSION >= 6
  int number_of_sample_buffers,
  SbTime sample_pts,
  const SbMediaVideoSampleInfo* video_sample_info,
  const SbDrmSampleInfo* sample_drm_info)
{
  if (number_of_sample_buffers <= 0) {
    return;
  }
  GDestroyNotify deallocate = nullptr;
  gpointer deallocateArgument = nullptr;
  if (SampleDeallocateFunction) {
    deallocate = DestroyBufferAdapter;
    deallocateArgument = new Call(std::bind(SampleDeallocateFunction,
                                            this,
                                            StarboardContext,
                                            sample_buffers[0]));
  }

  GstBuffer* const buffer
    = gst_buffer_new_wrapped_full(GST_MEMORY_FLAG_READONLY,
                                  const_cast<gpointer>(sample_buffers[0]),
                                  sample_buffer_sizes[0],
                                  0,
                                  sample_buffer_sizes[0],
                                  deallocateArgument,
                                  deallocate);
  for (int i = 1; i < number_of_sample_buffers; ++i) {
    GstMemory* m
      = gst_memory_new_wrapped(GST_MEMORY_FLAG_READONLY,
                               const_cast<gpointer>(sample_buffers[i]),
                               sample_buffer_sizes[i],
                               0,
                               sample_buffer_sizes[i],
                               nullptr,
                               nullptr); // tail freed when head is freed
    gst_buffer_insert_memory(buffer, i, m);
  }

  // convert micro seconds units to nano seconds
  GST_BUFFER_PTS(buffer) = SbTimeToGstTime(sample_pts);

  AbstractDecoder* decoder = nullptr;
  switch (sample_type) {
  case kSbMediaTypeVideo:
    decoder = Video.get();
    break;
  case kSbMediaTypeAudio:
    decoder = Audio.get();
    break;
  default:
    SB_DLOG(ERROR) << "strange buffer type " << sample_type;
    gst_buffer_unref(buffer);
    return;
  }

  decoder->PushWorker(buffer);
//  SafeCall(std::bind(&AbstractDecoder::PushWorker, decoder, buffer));
}

void SbPlayerPrivate::WriteEndOfStream(SbMediaType streamType)
{
  AbstractDecoder* decoder;
  switch (streamType) {
  case kSbMediaTypeVideo:
    decoder = Video.get();
    break;
  case kSbMediaTypeAudio:
    decoder = Audio.get();
    break;
  default:
    SB_DLOG(ERROR) << "strange stream type " << streamType;
    return;
  }
  decoder->EosWorker();
}

void SbPlayerPrivate::SetBounds(int z_index,
                                int x, int y, int width, int height)
{
  // z_index seems to be just a counter, so ignore it
  SB_UNREFERENCED_PARAMETER(z_index);
  SafeCall(std::bind(&SbPlayerPrivate::DoBounds, this,
                     0, x, y, width, height));
}

bool SbPlayerPrivate::SetPlaybackRate(double newPlaybackRate)
{
  if (newPlaybackRate >= 0.0) {
    SafeCall(std::bind(&SbPlayerPrivate::DoPlaybackRate, this, newPlaybackRate));
    return true;
  }
  return false;
}

void SbPlayerPrivate::SetVolume(double volume)
{
  if (volume >= 0.0 && volume <= 1.0) {
    SafeCall(std::bind(&SbPlayerPrivate::DoVolume, this, volume));
  }
}

void SbPlayerPrivate::GetInfo(SbPlayerInfo2* outPlayerInfo)
{
  *outPlayerInfo = Info;
}

SbPlayerPrivate::SbPlayerPrivate(SbWindow window,
  SbMediaVideoCodec video_codec,
  SbMediaAudioCodec audio_codec,
  SbTime duration_pts,
  SbDrmSystem drm_system,
  const SbMediaAudioSampleInfo* audio_header,
  SbPlayerDeallocateSampleFunc sample_deallocate_func,
  SbPlayerDecoderStatusFunc decoder_status_func,
  SbPlayerStatusFunc player_status_func,
  void* context,
  SbPlayerOutputMode output_mode,
  SbDecodeTargetGraphicsContextProvider* context_provider)
  : Window(*window),
  VideoCodec(video_codec),
  AudioCodec(audio_codec),
  DurationPts(duration_pts),
  DrmSystem(drm_system),
  SampleDeallocateFunction(sample_deallocate_func),
  DecoderStatusFunction(decoder_status_func),
  PlayerStatusFunction(player_status_func),
  StarboardContext(context),
  OutputMode(output_mode),
  ContextProvider(context_provider),
  Info(),
  PlayerState(kSbPlayerStateDestroyed), // error to be different from initial
  LastTicket(SB_PLAYER_INITIAL_TICKET),
  DefaultLoop(g_main_loop_new(nullptr, true)),
  WorkerContext(g_main_context_new()),
  WorkerLoop(g_main_loop_new(WorkerContext, TRUE)),
  Playbin(gst_element_factory_make("playbin", nullptr)),
  VideoSink(nullptr),
  AudioSink(nullptr),
  Source(nullptr),
  Audio(new AudioDecoder(*this, audio_header)),
  Video(new VideoDecoder(*this)),
  DefaultThreadHandle(kSbThreadInvalid),
  WorkerThreadHandle(kSbThreadInvalid),
  LastSeek(0),
//  AudioReady(false),
//  VideoReady(false)
//  WindowPosition(nullptr)
  PositionUpdateSource(nullptr),
  LastZ(-1),
  LastX(-1),
  LastY(-1),
  LastWidth(-1),
  LastHeight(-1)
{
  if (!SampleDeallocateFunction) {
    SB_DLOG(INFO) << "no sample deallocation function provided";
  }
  if (!DecoderStatusFunction) {
    SB_DLOG(INFO) << "no decoder status function provided";
  }
  if (!PlayerStatusFunction) {
    SB_DLOG(INFO) << "no player status function provided";
  }
  SB_DCHECK(WorkerContext);
  SB_DCHECK(WorkerLoop);
  SB_DCHECK(Playbin);
  SbPlayerInfo2 i = SbPlayerInfo2();
  i.is_paused = true;
  i.volume = 1.0;
  i.playback_rate = 1.0;
  Info = i;
}

bool SbPlayerPrivate::Initialize()
{
  SB_DLOG(INFO) << "Inside SbPlayerPrivate::Initialize()";

  // Enable GST_DEBUG logging
  GST_DEBUG_CATEGORY_INIT(COBALT_MEDIA_BACKEND, "COBALT_MEDIA_BACKEND", 0,
                          "Cobalt Gstreamer Media Playbin Backend");

  //Build pipeline
  g_signal_connect(Playbin, "source-setup",
                   G_CALLBACK(SbPlayerPrivate::SourceChangedCallback), this);
  g_object_set(Playbin, "uri", "cobalt://", nullptr);
  const gint flags = GetFlagValue("video")
    | GetFlagValue("audio")
    | GetFlagValue("native-video")
    | GetFlagValue("native-audio")
    | GetFlagValue("buffering");
  g_object_set(Playbin, "flags", flags, nullptr);

  //video sink
  VideoSink = gst_element_factory_make("westerossink", nullptr);
  g_object_set(Playbin, "video-sink", VideoSink, nullptr);
  g_object_set(G_OBJECT(VideoSink), "zorder", 0.0f, nullptr);

  //audio sink
  AudioSink = gst_element_factory_make("omxhdmiaudiosink", nullptr);
  g_object_set(Playbin, "audio-sink", AudioSink, nullptr);
  g_object_set(G_OBJECT(AudioSink), "async", true, nullptr);

  gst_element_set_state(Playbin, GST_STATE_PAUSED);

  //Initialize A/V stream players
  if (!Video->Initialize() || !Audio->Initialize()) {
    return false;
  }

  starboard::Semaphore starter;
  DefaultThreadHandle
    = SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "default", &ThreadStarter,
                     new Call(std::bind(&SbPlayerPrivate::DefaultThread,
                                        this)));
  g_main_context_invoke(nullptr,
    &SafeCaller,
    reinterpret_cast<gpointer>(new Call(std::bind(&starboard::Semaphore::Put,
                                                  &starter))));
  starter.Take();
  SB_DLOG(INFO) << "default loop started";

  WorkerThreadHandle
    = SbThreadCreate(0, kSbThreadPriorityRealTime, kSbThreadNoAffinity, true,
                     "player", &ThreadStarter,
                     new Call(std::bind(&SbPlayerPrivate::WorkerThread,
                                        this)));
  SafeCall(std::bind(&starboard::Semaphore::Put, &starter));
  // wait till loop really started
  starter.Take();
  SB_DLOG(INFO) << "worker loop created " << this;
  SafeCall(std::bind(&SbPlayerPrivate::ReportPlayerState, this,
                     kSbPlayerStateInitialized));
  return true;
}

void SbPlayerPrivate::DefaultThread()
{
  SB_DLOG(INFO) << "default loop started";
  g_main_context_push_thread_default(nullptr);
  g_main_loop_run(DefaultLoop);
  SB_DLOG(INFO) << "default loop finished";
}

void SbPlayerPrivate::WorkerThread()
{
  SB_DLOG(INFO) << "worker loop started";

  g_main_context_push_thread_default(WorkerContext);

  GstBus* bus = gst_element_get_bus(Playbin);
  gst_bus_add_watch(bus, SbPlayerPrivate::GstBusCallback, this);

  g_main_loop_run(WorkerLoop);

  gst_bus_remove_watch(bus);
  gst_object_unref(bus);
  ReportPlayerState(kSbPlayerStateDestroyed);
  // g_main_context_release(WorkerContext);
  SB_DLOG(INFO) << "worker loop finished";
}

void SbPlayerPrivate::SafeCall(Call call)
{
  g_main_context_invoke(WorkerContext, &SafeCaller,
                        reinterpret_cast<gpointer>(new Call(call)));
}

void SbPlayerPrivate::DoSeek(gint64 time, int newTicket)
{
  LastTicket = newTicket;
  const double playbackRate = Info.Load().playback_rate;
  // if paused just store position for playback
  if (playbackRate >= 1e-6 && PositionUpdateSource) {
    DoSeekAndSpeed(time, playbackRate);
    return;
  }
  // seek can't be done while paused. just record for the next play command
  LastSeek = time;
}

void SbPlayerPrivate::DoPlaybackRate(double newRate)
{
  if (PositionUpdateSource /*AudioReady && VideoReady*/) {
    if (newRate >= 1e-6) {
      DoSeekAndSpeed(LastSeek, newRate);
      return;
    }
    gst_element_set_state(Playbin, GST_STATE_PAUSED);
    // fallthrough to record state
  }
  SbPlayerInfo2 i = Info;
  if (newRate >= 1e-6) {
    i.playback_rate = newRate;
    i.is_paused = false;
  }
  else {
    i.is_paused = true;
  }
  Info = i;
}

void SbPlayerPrivate::DoSeekAndSpeed(gint64 seekTo, double speedTo)
{
  // here we sure that speedTo is not 0
  SB_DCHECK(speedTo >= 1e-6);

  SbPlayerInfo2 i = Info;
  gst_element_set_state(Playbin, GST_STATE_PLAYING);

  if (fabs(speedTo - i.playback_rate) < 1e-6
      && seekTo == 0 && i.current_media_timestamp == 0) {
    // special case - avoid explicit seek at the beginning of playback
    SB_DLOG(INFO) << "skipped seek at the start of playback";
  }
  else if (seekTo < 0 && fabs(speedTo - i.playback_rate) < 1e-6) {
    // special case - no position known, and no change of speed
    SB_DLOG(INFO) << "skipped seek as no speed and no position change";
  }
  else {
    // newRate is always positive!
    gint64 position = seekTo;
    const bool isJump = position > 0
      || (position == 0 && i.current_media_timestamp != 0);
    if (position >= 0) {
      // seek position is well known
      SB_DLOG(INFO) << "jumpimg to known time " << (position * 1e-9);
    }
    else if (gst_element_query_position(Playbin, GST_FORMAT_TIME, &position)) {
      SB_DLOG(INFO) << "successfully queried position " << (position * 1e-9);
    }
    else {
      position = SbTimeToGstTime(i.current_media_timestamp);
      SB_DLOG(INFO) << "failed to query position. using last known "
        << (position * 1e-9);
    }

    if (isJump) {
      ReportPlayerState(kSbPlayerStatePrerolling);
    }
    gst_element_seek(Playbin,
                     speedTo,
                     GST_FORMAT_TIME,
                     isJump ? GST_SEEK_FLAG_FLUSH : GST_SEEK_FLAG_NONE,
                     isJump ? GST_SEEK_TYPE_SET : GST_SEEK_TYPE_NONE,
                     isJump ? position : GST_CLOCK_TIME_NONE,
                     GST_SEEK_TYPE_NONE,
                     GST_CLOCK_TIME_NONE);
  }

  // update global data
  LastSeek = -1;
  i.playback_rate = speedTo;
  i.is_paused = false;
  Info = i;
}

void SbPlayerPrivate::DoBounds(
  int z_index, int x, int y, int width, int height)
{
  // z_index from 0 till infinity?
  // z value from 0.0 to 1.0, internally only 100 layers? (0.00, 0.01 etc)
  // so converting 0->0.0, 1->1.5/100.0, infinity -> 1.0 using atan
  if (z_index != LastZ) {
    const float realZ
      = atanf(static_cast<float>(z_index) * M_PI / 133.0f) * 2.0f / M_PI;
    g_object_set(G_OBJECT(VideoSink), "zorder", realZ, nullptr);
    LastZ = z_index;
  }

  if (x == LastX && y == LastY && width == LastWidth && height == LastHeight) {
    return;
  }
  gchar* position = g_strdup_printf("%d,%d,%d,%d", x, y, width, height);
  g_object_set(G_OBJECT(VideoSink), "window_set", position, nullptr);
  g_free(position);
  LastX = x;
  LastY = y;
  LastWidth = width;
  LastHeight = height;
}

void SbPlayerPrivate::DoVolume(double volume)
{
  SbPlayerInfo2 i = Info;
// just tracking
//  g_object_set(AudioSink, "volume", volume, "mute", FALSE, nullptr);
  i.volume = volume;
  Info = i;
}
gboolean SbPlayerPrivate::GstBusCallback(
  GstBus*, GstMessage* message, gpointer data)
{
  reinterpret_cast<SbPlayerPrivate*>(data)->GstBusCallback(message);
  return TRUE;
}

void SbPlayerPrivate::GstBusCallback(GstMessage* message)
{
  GError* error;
  gchar* debug;
  switch (GST_MESSAGE_TYPE(message))
  {
  case GST_MESSAGE_ASYNC_DONE:
    SB_DLOG(INFO) << "element " << GST_MESSAGE_SRC_NAME(message)
      << " GST_MESSAGE_ASYNC_DONE!";
    break;

  case GST_MESSAGE_ERROR:
    gst_message_parse_error(message, &error, &debug);
    g_error_free(error);
    g_free(debug);
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(
      GST_BIN(Playbin), GST_DEBUG_GRAPH_SHOW_ALL, "error-pipeline");
    SB_DLOG(ERROR) << "error reported in callback";
    break;

  case GST_MESSAGE_WARNING:
    gst_message_parse_warning(message, &error, &debug);
    g_error_free(error);
    g_free(debug);
    SB_DLOG(ERROR) << "warning reported in callback";
    break;

  case GST_MESSAGE_EOS:
    GST_INFO("EOS reached pipeline");
    ReportPlayerState(kSbPlayerStateEndOfStream);
    SB_DLOG(INFO) << "EOS reached pipeline";
    break;

  case GST_MESSAGE_STATE_CHANGED:
    {
      // prerolling ->
      GstState oldState, newState, pending;
      gst_message_parse_state_changed(message, &oldState, &newState,
                                      &pending);

      SB_DLOG(INFO) << "element " << GST_MESSAGE_SRC_NAME(message)
        << " state change " << gst_element_state_get_name(oldState)
        << " -> " << gst_element_state_get_name(newState);

      GstObject* m = GST_MESSAGE_SRC(message);
      if (m == GST_OBJECT(Playbin)) {
        if (newState == GST_STATE_PAUSED && !PositionUpdateSource) {
          PositionUpdateSource = g_timeout_source_new(16);
          g_source_attach(PositionUpdateSource, WorkerContext);
          g_source_set_callback(PositionUpdateSource,
                                &SbPlayerPrivate::UpdatePosition,
                                this, nullptr);
          ReportPlayerState(kSbPlayerStatePresenting);
          SbPlayerInfo2 i = Info;
          if (!i.is_paused) {
            DoSeekAndSpeed(LastSeek, i.playback_rate);
          }
          DoVolume(i.volume);
        }
        else if (newState == GST_STATE_PLAYING) {
          ReportPlayerState(kSbPlayerStatePresenting);
        }
      }
      else if (m == GST_OBJECT(Source)) {
        if ((newState == GST_STATE_READY)
            && Source && !CobaltSourceIsConfigured(Source)) {
          CobaltSourceRegisterPlayer(GST_ELEMENT(Source),
                                     Audio.get(), Video.get());
          ReportPlayerState(kSbPlayerStatePrerolling);
        }
      }
      break;
    }

  default:
    break;
  }
}

void SbPlayerPrivate::ReportDecoderState(SbMediaType type,
                                         SbPlayerDecoderState state)
{
  if (LastTicket != SB_PLAYER_INITIAL_TICKET && DecoderStatusFunction) {
    DecoderStatusFunction(this, StarboardContext, type, state, LastTicket);
  }
}

void SbPlayerPrivate::ReportPlayerState(SbPlayerState newState)
{
  if (newState != PlayerState) {
    PlayerState = newState;
#if 0
    std::string name;
    switch (PlayerState) {
#define STATE_STRING(s) case s: name = #s; break;
    STATE_STRING(kSbPlayerStateInitialized);
    STATE_STRING(kSbPlayerStatePrerolling);
    STATE_STRING(kSbPlayerStatePresenting);
    STATE_STRING(kSbPlayerStateEndOfStream);
    STATE_STRING(kSbPlayerStateDestroyed);
    STATE_STRING(kSbPlayerStateError);
#undef STATE_STRING
    default:
      name = std::string("IMPOSSIBLE ")
        + std::to_string(static_cast<int>(PlayerState));
      break;
    }
    SB_DLOG(INFO) << __func__ << " new state is " << name;
#endif
  }
  if (PlayerStatusFunction) {
    PlayerStatusFunction(this, StarboardContext, newState, LastTicket);
  }
}

// static
void SbPlayerPrivate::SourceChangedCallback(GstElement* element,
                                            GstElement* source,
                                            gpointer data)
{
  SbPlayerPrivate& p = *reinterpret_cast<SbPlayerPrivate*>(data);
  g_object_get(p.Playbin, "source", &p.Source, nullptr);
}

// static
gboolean SbPlayerPrivate::UpdatePosition(gpointer data)
{
  SbPlayerPrivate& p = *reinterpret_cast<SbPlayerPrivate*>(data);
  if (!p.VideoSink) {
    return G_SOURCE_CONTINUE;
  }

  gint64 pos = 0;
  gst_element_query_position(p.Playbin, GST_FORMAT_TIME, &pos);

  SbPlayerInfo2 i = p.Info;
  i.current_media_timestamp = GstTimeToSbTime(pos);
  p.Info = i;

#if 0
  static bool reported = false;
  if (pos > 5000000000ll && !reported) {
    DisplayGraph(GST_BIN(p.Playbin), "after5sec");
    reported = true;
  }
#endif

  return G_SOURCE_CONTINUE;
}

