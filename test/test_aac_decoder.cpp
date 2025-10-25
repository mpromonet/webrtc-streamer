/* ---------------------------------------------------------------------------
** Test program for AAC audio decoder
**
** This test verifies that the AAC decoder can be instantiated and initialized
** with FFmpeg.
** -------------------------------------------------------------------------*/

#include <iostream>
#include <memory>
#include <cstdint>

#ifdef HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

// Minimal AudioDecoder interface for testing
class TestAudioDecoder {
public:
    enum SpeechType {
        kSpeech = 0,
        kComfortNoise = 1
    };

    virtual ~TestAudioDecoder() = default;
    virtual bool Init() = 0;
    virtual void Reset() = 0;
    virtual int SampleRateHz() const = 0;
    virtual size_t Channels() const = 0;
    virtual int Decode(const uint8_t* encoded,
                      size_t encoded_len,
                      int sample_rate_hz,
                      size_t max_decoded_bytes,
                      int16_t* decoded,
                      SpeechType* speech_type) = 0;
};

// Simple AAC decoder for testing
class SimpleAACDecoder : public TestAudioDecoder {
public:
    SimpleAACDecoder(int sample_rate_hz, size_t num_channels)
        : sample_rate_hz_(sample_rate_hz),
          num_channels_(num_channels),
          codec_context_(nullptr),
          frame_(nullptr),
          packet_(nullptr),
          initialized_(false) {}

    ~SimpleAACDecoder() override {
        if (frame_) av_frame_free(&frame_);
        if (packet_) av_packet_free(&packet_);
        if (codec_context_) avcodec_free_context(&codec_context_);
    }

    bool Init() override {
        if (initialized_) return true;

        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
        if (!codec) {
            std::cerr << "AAC decoder not found" << std::endl;
            return false;
        }

        codec_context_ = avcodec_alloc_context3(codec);
        if (!codec_context_) {
            std::cerr << "Failed to allocate codec context" << std::endl;
            return false;
        }

        codec_context_->sample_rate = sample_rate_hz_;
        
#if LIBAVCODEC_VERSION_MAJOR >= 59
        av_channel_layout_default(&codec_context_->ch_layout, num_channels_);
#else
        codec_context_->channels = num_channels_;
        codec_context_->channel_layout = (num_channels_ == 2) ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
#endif

        if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
            std::cerr << "Failed to open codec" << std::endl;
            avcodec_free_context(&codec_context_);
            return false;
        }

        frame_ = av_frame_alloc();
        packet_ = av_packet_alloc();

        if (!frame_ || !packet_) {
            std::cerr << "Failed to allocate frame/packet" << std::endl;
            return false;
        }

        initialized_ = true;
        std::cout << "✓ AAC decoder initialized: " << sample_rate_hz_ << "Hz, " 
                  << num_channels_ << " channels" << std::endl;
        return true;
    }

    void Reset() override {
        if (codec_context_) {
            avcodec_flush_buffers(codec_context_);
        }
    }

    int SampleRateHz() const override { return sample_rate_hz_; }
    size_t Channels() const override { return num_channels_; }

    int Decode(const uint8_t* encoded,
              size_t encoded_len,
              int sample_rate_hz,
              size_t max_decoded_bytes,
              int16_t* decoded,
              SpeechType* speech_type) override {
        // Minimal implementation for testing
        return 0;
    }

private:
    const int sample_rate_hz_;
    const size_t num_channels_;
    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    bool initialized_;
};

int main() {
    std::cout << "=== AAC Decoder Test ===" << std::endl;
    std::cout << std::endl;

    // Test common AAC configurations
    struct TestConfig {
        int sample_rate;
        size_t channels;
        const char* description;
    };

    TestConfig configs[] = {
        {48000, 2, "48kHz Stereo"},
        {44100, 2, "44.1kHz Stereo"},
        {48000, 1, "48kHz Mono"},
        {32000, 2, "32kHz Stereo"},
        {16000, 1, "16kHz Mono"},
    };

    bool all_passed = true;
    for (const auto& config : configs) {
        std::cout << "Testing " << config.description << "... ";
        auto decoder = std::make_unique<SimpleAACDecoder>(config.sample_rate, config.channels);
        if (decoder->Init()) {
            std::cout << "PASS" << std::endl;
        } else {
            std::cout << "FAIL" << std::endl;
            all_passed = false;
        }
    }

    std::cout << std::endl;
    if (all_passed) {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}

#else

int main() {
    std::cout << "AAC decoder test skipped - FFmpeg not available" << std::endl;
    std::cout << "Build with FFmpeg support to enable AAC decoding" << std::endl;
    return 0;
}

#endif // HAVE_FFMPEG
