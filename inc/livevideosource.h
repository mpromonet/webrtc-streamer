/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** livevideosource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <string.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "environment.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "media/base/codec.h"
#include "media/base/video_common.h"
#include "media/base/video_broadcaster.h"
#include "media/engine/internal_decoder_factory.h"

#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"

#include "api/video_codecs/video_decoder.h"

#include "VideoDecoder.h"

template <typename T>
class LiveVideoSource : public VideoSourceWithDecoder, public T::Callback
{
public:
    LiveVideoSource(const std::string &uri, const std::map<std::string, std::string> &opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, bool wait) :
        m_env(m_stop),
	    m_liveclient(m_env, this, uri.c_str(), opts, rtc::LogMessage::GetLogToDebug()<=2),
	    VideoSourceWithDecoder(opts, videoDecoderFactory, wait) {
            this->Start();
    }
    virtual ~LiveVideoSource() {
            this->Stop();
    }

    void Start()
    {
        RTC_LOG(LS_INFO) << "LiveVideoSource::Start";
        m_capturethread = std::thread(&LiveVideoSource::CaptureThread, this);
    }
    void Stop()
    {
        RTC_LOG(LS_INFO) << "LiveVideoSource::stop";
        m_env.stop();
        m_capturethread.join();
    }
    bool IsRunning() { return (m_stop == 0); }

    void CaptureThread()
    {
        m_env.mainloop();
    }

    // overide T::Callback
    virtual bool onNewSession(const char *id, const char *media, const char *codec, const char *sdp)
    {
        bool success = false;
        if (strcmp(media, "video") == 0)
        {
            RTC_LOG(LS_INFO) << "LiveVideoSource::onNewSession id:"<< id << " media:" << media << "/" << codec << " sdp:" << sdp;

            if ( (strcmp(codec, "H264") == 0)
               || (strcmp(codec, "JPEG") == 0)
               || (strcmp(codec, "VP9") == 0) )
            {
                RTC_LOG(LS_INFO) << "LiveVideoSource::onNewSession id:'" << id << "' '" << codec << "'\n";
                m_codec[id] = codec;
                success = true;
            }
            RTC_LOG(LS_INFO) << "LiveVideoSource::onNewSession success:" << success << "\n";
            if (success) 
            {
                struct timeval presentationTime;
                timerclear(&presentationTime);

                std::vector<std::vector<uint8_t>> initFrames = m_decoder.getInitFrames(codec, sdp);
                for (auto frame : initFrames)
                {
                    onData(id, frame.data(), frame.size(), presentationTime);
                }
            }
        }
        return success;
    }

