/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** liveaudiosource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#ifdef WIN32
#include "base/win/wincrypt_shim.h"
#endif

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>
#include <cctype>
#include <chrono>

#include "environment.h"

#include "pc/local_audio_source.h"
#include "api/environment/environment_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"

template <typename T>
class LiveAudioSource : public webrtc::Notifier<webrtc::AudioSourceInterface>, public T::Callback
{
public:
    SourceState state() const override { return kLive; }
    bool remote() const override { return true; }

    virtual void AddSink(webrtc::AudioTrackSinkInterface *sink) override
    {
        RTC_LOG(LS_INFO) << "LiveAudioSource::AddSink ";
        std::lock_guard<std::mutex> lock(m_sink_lock);
        m_sinks.push_back(sink);
    }
    virtual void RemoveSink(webrtc::AudioTrackSinkInterface *sink) override
    {
        RTC_LOG(LS_INFO) << "LiveAudioSource::RemoveSink ";
        std::lock_guard<std::mutex> lock(m_sink_lock);
        m_sinks.remove(sink);
    }

    void CaptureThread()
    {
        m_env.mainloop();
    }

    // overide RTSPConnection::Callback
    bool onNewSession(const char *id, const char *media, const char *codec, const char *sdp, unsigned int rtpfrequency, unsigned int channels) override
    {
        bool success = false;
        if (strcmp(media, "audio") == 0)
        {
            RTC_LOG(LS_INFO) << "LiveAudioSource::onNewSession " << media << "/" << codec << " " << sdp;

            m_freq = rtpfrequency;
            m_channel = channels;

            RTC_LOG(LS_INFO) << "LiveAudioSource::onNewSession codec:" << " freq:" << m_freq << " channel:" << m_channel;
            std::map<std::string, std::string> params;
            if (m_channel == 2)
            {
                params["stereo"] = "1";
            }

            webrtc::SdpAudioFormat format = webrtc::SdpAudioFormat(codec, m_freq, m_channel, std::move(params));
            if (m_factory->IsSupportedDecoder(format))
            {
                m_decoder = m_factory->Create(m_webrtcenv, format, std::optional<webrtc::AudioCodecPairId>());
                m_codec[id] = codec;
                success = true;
            }
            else
            {
                RTC_LOG(LS_ERROR) << "LiveAudioSource::onNewSession not support codec" << sdp;
            }
        }
        return success;
    }
    bool onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime) override
    {
        bool success = false;
        int segmentLength = m_freq / 100;

        if (m_codec.find(id) != m_codec.end())
        {
            int64_t sourcets = presentationTime.tv_sec;
            sourcets = sourcets * 1000 + presentationTime.tv_usec / 1000;

            int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000 / 1000;

            RTC_LOG(LS_VERBOSE) << "LiveAudioSource::onData decode ts:" << ts
                              << " source ts:" << sourcets;

            if (m_decoder.get() != NULL)
            {

                // waiting
                if ((m_wait) && (m_prevts != 0))
                {
                    int64_t periodSource = sourcets - m_previmagets;
                    int64_t periodDecode = ts - m_prevts;

                    RTC_LOG(LS_VERBOSE) << "LiveAudioSource::onData interframe decode:" << periodDecode << " source:" << periodSource;
                    int64_t delayms = periodSource - periodDecode;
                    if ((delayms > 0) && (delayms < 1000))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(delayms));
                    }
                }

                int maxDecodedBufferSize = m_decoder->PacketDuration(buffer, size) * m_channel * sizeof(int16_t);
                int16_t *decoded = new int16_t[maxDecodedBufferSize];
                webrtc::AudioDecoder::SpeechType speech_type;
                int decodedBufferSize = m_decoder->Decode(buffer, size, m_freq, maxDecodedBufferSize, decoded, &speech_type);
                RTC_LOG(LS_VERBOSE) << "LiveAudioSource::onData size:" << size << " decodedBufferSize:" << decodedBufferSize << " maxDecodedBufferSize: " << maxDecodedBufferSize << " channels: " << m_channel;
                if (decodedBufferSize > 0)
                {
                    for (int i = 0; i < decodedBufferSize; ++i)
                    {
                        m_buffer.push(decoded[i]);
                    }
                }
                else
                {
                    RTC_LOG(LS_ERROR) << "LiveAudioSource::onData error:Decode Audio failed";
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

                m_previmagets = sourcets;
                m_prevts = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000 / 1000;

                success = true;
            }
            else
            {
                RTC_LOG(LS_VERBOSE) << "LiveAudioSource::onData error:No Audio decoder";
            }
        }
        return success;
    }

protected:
    LiveAudioSource(rtc::scoped_refptr<webrtc::AudioDecoderFactory> audioDecoderFactory, const std::string &uri, const std::map<std::string, std::string> &opts, bool wait)
        : m_env(m_stop)
        , m_liveclient(m_env, this, uri.c_str(), opts, rtc::LogMessage::GetLogToDebug() <= 2)
        , m_webrtcenv(webrtc::CreateEnvironment())
        , m_factory(audioDecoderFactory)
        , m_freq(8000)
        , m_channel(1)
        , m_wait(wait)
        , m_previmagets(0)
        , m_prevts(0)
    {
        m_liveclient.start();
        m_capturethread = std::thread(&LiveAudioSource::CaptureThread, this);
    }
    virtual ~LiveAudioSource()
    {
        m_liveclient.stop();
        m_env.stop();
        m_capturethread.join();
    }

private:
    char m_stop;
    Environment m_env;

private:
    T                                               m_liveclient;
    const webrtc::Environment                       m_webrtcenv;
    std::thread                                     m_capturethread;
    rtc::scoped_refptr<webrtc::AudioDecoderFactory> m_factory;
    std::unique_ptr<webrtc::AudioDecoder>           m_decoder;
    int                                             m_freq;
    int                                             m_channel;
    std::queue<uint16_t>                            m_buffer;
    std::list<webrtc::AudioTrackSinkInterface *>    m_sinks;
    std::mutex                                      m_sink_lock;

    std::map<std::string, std::string>              m_codec;

    bool                                            m_wait;
    int64_t                                         m_previmagets;
    int64_t                                         m_prevts;
};
