//
// If not stated otherwise in this file or this component's LICENSE file the
// following copyright and licenses apply:
//
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

#include "starboard/player.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"

#include "third_party/starboard/raspi/wayland/audio_decoder.h"
#include "third_party/starboard/raspi/wayland/video_decoder.h"

// Creates a player that will be displayed on |window| for the specified
// |video_codec| and |audio_codec|, acquiring all resources needed to operate
// it, and returning an opaque handle to it. The expectation is that a new
// player will be created and destroyed for every playback.
//
// This function returns the created player. Note the following:
// - The associated decoder of the returned player should be assumed to not be
//   in |kSbPlayerDecoderStateNeedsData| until SbPlayerSeek() has been called
//   on it.
// - It is expected either that the thread that calls SbPlayerCreate is the same
//   thread that calls the other |SbPlayer| functions for that player, or that
//   there is a mutex guarding calls into each |SbPlayer| instance.
// - If there is a platform limitation on how many players can coexist
//   simultaneously, then calls made to this function that attempt to exceed
//   that limit will return |kSbPlayerInvalid|.
//
// |window|: The window that will display the player. |window| can be
//   |kSbWindowInvalid| for platforms where video is only displayed on a
//   particular window that the underlying implementation already has access to.
//
// |video_codec|: The video codec used for the player. If |video_codec| is
//   |kSbMediaVideoCodecNone|, the player is an audio-only player. If
//   |video_codec| is any other value, the player is an audio/video decoder.
//
// |audio_codec|: The audio codec used for the player. The value should never
//   be |kSbMediaAudioCodecNone|. In addition, the caller must provide a
//   populated |audio_header| if the audio codec is |kSbMediaAudioCodecAac|.
//
// |duration_pts|: The expected media duration in 90KHz ticks (PTS). It may be
//   set to |SB_PLAYER_NO_DURATION| for live streams.
//
// |drm_system|: If the media stream has encrypted portions, then this
//   parameter provides an appropriate DRM system, created with
//   |SbDrmCreateSystem()|. If the stream does not have encrypted portions,
//   then |drm_system| may be |kSbDrmSystemInvalid|.
//
// |audio_header|: Note that the caller must provide a populated |audio_header|
//   if the audio codec is |kSbMediaAudioCodecAac|. Otherwise, |audio_header|
//   can be NULL. See media.h for the format of the |SbMediaAudioHeader| struct.
#if SB_API_VERSION >= 6
//   Note that |audio_specific_config| is a pointer and the content it points to
//   is no longer valid after this function returns.  The implementation has to
//   make a copy of the content if it is needed after the function returns.
#endif  // SB_API_VERSION >= 6
//
// |sample_deallocator_func|: If not |NULL|, the player calls this function
//   on an internal thread to free the sample buffers passed into
//   SbPlayerWriteSample().
//
// |decoder_status_func|: If not |NULL|, the decoder calls this function on an
//   internal thread to provide an update on the decoder's status. No work
//   should be done on this thread. Rather, it should just signal the client
//   thread interacting with the decoder.
//
// |player_status_func|: If not |NULL|, the player calls this function on an
//   internal thread to provide an update on the playback status. No work
//   should be done on this thread. Rather, it should just signal the client
//   thread interacting with the decoder.
//
// |context|: This is passed to all callbacks and is generally used to point
//   at a class or struct that contains state associated with the player.
//
// |output_mode|: Selects how the decoded video frames will be output.  For
//   example, kSbPlayerOutputModePunchOut indicates that the decoded video
//   frames will be output to a background video layer by the platform, and
//   kSbPlayerOutputDecodeToTexture indicates that the decoded video frames
//   should be made available for the application to pull via calls to
//   SbPlayerGetCurrentFrame().
//
// |provider|: Only present in Starboard version 3 and up.  If not |NULL|,
//   then when output_mode == kSbPlayerOutputModeDecodeToTexture, the player MAY
//   use the provider to create SbDecodeTargets on the renderer thread. A
//   provider may not always be needed by the player, but if it is needed, and
//   the provider is not given, the player will fail by returning
//   kSbPlayerInvalid.
SB_EXPORT SbPlayer
SbPlayerCreate(SbWindow window,
               SbMediaVideoCodec videoCodec,
               SbMediaAudioCodec audioCodec,
               SbDrmSystem drmSystem,
               const SbMediaAudioHeader* audioHeader,
               SbPlayerDeallocateSampleFunc sampleDeallocateFunc,
               SbPlayerDecoderStatusFunc decoderStatusFunc,
               SbPlayerStatusFunc playerStatusFunc,
               SbPlayerErrorFunc player_error_func,
               void* context,
               SbPlayerOutputMode outputMode,
               SbDecodeTargetGraphicsContextProvider* contextProvider)
{
  return SbPlayerPrivate::CreatePlayer(window, videoCodec, audioCodec,
    0, drmSystem, audioHeader, sampleDeallocateFunc,
    decoderStatusFunc, playerStatusFunc, context, outputMode,
    contextProvider);
}

