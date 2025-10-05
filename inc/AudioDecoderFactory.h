/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** AudioDecoderFactory.h
**
** Custom Audio Decoder Factory with AAC support
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_format.h"
#include "rtc_base/ref_counted_object.h"

#ifdef HAVE_FFMPEG
#include "AACDecoder.h"
#endif

#include <memory>
#include <vector>
#include <algorithm>

// Custom Audio Decoder Factory that extends the builtin factory with AAC support
class AudioDecoderFactory : public webrtc::AudioDecoderFactory {
public:
    AudioDecoderFactory() : builtin_factory_(webrtc::CreateBuiltinAudioDecoderFactory()) {}
    
    std::vector<webrtc::AudioCodecSpec> GetSupportedDecoders() override {
        std::vector<webrtc::AudioCodecSpec> specs = builtin_factory_->GetSupportedDecoders();
        
#ifdef HAVE_FFMPEG
        // Add AAC decoder support for common sample rates
        // AAC is identified as "mpeg4-generic" in RTSP streams
        std::vector<int> sample_rates = {48000, 44100, 32000, 24000, 16000, 8000};
        std::vector<int> channels = {1, 2};
        
        for (int rate : sample_rates) {
            for (int ch : channels) {
                webrtc::AudioCodecSpec aac_spec;
                aac_spec.format = webrtc::SdpAudioFormat("mpeg4-generic", rate, ch);
                specs.push_back(aac_spec);
            }
        }
        RTC_LOG(LS_INFO) << "AudioDecoderFactory: AAC support enabled via FFmpeg";
#else
        RTC_LOG(LS_INFO) << "AudioDecoderFactory: AAC support disabled (FFmpeg not available)";
#endif
        
        return specs;
    }

    bool IsSupportedDecoder(const webrtc::SdpAudioFormat& format) override {
#ifdef HAVE_FFMPEG
        // Check if it's AAC (case-insensitive comparison)
        std::string codec_name = format.name;
        std::transform(codec_name.begin(), codec_name.end(), codec_name.begin(), ::tolower);
        
        if (codec_name == "mpeg4-generic") {
            RTC_LOG(LS_INFO) << "AudioDecoderFactory: AAC/MPEG4-GENERIC codec supported";
            return true;
        }
#endif
        
        return builtin_factory_->IsSupportedDecoder(format);
    }

    std::unique_ptr<webrtc::AudioDecoder> Create(
        const webrtc::Environment& env,
        const webrtc::SdpAudioFormat& format,
        std::optional<webrtc::AudioCodecPairId> codec_pair_id) override {
        
#ifdef HAVE_FFMPEG
        // Check if it's AAC
        std::string codec_name = format.name;
        std::transform(codec_name.begin(), codec_name.end(), codec_name.begin(), ::tolower);
        
        if (codec_name == "mpeg4-generic") {
            RTC_LOG(LS_INFO) << "Creating AAC decoder for format: " << format.name 
                            << " freq: " << format.clockrate_hz 
                            << " channels: " << format.num_channels;
            
            auto decoder = std::make_unique<AACDecoder>(format.clockrate_hz, format.num_channels);
            if (decoder->Init()) {
                return decoder;
            } else {
                RTC_LOG(LS_ERROR) << "Failed to initialize AAC decoder";
                return nullptr;
            }
        }
#endif
        
        return builtin_factory_->Create(env, format, codec_pair_id);
    }

private:
    webrtc::scoped_refptr<webrtc::AudioDecoderFactory> builtin_factory_;
};

