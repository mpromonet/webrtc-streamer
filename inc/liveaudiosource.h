/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** liveaudiosource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <cctype>

#include "environment.h"
#include "rtspconnectionclient.h"

#include "pc/local_audio_source.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"

template <typename T>
class LiveAudioSource : public webrtc::Notifier<webrtc::AudioSourceInterface>, public T::Callback
{
public:

    SourceState state() const override { return kLive; }
    bool remote() const override { return true; }

    virtual void AddSink(webrtc::AudioTrackSinkInterface *sink) override
    {
        RTC_LOG(INFO) << "RTSPAudioSource::AddSink ";
        std::lock_guard<std::mutex> lock(m_sink_lock);
        m_sinks.push_back(sink);
    }
    virtual void RemoveSink(webrtc::AudioTrackSinkInterface *sink) override
    {
        RTC_LOG(INFO) << "RTSPAudioSource::RemoveSink ";
        std::lock_guard<std::mutex> lock(m_sink_lock);
        m_sinks.remove(sink);
    }

    void CaptureThread() { m_env.mainloop(); }

    // overide RTSPConnection::Callback
    virtual bool onNewSession(const char *id, const char *media, const char *codec, const char *sdp) override
    {
        bool success = false;
        if (strcmp(media, "audio") == 0)
        {
            RTC_LOG(INFO) << "RTSPAudioSource::onNewSession " << media << "/" << codec << " " << sdp;

            // parse sdp to extract freq and channel
            std::string fmt(sdp);
            std::transform(fmt.begin(), fmt.end(), fmt.begin(), [](unsigned char c) { return std::tolower(c); });
            std::string codecstr(codec);
            std::transform(codecstr.begin(), codecstr.end(), codecstr.begin(), [](unsigned char c) { return std::tolower(c); });
            size_t pos = fmt.find(codecstr);
            if (pos != std::string::npos)
            {
                fmt.erase(0, pos + strlen(codec));
                fmt.erase(fmt.find_first_of(" \r\n"));
                std::istringstream is(fmt);
                std::string dummy;
                std::getline(is, dummy, '/');
                std::string freq;
                std::getline(is, freq, '/');
                if (!freq.empty())
                {
                    m_freq = std::stoi(freq);
                }
                std::string channel;
                std::getline(is, channel, '/');
                if (!channel.empty())
                {
                    m_channel = std::stoi(channel);
                }
            }
            RTC_LOG(INFO) << "RTSPAudioSource::onNewSession codec:" << codecstr << " freq:" << m_freq << " channel:" << m_channel;
            std::map<std::string, std::string> params;
            if (m_channel == 2)
            {
                params["stereo"] = "1";
            }

            webrtc::SdpAudioFormat format = webrtc::SdpAudioFormat(codecstr, m_freq, m_channel, std::move(params));
            if (m_factory->IsSupportedDecoder(format))
            {
                m_decoder = m_factory->MakeAudioDecoder(format, absl::optional<webrtc::AudioCodecPairId>());
                success = true;
            }
            else
            {
                RTC_LOG(LS_ERROR) << "RTSPAudioSource::onNewSession not support codec" << sdp;
            }
        }
        return success;
    }
    virtual bool onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime) override
    {
        bool success = false;
        int segmentLength = m_freq / 100;
        if (m_decoder.get() != NULL)
        {
            int maxDecodedBufferSize = m_decoder->PacketDuration(buffer, size) * m_channel * sizeof(int16_t);
            int16_t *decoded = new int16_t[maxDecodedBufferSize];
            webrtc::AudioDecoder::SpeechType speech_type;
            int decodedBufferSize = m_decoder->Decode(buffer, size, m_freq, maxDecodedBufferSize, decoded, &speech_type);
            RTC_LOG(LS_VERBOSE) << "RTSPAudioSource::onData size:" << size << " decodedBufferSize:" << decodedBufferSize << " maxDecodedBufferSize: " << maxDecodedBufferSize << " channels: " << m_channel;
            if (decodedBufferSize > 0)
            {
                for (int i = 0; i < decodedBufferSize; ++i)
                {
                    m_buffer.push(decoded[i]);
                }
            }
            else
            {
                RTC_LOG(LS_ERROR) << "RTSPAudioSource::onData error:Decode Audio failed";
            }
            delete[] decoded;
            while (m_buffer.size() > segmentLength * m_channel)
            {
                int16_t *outbuffer = new int16_t[segmentLength * m_channel];
                for (int i = 0; i < segmentLength * m_channel; ++i)
                {
                    uint16_t value = m_buffer.front();
                    outbuffer[i] = value;
                    m_buffer.pop();
                }
                std::lock_guard<std::mutex> lock(m_sink_lock);
                for (auto *sink : m_sinks)
                {
                    sink->OnData(outbuffer, 16, m_freq, m_channel, segmentLength);
                }
                delete[] outbuffer;
            }
            success = true;
        }
        else
        {
            RTC_LOG(LS_VERBOSE) << "RTSPAudioSource::onData error:No Audio decoder";
        }
        return success;
    }

protected:
    LiveAudioSource(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string &uri, const std::map<std::string, std::string> &opts) 
        : m_connection(m_env, this, uri.c_str(), opts)
        , m_factory(audioDecoderFactory)
        , m_freq(8000)
        , m_channel(1)
    {
        m_capturethread = std::thread(&LiveAudioSource::CaptureThread, this);
    }
    virtual ~LiveAudioSource()
    {
        m_env.stop();
        m_capturethread.join();
    }

private:
    std::thread m_capturethread;
    Environment m_env;
    T m_connection;
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> m_factory;
    std::unique_ptr<webrtc::AudioDecoder> m_decoder;
    int m_freq;
    int m_channel;
    std::queue<uint16_t> m_buffer;
    std::list<webrtc::AudioTrackSinkInterface *> m_sinks;
    std::mutex m_sink_lock;
};
