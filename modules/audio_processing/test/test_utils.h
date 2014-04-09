/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_processing/debug.pb.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/interface/module_common_types.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

static const AudioProcessing::Error kNoErr = AudioProcessing::kNoError;
#define EXPECT_NOERR(expr) EXPECT_EQ(kNoErr, (expr))

static const int kChunkSizeMs = 10;

// Helper to encapsulate a contiguous data buffer with access to a pointer
// array of the deinterleaved channels.
template <typename T>
class ChannelBuffer {
 public:
  ChannelBuffer(int samples_per_channel, int num_channels)
      : data_(new T[samples_per_channel * num_channels]),
        channels_(new T*[num_channels]),
        samples_per_channel_(samples_per_channel) {
    memset(data_.get(), 0, sizeof(T) * samples_per_channel * num_channels);
    for (int i = 0; i < num_channels; ++i)
      channels_[i] = &data_[i * samples_per_channel];
  }
  ~ChannelBuffer() {}

  void CopyFrom(const void* channel_ptr, int index) {
    memcpy(channels_[index], channel_ptr, samples_per_channel_ * sizeof(T));
  }

  T* data() { return data_.get(); }
  T* channel(int index) { return channels_[index]; }
  T** channels() { return channels_.get(); }

 private:
  scoped_ptr<T[]> data_;
  scoped_ptr<T*[]> channels_;
  int samples_per_channel_;
};

// Exits on failure; do not use in unit tests.
static inline FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file = fopen(filename.c_str(), mode);
  if (!file) {
    printf("Unable to open file %s\n", filename.c_str());
    exit(1);
  }
  return file;
}

static inline void SetFrameSampleRate(AudioFrame* frame,
                                      int sample_rate_hz) {
  frame->sample_rate_hz_ = sample_rate_hz;
  frame->samples_per_channel_ = kChunkSizeMs * sample_rate_hz / 1000;
}

template <typename T>
void SetContainerFormat(int sample_rate_hz,
                        int num_channels,
                        AudioFrame* frame,
                        scoped_ptr<ChannelBuffer<T> >* cb) {
  SetFrameSampleRate(frame, sample_rate_hz);
  frame->num_channels_ = num_channels;
  cb->reset(new ChannelBuffer<T>(frame->samples_per_channel_, num_channels));
}

static inline AudioProcessing::ChannelLayout LayoutFromChannels(
    int num_channels) {
  switch (num_channels) {
    case 1:
      return AudioProcessing::kMono;
    case 2:
      return AudioProcessing::kStereo;
    default:
      assert(false);
      return AudioProcessing::kMono;
  }
}

// Allocates new memory in the scoped_ptr to fit the raw message and returns the
// number of bytes read.
static inline size_t ReadMessageBytesFromFile(FILE* file,
                                              scoped_ptr<uint8_t[]>* bytes) {
  // The "wire format" for the size is little-endian. Assume we're running on
  // a little-endian machine.
  int32_t size = 0;
  if (fread(&size, sizeof(size), 1, file) != 1)
    return 0;
  if (size <= 0)
    return 0;

  bytes->reset(new uint8_t[size]);
  return fread(bytes->get(), sizeof((*bytes)[0]), size, file);
}

// Returns true on success, false on error or end-of-file.
static inline bool ReadMessageFromFile(FILE* file,
                                       ::google::protobuf::MessageLite* msg) {
  scoped_ptr<uint8_t[]> bytes;
  size_t size = ReadMessageBytesFromFile(file, &bytes);
  if (!size)
    return false;

  msg->Clear();
  return msg->ParseFromArray(bytes.get(), size);
}

}  // namespace webrtc