    virtual bool onData(const char *id, unsigned char *buffer, ssize_t size, struct timeval presentationTime)
    {
        int64_t ts = presentationTime.tv_sec;
        ts = ts * 1000 + presentationTime.tv_usec / 1000;
        RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData id:" << id << " size:" << size << " ts:" << ts;
        int res = 0;

        std::string codec = m_codec[id];
        if (codec == "H264")
        {
            std::vector<webrtc::H264::NaluIndex> indexes = webrtc::H264::FindNaluIndices(buffer,size);
            RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData nbNalu:" << indexes.size();
            for (const webrtc::H264::NaluIndex & index : indexes) {
                webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(buffer[index.payload_start_offset]);
                if (nalu_type == webrtc::H264::NaluType::kSps)
                {
                    RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData SPS";
                    m_cfg.clear();
                    m_cfg.insert(m_cfg.end(), buffer + index.start_offset, buffer + index.payload_size + index.payload_start_offset - index.start_offset);

                    absl::optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(buffer + index.payload_start_offset, index.payload_size);
                    if (!sps)
                    {
                        RTC_LOG(LS_ERROR) << "cannot parse sps";
                        res = -1;
                    }
                    else
                    {
                        int fps = 25;
                        RTC_LOG(LS_INFO) << "LiveVideoSource:onData SPS set format " << sps->width << "x" << sps->height << " fps:" << fps;
                        cricket::VideoFormat videoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(fps), cricket::FOURCC_H264);
                        m_decoder.postFormat(videoFormat);
                    }
                }
                else if (nalu_type == webrtc::H264::NaluType::kPps)
                {
                    RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData PPS";
                    m_cfg.insert(m_cfg.end(), buffer + index.start_offset, buffer + index.payload_size + index.payload_start_offset - index.start_offset);
                }
                else if (nalu_type == webrtc::H264::NaluType::kSei) 
                {
                }            
                else
                {
                    webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameDelta;
                    std::vector<uint8_t> content;
                    if (nalu_type == webrtc::H264::NaluType::kIdr)
                    {
                        frameType = webrtc::VideoFrameType::kVideoFrameKey;
                        RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData IDR";
                        content.insert(content.end(), m_cfg.begin(), m_cfg.end());
                    }
                    else
                    {
                        RTC_LOG(LS_VERBOSE) << "LiveVideoSource:onData SLICE NALU:" << nalu_type;
                    }
                    std::string decoderName(m_decoder.m_decoder->ImplementationName());
                    if (m_prevTimestamp && ts < m_prevTimestamp && decoderName == "FFmpeg") {
                        RTC_LOG(LS_ERROR) << "LiveVideoSource:onData drop frame in past for FFmpeg:" << (m_prevTimestamp-ts);

                    } else {
                        content.insert(content.end(), buffer + index.start_offset, buffer + index.payload_size + index.payload_start_offset - index.start_offset);
                        rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(content.data(), content.size());
                        m_decoder.PostFrame(frame, ts, frameType);
                    }
                }
            }
        }
        else if (codec == "JPEG")
        {
            int32_t width = 0;
            int32_t height = 0;
            if (libyuv::MJPGSize(buffer, size, &width, &height) == 0)
            {
                int stride_y = width;
                int stride_uv = (width + 1) / 2;

                rtc::scoped_refptr<webrtc::I420Buffer> I420buffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);
                const int conversionResult = libyuv::ConvertToI420((const uint8_t *)buffer, size,
                                                                   I420buffer->MutableDataY(), I420buffer->StrideY(),
                                                                   I420buffer->MutableDataU(), I420buffer->StrideU(),
                                                                   I420buffer->MutableDataV(), I420buffer->StrideV(),
                                                                   0, 0,
                                                                   width, height,
                                                                   width, height,
                                                                   libyuv::kRotate0, ::libyuv::FOURCC_MJPG);

                if (conversionResult >= 0)
                {
                    webrtc::VideoFrame frame = webrtc::VideoFrame::Builder()
                        .set_video_frame_buffer(I420buffer)
                        .set_rotation(webrtc::kVideoRotation_0)
                        .set_timestamp_ms(ts)
                        .set_id(ts)
                        .build();
                    m_decoder.Decoded(frame);
                }
                else
                {
                    RTC_LOG(LS_ERROR) << "LiveVideoSource:onData decoder error:" << conversionResult;
                    res = -1;
                }
            }
            else
            {
                RTC_LOG(LS_ERROR) << "LiveVideoSource:onData cannot JPEG dimension";
                res = -1;
            }
        }
        else if (codec == "VP9")
        {
            cricket::VideoFormat videoFormat(0, 0, cricket::VideoFormat::FpsToInterval(0), FOURCC_VP9);
            m_decoder.postFormat(videoFormat);

            webrtc::VideoFrameType frameType = webrtc::VideoFrameType::kVideoFrameKey;
            rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(buffer, size);
            m_decoder.PostFrame(frame, ts, frameType);
        }

        m_prevTimestamp = ts;
        return (res == 0);
    }


private:
    char        m_stop;
    Environment m_env;

protected:
    T m_liveclient;

private:
    std::thread                        m_capturethread;
    std::vector<uint8_t>               m_cfg;
    std::map<std::string, std::string> m_codec;

    uint64_t                           m_prevTimestamp;
};