// Returns true if the given player output mode is supported by the platform.
// If this function returns true, it is okay to call SbPlayerCreate() with
// the given |output_mode|.
SB_EXPORT bool SbPlayerOutputModeSupported(SbPlayerOutputMode output_mode,
                                           SbMediaVideoCodec codec,
                                           SbDrmSystem drm_system)
{
  return SbPlayerPrivate::OutputModeSupported(output_mode, codec, drm_system);
}

// Destroys |player|, freeing all associated resources. Each callback must
// receive one more callback to say that the player was destroyed. Callbacks
// may be in-flight when SbPlayerDestroy is called, and should be ignored once
// this function is called.
//
// It is not allowed to pass |player| into any other |SbPlayer| function once
// SbPlayerDestroy has been called on that player.
//
// |player|: The player to be destroyed.
SB_EXPORT void SbPlayerDestroy(SbPlayer player)
{
  delete player;
}

// Tells the player to freeze playback (if playback has already started),
// reset or flush the decoder pipeline, and go back to the Prerolling state.
// The player should restart playback once it can display the frame at
// |seek_to_pts|, or the closest it can get. (Some players can only seek to
// I-Frames, for example.)
//
// - Seek must be called before samples are sent when starting playback for
//   the first time, or the client never receives the
//   |kSbPlayerDecoderStateNeedsData| signal.
// - A call to seek may interrupt another seek.
// - After this function is called, the client should not send any more audio
//   or video samples until |SbPlayerDecoderStatusFunc| is called back with
//   |kSbPlayerDecoderStateNeedsData| for each required media type.
//   |SbPlayerDecoderStatusFunc| is the |decoder_status_func| callback function
//   that was specified when the player was created (SbPlayerCreate).
//
// |player|: The SbPlayer in which the seek operation is being performed.
// |seek_to_pts|: The frame at which playback should begin.
// |ticket|: A user-supplied unique ID that is be passed to all subsequent
//   |SbPlayerDecoderStatusFunc| calls. (That is the |decoder_status_func|
//   callback function specified when calling SbPlayerCreate.)
//
//   The |ticket| value is used to filter calls that may have been in flight
//   when SbPlayerSeek was called. To be very specific, once SbPlayerSeek has
//   been called with ticket X, a client should ignore all
//   |SbPlayerDecoderStatusFunc| calls that do not pass in ticket X.
SB_EXPORT void SbPlayerSeek(SbPlayer player,
                            SbTime seek_to_pts,
                            int ticket)
{
  player->Seek(seek_to_pts, ticket);
}

// Writes a single sample of the given media type to |player|'s input stream.
// Its data may be passed in via more than one buffers.  The lifetime of
// |sample_buffers|, |sample_buffer_sizes|, |video_sample_info|, and
// |sample_drm_info| (as well as member |subsample_mapping| contained inside it)
// are not guaranteed past the call to SbPlayerWriteSample. That means that
// before returning, the implementation must synchronously copy any information
// it wants to retain from those structures.
//
// |player|: The player to which the sample is written.
// |sample_type|: The type of sample being written. See the |SbMediaType|
//   enum in media.h.
// |sample_buffers|: A pointer to an array of buffers with
//   |number_of_sample_buffers| elements that hold the data for this sample. The
//   buffers are expected to be a portion of a bytestream of the codec type that
//   the player was created with. The buffers should contain a sequence of whole
//   NAL Units for video, or a complete audio frame.  |sample_buffers| cannot be
//   assumed to live past the call into SbPlayerWriteSample(), so it must be
//   copied if its content will be used after SbPlayerWriteSample() returns.
// |sample_buffer_sizes|: A pointer to an array of sizes with
//   |number_of_sample_buffers| elements.  Each of them specify the number of
//   bytes in the corresponding buffer contained in |sample_buffers|.  None of
//   them can be 0.  |sample_buffer_sizes| cannot be assumed to live past the
//   call into SbPlayerWriteSample(), so it must be copied if its content will
//   be used after SbPlayerWriteSample() returns.
// |number_of_sample_buffers|: Specify the number of elements contained inside
//   |sample_buffers| and |sample_buffer_sizes|.  It has to be at least one, or
//   the call will be ignored.
// |sample_pts|: The timestamp of the sample in 90KHz ticks (PTS). Note that
//   samples MAY be written "slightly" out of order.
// |video_sample_info|: Information about a video sample. This value is
//   required if |sample_type| is |kSbMediaTypeVideo|. Otherwise, it must be
//   |NULL|.
// |sample_drm_info|: The DRM system for the media sample. This value is
//   required for encrypted samples. Otherwise, it must be |NULL|.
SB_EXPORT void SbPlayerWriteSample(
  SbPlayer player,
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
  player->WriteSample(sample_type, sample_buffers, sample_buffer_sizes,
                      number_of_sample_buffers, sample_pts,
                      video_sample_info, sample_drm_info);
}

