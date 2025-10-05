/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** AACDecoder.h
**
** AAC Audio Decoder using FFmpeg (when available)
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/audio_codecs/audio_decoder.h"
#include "rtc_base/logging.h"

// Forward declare FFmpeg types to avoid including FFmpeg headers globally
struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;

class AACDecoder : public webrtc::AudioDecoder {
public:
    AACDecoder(int sample_rate_hz, size_t num_channels);
    ~AACDecoder() override;

    bool Init();
    void Reset() override;
    int SampleRateHz() const override { return sample_rate_hz_; }
    size_t Channels() const override { return num_channels_; }
    
    int Decode(const uint8_t* encoded,
                size_t encoded_len,
                int sample_rate_hz,
                size_t max_decoded_bytes,
                int16_t* decoded,
                SpeechType* speech_type) override;

private:
    const int sample_rate_hz_;
    const size_t num_channels_;
    
    AVCodecContext* codec_context_;
    AVCodec* codec_;
    AVFrame* frame_;
    AVPacket* packet_;
    bool initialized_;
};
