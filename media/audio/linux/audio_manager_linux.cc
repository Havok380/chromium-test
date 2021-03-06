// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#if defined(USE_CRAS)
#include "media/audio/cras/audio_manager_cras.h"
#endif
#include "media/audio/linux/alsa_input.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"
#if defined(USE_PULSEAUDIO)
#include "media/audio/pulse/audio_manager_pulse.h"
#endif
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Default sample rate for input and output streams.
static const int kDefaultSampleRate = 48000;

// Since "default", "pulse" and "dmix" devices are virtual devices mapped to
// real devices, we remove them from the list to avoiding duplicate counting.
// In addition, note that we support no more than 2 channels for recording,
// hence surround devices are not stored in the list.
static const char* kInvalidAudioInputDevices[] = {
  "default",
  "null",
  "pulse",
  "dmix",
  "surround",
};

enum LinuxAudioIO {
  kPulse,
  kAlsa,
  kCras,
  kAudioIOMax  // Must always be last!
};

// static
void AudioManagerLinux::ShowLinuxAudioInputSettings() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  CommandLine command_line(CommandLine::NO_PROGRAM);
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      command_line.SetProgram(base::FilePath("gnome-volume-control"));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      command_line.SetProgram(base::FilePath("kmix"));
      break;
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
      command_line.SetProgram(base::FilePath("gnome-control-center"));
      command_line.AppendArg("sound");
      command_line.AppendArg("input");
      break;
    default:
      LOG(ERROR) << "Failed to show audio input settings: we don't know "
                 << "what command to use for your desktop environment.";
      return;
  }
  base::LaunchProcess(command_line, base::LaunchOptions(), NULL);
}

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioOutputDevices() {
  return HasAnyAlsaAudioDevice(kStreamPlayback);
}

bool AudioManagerLinux::HasAudioInputDevices() {
  return HasAnyAlsaAudioDevice(kStreamCapture);
}

AudioManagerLinux::AudioManagerLinux()
    : wrapper_(new AlsaWrapper()) {
  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerLinux::~AudioManagerLinux() {
  Shutdown();
}

void AudioManagerLinux::ShowAudioInputSettings() {
  ShowLinuxAudioInputSettings();
}

void AudioManagerLinux::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  GetAlsaAudioInputDevices(device_names);
}

AudioParameters AudioManagerLinux::GetInputStreamParameters(
    const std::string& device_id) {
  static const int kDefaultInputBufferSize = 1024;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      kDefaultSampleRate, 16, kDefaultInputBufferSize);
}

void AudioManagerLinux::GetAlsaAudioInputDevices(
    media::AudioDeviceNames* device_names) {
  // Constants specified by the ALSA API for device hints.
  static const char kPcmInterfaceName[] = "pcm";
  int card = -1;

  // Loop through the sound cards to get ALSA device hints.
  while (!wrapper_->CardNext(&card) && card >= 0) {
    void** hints = NULL;
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      GetAlsaDevicesInfo(hints, device_names);

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
    } else {
      DLOG(WARNING) << "GetAudioInputDevices: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }
}

void AudioManagerLinux::GetAlsaDevicesInfo(
    void** hints, media::AudioDeviceNames* device_names) {
  static const char kIoHintName[] = "IOID";
  static const char kNameHintName[] = "NAME";
  static const char kDescriptionHintName[] = "DESC";
  static const char kOutputDevice[] = "Output";

  for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
    // Only examine devices that are input capable.  Valid values are
    // "Input", "Output", and NULL which means both input and output.
    scoped_ptr_malloc<char> io(wrapper_->DeviceNameGetHint(*hint_iter,
                                                           kIoHintName));
    if (io != NULL && strcmp(kOutputDevice, io.get()) == 0)
      continue;

    // Found an input device, prepend the default device since we always want
    // it to be on the top of the list for all platforms. And there is no
    // duplicate counting here since it is only done if the list is still empty.
    // Note, pulse has exclusively opened the default device, so we must open
    // the device via the "default" moniker.
    if (device_names->empty()) {
      device_names->push_front(media::AudioDeviceName(
          AudioManagerBase::kDefaultDeviceName,
          AudioManagerBase::kDefaultDeviceId));
    }

    // Get the unique device name for the device.
    scoped_ptr_malloc<char> unique_device_name(
        wrapper_->DeviceNameGetHint(*hint_iter, kNameHintName));

    // Find out if the device is available.
    if (IsAlsaDeviceAvailable(unique_device_name.get())) {
      // Get the description for the device.
      scoped_ptr_malloc<char> desc(wrapper_->DeviceNameGetHint(
          *hint_iter, kDescriptionHintName));

      media::AudioDeviceName name;
      name.unique_id = unique_device_name.get();
      if (desc.get()) {
        // Use the more user friendly description as name.
        // Replace '\n' with '-'.
        char* pret = strchr(desc.get(), '\n');
        if (pret)
          *pret = '-';
        name.device_name = desc.get();
      } else {
        // Virtual devices don't necessarily have descriptions.
        // Use their names instead.
        name.device_name = unique_device_name.get();
      }

      // Store the device information.
      device_names->push_back(name);
    }
  }
}