// Writes a marker to |player|'s input stream of |stream_type| indicating that
// there are no more samples for that media type for the remainder of this
// media stream. This marker is invalidated, along with the rest of the stream's
// contents, after a call to SbPlayerSeek.
//
// |player|: The player to which the marker is written.
// |stream_type|: The type of stream for which the marker is written.
SB_EXPORT void SbPlayerWriteEndOfStream(SbPlayer player,
                                        SbMediaType stream_type)
{
  player->WriteEndOfStream(stream_type);
}

// Sets the player bounds to the given graphics plane coordinates. The changes
// do not take effect until the next graphics frame buffer swap. The default
// bounds for a player is the full screen.  This function is only relevant when
// the |player| is created with the kSbPlayerOutputModePunchOut output mode, and
// if this is not the case then this function call can be ignored.
//
// This function is called on every graphics frame that changes the video
// bounds. For example, if the video bounds are being animated, then this will
// be called at up to 60 Hz. Since the function could be called up to once per
// frame, implementors should take care to avoid related performance concerns
// with such frequent calls.
//
// |player|: The player that is being resized.
// |z_index|: The z-index of the player.  When the bounds of multiple players
//            are overlapped, the one with larger z-index will be rendered on
//            top of the ones with smaller z-index.
// |x|: The x-coordinate of the upper-left corner of the player.
// |y|: The y-coordinate of the upper-left corner of the player.
// |width|: The width of the player, in pixels.
// |height|: The height of the player, in pixels.
SB_EXPORT void SbPlayerSetBounds(SbPlayer player,
                                 int z_index,
                                 int x,
                                 int y,
                                 int width,
                                 int height)
{
  player->SetBounds(z_index, x, y, width, height);
}

// Set the playback rate of the |player|.  |rate| is default to 1.0 which
// indicates the playback is at its original speed.  A |rate| greater than one
// will make the playback faster than its original speed.  For example, when
// |rate| is 2, the video will be played at twice the speed as its original
// speed.  A |rate| less than 1.0 will make the playback slower than its
// original speed.  When |rate| is 0, the playback will be paused.
// The function returns true when the playback rate is set to |playback_rate| or
// to a rate that is close to |playback_rate| which the implementation supports.
// It returns false when the playback rate is unchanged, this can happen when
// |playback_rate| is negative or if it is too high to support.
SB_EXPORT bool SbPlayerSetPlaybackRate(SbPlayer player, double playback_rate)
{
  return player->SetPlaybackRate(playback_rate);
}

// Sets the player's volume.
//
// |player|: The player in which the volume is being adjusted.
// |volume|: The new player volume. The value must be between |0.0| and |1.0|,
//   inclusive. A value of |0.0| means that the audio should be muted, and a
//   value of |1.0| means that it should be played at full volume.
SB_EXPORT void SbPlayerSetVolume(SbPlayer player, double volume)
{
  player->SetVolume(volume);
}

// Gets a snapshot of the current player state and writes it to
// |out_player_info|. This function may be called very frequently and is
// expected to be inexpensive.
//
// |player|: The player about which information is being retrieved.
// |out_player_info|: The information retrieved for the player.
SB_EXPORT void SbPlayerGetInfo(SbPlayer player, SbPlayerInfo2* out_player_info)
{
  player->GetInfo(out_player_info);
}

// Given a player created with the kSbPlayerOutputModeDecodeToTexture
// output mode, it will return a SbDecodeTarget representing the current frame
// to be rasterized.  On GLES systems, this function must be called on a
// thread with an EGLContext current, and specifically the EGLContext that will
// be used to eventually render the frame.  If this function is called with a
// |player| object that was created with an output mode other than
// kSbPlayerOutputModeDecodeToTexture, kSbDecodeTargetInvalid is returned.
SB_EXPORT SbDecodeTarget SbPlayerGetCurrentFrame(SbPlayer player)
{
  return player->GetCurrentFrame();
}

SB_EXPORT bool SbMediaIsSupported(SbMediaVideoCodec videoCodec,
                                  SbMediaAudioCodec audioCodec,
                                  const char* keySystem)
{
  if (keySystem) {
    SB_DLOG(INFO) << "pretending to support " << keySystem;
  }
  const bool result = SbMediaIsAudioSupported(audioCodec, 0)
    && SbMediaIsVideoSupported(videoCodec, 0, 0, 0, 0, 0, kSbMediaTransferIdUnspecified);
  return result;
}

SB_EXPORT bool SbMediaIsTransferCharacteristicsSupported(
    SbMediaTransferId transferId)
{
  // let's support only standard defined values for now
  return transferId <= kSbMediaTransferIdLastStandardValue;
}
