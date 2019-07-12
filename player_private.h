#pragma once

#include "starboard/configuration.h"
#include "starboard/player.h"
#include "starboard/thread.h"
#include "starboard/common/semaphore.h"

#include <glib.h>
#include <gst/gst.h>

//#include <atomic>
#include <memory>
#include <functional>

typedef std::function<void()> Call;
class AbstractDecoder;

template <typename T>
struct Atomic
{
  Atomic() { Value = T(); }
  T Load() const {
    starboard::ScopedLock lock(Mutex);
    return Value;
  }
  operator T() const {
    starboard::ScopedLock lock(Mutex);
    return Value;
  }
  T operator =(T value) {
    starboard::ScopedLock lock(Mutex);
    Value = value;
    return Value;
  }

private:
  T Value;
  mutable starboard::Mutex Mutex;

  Atomic(const Atomic&) = delete;
  Atomic& operator=(const Atomic&) = delete;
};

// this came from cobalt, so following requested functionality
class SbPlayerPrivate
{
public:
  static SbPlayer CreatePlayer(SbWindow window,
    SbMediaVideoCodec video_codec, SbMediaAudioCodec audio_codec,
    SbTime duration_pts, SbDrmSystem drm_system,
    const SbMediaAudioHeader* audio_header,
    SbPlayerDeallocateSampleFunc sample_deallocate_func,
    SbPlayerDecoderStatusFunc decoder_status_func,
    SbPlayerStatusFunc player_status_func,
    void* context, SbPlayerOutputMode output_mode,
    SbDecodeTargetGraphicsContextProvider* context_provider);

  virtual ~SbPlayerPrivate();

  static bool OutputModeSupported(SbPlayerOutputMode output_mode,
                                  SbMediaVideoCodec codec,
                                  SbDrmSystem drm_system);

  void Seek(SbTime seek_to_pts, int ticket);

  void WriteSample(SbMediaType sample_type,
#if SB_API_VERSION >= 6
    const void* const* sample_buffers, const int* sample_buffer_sizes,
#else   // SB_API_VERSION >= 6
    const void** sample_buffers, int* sample_buffer_sizes,
#endif  // SB_API_VERSION >= 6
    int number_of_sample_buffers, SbTime sample_pts,
    const SbMediaVideoSampleInfo* video_sample_info,
    const SbDrmSampleInfo* sample_drm_info);

  void WriteEndOfStream(SbMediaType stream_type);
  void SetBounds(int z_index, int x, int y, int width, int height);
  bool SetPlaybackRate(double playback_rate);
  void SetVolume(double volume);
  void GetInfo(SbPlayerInfo2* out_player_info);
  SbDecodeTarget GetCurrentFrame() {
    return kSbDecodeTargetInvalid;
  }
  SbMediaVideoCodec GetVideoCodec() const {
    return VideoCodec;
  }
  SbMediaAudioCodec GetAudioCodec() const {
    return AudioCodec;
  }

private:
  SbPlayerPrivate(SbWindow window,
                  SbMediaVideoCodec video_codec,
                  SbMediaAudioCodec audio_codec,
                  SbTime duration_pts,
                  SbDrmSystem drm_system,
                  const SbMediaAudioHeader* audio_header,
                  SbPlayerDeallocateSampleFunc sample_deallocate_func,
                  SbPlayerDecoderStatusFunc decoder_status_func,
                  SbPlayerStatusFunc player_status_func,
                  void* context,
                  SbPlayerOutputMode output_mode,
                  SbDecodeTargetGraphicsContextProvider* context_provider);

  // 2nd stage of constructor, as exceprions are not allowed
  bool Initialize();

  void DefaultThread();
  void WorkerThread();

  friend class AbstractDecoder;
  void SafeCall(Call call);

  void DoSeek(gint64 time, int newTicket);
  void DoPlaybackRate(double newRate);
  void DoSeekAndSpeed(gint64 seekTo, double speedTo);
  void DoBounds(int z_index, int x, int y, int width, int height);
  void DoVolume(double volume);

  static gboolean GstBusCallback(GstBus*, GstMessage* message, gpointer data);
  void GstBusCallback(GstMessage* message);

  void ReportDecoderState(SbMediaType type, SbPlayerDecoderState state);
  void ReportPlayerState(SbPlayerState newState);

  static void SourceChangedCallback(GstElement* element,
                                    GstElement* source,
                                    gpointer data);

  static gboolean UpdatePosition(gpointer data);

//  static void FirstVideoFrame(GstElement* object,
//                              guint arg0,
//                              gpointer arg1,
//                              gpointer data);

  // constant parameters
  SbWindowPrivate &Window;
  const SbMediaVideoCodec VideoCodec;
  const SbMediaAudioCodec AudioCodec;
  const SbTime DurationPts;
  const SbDrmSystem DrmSystem;
  const SbPlayerDeallocateSampleFunc SampleDeallocateFunction;
  const SbPlayerDecoderStatusFunc DecoderStatusFunction;
  const SbPlayerStatusFunc PlayerStatusFunction;
  void* const StarboardContext;
  const SbPlayerOutputMode OutputMode;
  SbDecodeTargetGraphicsContextProvider* const ContextProvider;

  // working parameters
  Atomic<SbPlayerInfo2> Info;
  SbPlayerState PlayerState;
  int LastTicket;

  // GStreamer handles
  GMainLoop* const DefaultLoop;
  GMainContext* const WorkerContext;
  GMainLoop* const WorkerLoop;
  GstElement* const Playbin;
  // referenced, but not owned
  GstElement* VideoSink;
  GstElement* AudioSink;
  GstElement* Source;

  std::unique_ptr<AbstractDecoder> Audio;
  std::unique_ptr<AbstractDecoder> Video;

  SbThreadId DefaultThreadHandle;
  SbThreadId WorkerThreadHandle;
  gint64 LastSeek;
  GSource* PositionUpdateSource;
  int LastZ;
  int LastX;
  int LastY;
  int LastWidth;
  int LastHeight;

};