bool AudioManagerLinux::IsAlsaDeviceAvailable(const char* device_name) {
  if (!device_name)
    return false;

  // Check if the device is in the list of invalid devices.
  for (size_t i = 0; i < arraysize(kInvalidAudioInputDevices); ++i) {
    if (strncmp(kInvalidAudioInputDevices[i], device_name,
                strlen(kInvalidAudioInputDevices[i])) == 0)
      return false;
  }

  return true;
}

bool AudioManagerLinux::HasAnyAlsaAudioDevice(StreamType stream) {
  static const char kPcmInterfaceName[] = "pcm";
  static const char kIoHintName[] = "IOID";
  const char* kNotWantedDevice =
      (stream == kStreamPlayback ? "Input" : "Output");
  void** hints = NULL;
  bool has_device = false;
  int card = -1;

  // Loop through the sound cards.
  // Don't use snd_device_name_hint(-1,..) since there is a access violation
  // inside this ALSA API with libasound.so.2.0.0.
  while (!wrapper_->CardNext(&card) && (card >= 0) && !has_device) {
    int error = wrapper_->DeviceNameHint(card, kPcmInterfaceName, &hints);
    if (!error) {
      for (void** hint_iter = hints; *hint_iter != NULL; hint_iter++) {
        // Only examine devices that are |stream| capable.  Valid values are
        // "Input", "Output", and NULL which means both input and output.
        scoped_ptr_malloc<char> io(wrapper_->DeviceNameGetHint(*hint_iter,
                                                               kIoHintName));
        if (io != NULL && strcmp(kNotWantedDevice, io.get()) == 0)
          continue;  // Wrong type, skip the device.

        // Found an input device.
        has_device = true;
        break;
      }

      // Destroy the hints now that we're done with it.
      wrapper_->DeviceNameFreeHint(hints);
      hints = NULL;
    } else {
      DLOG(WARNING) << "HasAnyAudioDevice: unable to get device hints: "
                    << wrapper_->StrError(error);
    }
  }

  return has_device;
}

AudioOutputStream* AudioManagerLinux::MakeLinearOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeOutputStream(params);
}

AudioOutputStream* AudioManagerLinux::MakeLowLatencyOutputStream(
    const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeOutputStream(params);
}

AudioInputStream* AudioManagerLinux::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return MakeInputStream(params, device_id);
}

AudioInputStream* AudioManagerLinux::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  return MakeInputStream(params, device_id);
}

AudioParameters AudioManagerLinux::GetPreferredOutputStreamParameters(
    const AudioParameters& input_params) {
  static const int kDefaultOutputBufferSize = 2048;
  ChannelLayout channel_layout = CHANNEL_LAYOUT_STEREO;
  int sample_rate = kDefaultSampleRate;
  int buffer_size = kDefaultOutputBufferSize;
  int bits_per_sample = 16;
  int input_channels = 0;
  if (input_params.IsValid()) {
    // Some clients, such as WebRTC, have a more limited use case and work
    // acceptably with a smaller buffer size.  The check below allows clients
    // which want to try a smaller buffer size on Linux to do so.
    // TODO(dalecurtis): This should include bits per channel and channel layout
    // eventually.
    sample_rate = input_params.sample_rate();
    bits_per_sample = input_params.bits_per_sample();
    channel_layout = input_params.channel_layout();
    input_channels = input_params.input_channels();
    buffer_size = std::min(input_params.frames_per_buffer(), buffer_size);
  }

  int user_buffer_size = GetUserBufferSize();
  if (user_buffer_size)
    buffer_size = user_buffer_size;

  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout, input_channels,
      sample_rate, bits_per_sample, buffer_size);
}

AudioOutputStream* AudioManagerLinux::MakeOutputStream(
    const AudioParameters& params) {
  std::string device_name = AlsaPcmOutputStream::kAutoSelectDevice;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAlsaOutputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaOutputDevice);
  }
  return new AlsaPcmOutputStream(device_name, params, wrapper_.get(), this);
}

AudioInputStream* AudioManagerLinux::MakeInputStream(
    const AudioParameters& params, const std::string& device_id) {
  std::string device_name = (device_id == AudioManagerBase::kDefaultDeviceId) ?
      AlsaPcmInputStream::kAutoSelectDevice : device_id;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAlsaInputDevice)) {
    device_name = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAlsaInputDevice);
  }

  return new AlsaPcmInputStream(this, device_name, params, wrapper_.get());
}

AudioManager* CreateAudioManager() {
#if defined(USE_CRAS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseCras)) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kCras, kAudioIOMax);
    return new AudioManagerCras();
  }
#endif

#if defined(USE_PULSEAUDIO)
  AudioManager* manager = AudioManagerPulse::Create();
  if (manager) {
    UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kPulse, kAudioIOMax);
    return manager;
  }
#endif

  UMA_HISTOGRAM_ENUMERATION("Media.LinuxAudioIO", kAlsa, kAudioIOMax);
  return new AudioManagerLinux();
}

}  // namespace media
