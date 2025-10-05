/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** AACDecoder.cpp
**
** AAC Audio Decoder using FFmpeg (when available)
**
** -------------------------------------------------------------------------*/

#include "AACDecoder.h"

#ifdef HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

AACDecoder::AACDecoder(int sample_rate_hz, size_t num_channels)
    : sample_rate_hz_(sample_rate_hz),
      num_channels_(num_channels),
      codec_context_(nullptr),
      codec_(nullptr),
      frame_(nullptr),
      packet_(nullptr),
      initialized_(false) {
}

AACDecoder::~AACDecoder() {
    if (frame_) {
        av_frame_free(&frame_);
    }
    if (packet_) {
        av_packet_free(&packet_);
    }
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
    }
}

bool AACDecoder::Init() {
    if (initialized_) {
        return true;
    }

    // Find AAC decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (!codec_) {
        RTC_LOG(LS_ERROR) << "AAC decoder not found in FFmpeg";
        return false;
    }

    // Allocate codec context
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
        RTC_LOG(LS_ERROR) << "Failed to allocate AAC codec context";
        return false;
    }

    // Set codec parameters
    codec_context_->sample_rate = sample_rate_hz_;
    codec_context_->channels = num_channels_;
    codec_context_->channel_layout = (num_channels_ == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;

    // Open codec
    if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
        RTC_LOG(LS_ERROR) << "Failed to open AAC codec";
        avcodec_free_context(&codec_context_);
        return false;
    }

    // Allocate frame and packet
    frame_ = av_frame_alloc();
    packet_ = av_packet_alloc();

    if (!frame_ || !packet_) {
        RTC_LOG(LS_ERROR) << "Failed to allocate AAC frame or packet";
        return false;
    }

    initialized_ = true;
    RTC_LOG(LS_INFO) << "AAC decoder initialized: " << sample_rate_hz_ << "Hz, " 
                      << num_channels_ << " channels";
    return true;
}

void AACDecoder::Reset() {
    if (codec_context_) {
        avcodec_flush_buffers(codec_context_);
    }
}

int AACDecoder::Decode(const uint8_t* encoded,
                        size_t encoded_len,
                        int sample_rate_hz,
                        size_t max_decoded_bytes,
                        int16_t* decoded,
                        SpeechType* speech_type) {
    if (!initialized_ && !Init()) {
        return -1;
    }

    if (!encoded || encoded_len == 0) {
        return 0;
    }

    // Prepare packet
    packet_->data = const_cast<uint8_t*>(encoded);
    packet_->size = encoded_len;

    // Send packet to decoder
    int ret = avcodec_send_packet(codec_context_, packet_);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << "Error sending AAC packet for decoding: " << ret;
        return -1;
    }

    // Receive decoded frame
    ret = avcodec_receive_frame(codec_context_, frame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    } else if (ret < 0) {
        RTC_LOG(LS_WARNING) << "Error receiving AAC decoded frame: " << ret;
        return -1;
    }

    // Convert decoded samples to int16_t
    int samples_per_channel = frame_->nb_samples;
    int total_samples = samples_per_channel * num_channels_;

    if (total_samples * sizeof(int16_t) > max_decoded_bytes) {
        RTC_LOG(LS_ERROR) << "Decoded AAC buffer too small";
        return -1;
    }

    // Convert float samples to int16_t
    // Note: FFmpeg AAC decoder typically outputs float samples
    if (frame_->format == AV_SAMPLE_FMT_FLTP) {
        // Planar float format
        for (int i = 0; i < samples_per_channel; i++) {
            for (size_t ch = 0; ch < num_channels_; ch++) {
                float sample = ((float*)frame_->data[ch])[i];
                // Clamp and convert to int16
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                decoded[i * num_channels_ + ch] = static_cast<int16_t>(sample * 32767.0f);
            }
        }
    } else if (frame_->format == AV_SAMPLE_FMT_S16P) {
        // Planar int16 format
        for (int i = 0; i < samples_per_channel; i++) {
            for (size_t ch = 0; ch < num_channels_; ch++) {
                decoded[i * num_channels_ + ch] = ((int16_t*)frame_->data[ch])[i];
            }
        }
    } else {
        RTC_LOG(LS_ERROR) << "Unsupported AAC sample format: " << frame_->format;
        return -1;
    }

    if (speech_type) {
        *speech_type = kSpeech;
    }

    av_frame_unref(frame_);
    return total_samples;
}

#endif // HAVE_FFMPEG

