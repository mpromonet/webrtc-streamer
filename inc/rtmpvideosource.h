/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtmpvideosource.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <string.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "media/base/codec.h"
#include "media/base/video_common.h"
#include "media/base/video_broadcaster.h"
#include "media/engine/internal_decoder_factory.h"

#include "common_video/h264/h264_common.h"
#include "common_video/h264/sps_parser.h"

#include "common_video/h265/h265_common.h"
#include "common_video/h265/h265_sps_parser.h"

#include "api/video_codecs/video_decoder.h"

#include "VideoDecoder.h"

#include <librtmp/rtmp.h>
#include <librtmp/log.h>

#define CODEC_ID_AVC 7
#define CODEC_ID_HEVC 12

class RtmpVideoSource : public VideoDecoder
{
public:
    static RtmpVideoSource *Create(const std::string &url, const std::map<std::string, std::string> &opts, std::unique_ptr<webrtc::VideoDecoderFactory> &videoDecoderFactory)
    {
        std::unique_ptr<RtmpVideoSource> capturer(new RtmpVideoSource(url, opts, videoDecoderFactory));
        return capturer.release();
    }
    virtual ~RtmpVideoSource()
    {
        this->Stop();
        RTMP_Close(&m_rtmp);
    }
    
private:
    RtmpVideoSource(const std::string &uri, const std::map<std::string, std::string> &opts, std::unique_ptr<webrtc::VideoDecoderFactory> &videoDecoderFactory) : 
        m_stop(false),
        m_url(uri),
        VideoDecoder(opts, videoDecoderFactory)
    {
        RTMP_Init(&m_rtmp);
        RTMP_LogSetOutput(stderr);
        RTMP_LogSetLevel(RTMP_LOGINFO);
        if (!RTMP_SetupURL(&m_rtmp, const_cast<char*>(m_url.c_str()))) {
            RTC_LOG(LS_INFO) << "Unable to parse rtmp url:" << m_url;
        }

        this->Start();
    }

    void Start()
    {
        RTC_LOG(LS_INFO) << "RtmpVideoSource::Start";
        m_capturethread = std::thread(&RtmpVideoSource::CaptureThread, this);
    }
    void Stop()
    {
        RTC_LOG(LS_INFO) << "RtmpVideoSource::stop";
        m_stop = true;
        m_capturethread.join();
    }
    bool IsRunning() { return (!m_stop); }

    void CaptureThread()
    {
        RTC_LOG(LS_INFO) << "RtmpVideoSource::CaptureThread begin";
        while (!m_stop)
        {
            if ( !RTMP_IsConnected(&m_rtmp) && (!RTMP_Connect(&m_rtmp, NULL) || !RTMP_ConnectStream(&m_rtmp, 0)) ) {
                RTC_LOG(LS_INFO) << "Unable to connect to stream";
            } 

            if (RTMP_IsConnected(&m_rtmp)) {
                RTMPPacket packet;
                if (RTMP_ReadPacket(&m_rtmp, &packet)) {
                    RTMPPacket_Dump(&packet);
                    if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO) {
                        this->processVideoPacket(packet.m_body, packet.m_nBodySize);
                    }
                }
                RTMPPacket_Free(&packet);
            }
        }
        RTC_LOG(LS_INFO) << "RtmpVideoSource::CaptureThread end";
    }

private:
    char m_stop;

    void processVideoPacket(char * body, unsigned int size) {
        int64_t ts = RTMP_GetTime();

        unsigned char frameTypeAndCodecId = body[0];
        unsigned char frameType = frameTypeAndCodecId >> 4;  // Shift right to get the first 4 bits
        unsigned char codecId = frameTypeAndCodecId & 0x0F;  // Bitwise AND to get the last 4 bits

        if (codecId == CODEC_ID_AVC) {
            if (frameType == 1 && body[1] == 0) {
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 SPS/PPS";
                int start_sps = 11;
                int spssize = (body[start_sps]<<8) + body[start_sps+1];
                webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(body[start_sps+2]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 NALU type:" << nalu_type;
                if (nalu_type == webrtc::H264::NaluType::kSps)
                {
                    m_cfg.clear();
                    RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 SPS size:" << spssize;
                    absl::optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps((const unsigned char*)(&body[start_sps+3]), spssize);
                    if (!sps)
                    {
                        RTC_LOG(LS_ERROR) << "cannot parse H264 sps";
                    } else {
                        RTC_LOG(LS_ERROR) << "sps " << sps->width << "x" << sps->height;
                        RTC_LOG(LS_INFO) << "RtmpVideoSource:onData H264 SPS set format " << sps->width << "x" << sps->height;
                        postFormat("H264", sps->width, sps->height);
                        
                        m_cfg.insert(m_cfg.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        m_cfg.insert(m_cfg.end(), &body[start_sps+2], &body[start_sps+2 + spssize + 1]);

                        int start_pps = start_sps + spssize + 3;
                        int ppssize = (body[start_pps]<<8) + body[start_pps+1];
                        nalu_type = webrtc::H264::ParseNaluType(body[start_pps+2]);
                        RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 NALU type:" << nalu_type;
                        RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 PPS size:" << ppssize;

                        m_cfg.insert(m_cfg.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        m_cfg.insert(m_cfg.end(), &body[start_pps+2], &body[start_pps + 2  + ppssize + 1]);
                    }                                
                } 
            } else if (frameType == 1 && body[1] == 1) {
                webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(body[9]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 IDR NALU type:" << nalu_type;

                std::vector<uint8_t> content;
                content.insert(content.end(), m_cfg.begin(), m_cfg.end());
                content.insert(content.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                content.insert(content.end(), &body[9], &body[size]);
                rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(content.data(), content.size());
                PostFrame(frame, ts, webrtc::VideoFrameType::kVideoFrameKey);
            }
            else if (frameType == 2) {
                webrtc::H264::NaluType nalu_type = webrtc::H264::ParseNaluType(body[9]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H264 Slice NALU type:" << nalu_type;                            
                std::vector<uint8_t> content;
                content.insert(content.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                content.insert(content.end(), &body[9], &body[size]);
                rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(content.data(), content.size());
                PostFrame(frame, ts, webrtc::VideoFrameType::kVideoFrameDelta);
            }
        }
        else if (codecId == CODEC_ID_HEVC) {
            if (frameType == 1 && body[1] == 0) {
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 VPS/SPS/PPS";
            
                int start_vps = 31;
                int vpssize = (body[start_vps]<<8) + body[start_vps+1];
                webrtc::H265::NaluType nalu_type = webrtc::H265::ParseNaluType(body[start_vps+2]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 NALU type:" << nalu_type;
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 VPS size:" << vpssize;
                if (nalu_type == webrtc::H265::NaluType::kVps)
                {
                    m_cfg.clear();
                    m_cfg.insert(m_cfg.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                    m_cfg.insert(m_cfg.end(), &body[start_vps+2], &body[start_vps+2 + vpssize + 1]);

                    int start_sps = start_vps + vpssize + 3 + 2;
                    int spssize = (body[start_sps]<<8) + body[start_sps+1];
                    nalu_type = webrtc::H265::ParseNaluType(body[start_sps+2]);
                    RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 NALU type:" << nalu_type;
                    RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 SPS size:" << spssize;
                    if (nalu_type == webrtc::H265::NaluType::kSps)
                    {
                        absl::optional<webrtc::H265SpsParser::SpsState> sps = webrtc::H265SpsParser::ParseSps((const unsigned char*)(&body[start_sps+3]), spssize);
                        if (!sps)
                        {
                            RTC_LOG(LS_ERROR) << "cannot parse H265 sps";
                        } else {
                            RTC_LOG(LS_ERROR) << "sps " << sps->width << "x" << sps->height;
                            RTC_LOG(LS_INFO) << "RtmpVideoSource:onData H265 SPS set format " << sps->width << "x" << sps->height;
                            postFormat("H265", sps->width, sps->height);
                        }                               
                        m_cfg.insert(m_cfg.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        m_cfg.insert(m_cfg.end(), &body[start_sps+2], &body[start_sps+2  + spssize + 1]);

                        int start_pps = start_sps + spssize + 3 + 2;
                        int ppssize = (body[start_pps]<<8) + body[start_pps+1];
                        nalu_type = webrtc::H265::ParseNaluType(body[start_pps+2]);
                        RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 NALU type:" << nalu_type;
                        RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 PPS size:" << ppssize;

                        m_cfg.insert(m_cfg.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        m_cfg.insert(m_cfg.end(), &body[start_pps+2], &body[start_pps+2  + ppssize + 1]);
                    } 
                } 
            } else if (frameType == 1 && body[1] == 1) {
                webrtc::H265::NaluType nalu_type = webrtc::H265::ParseNaluType(body[9]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 IDR type:" << nalu_type;

                std::vector<uint8_t> content;
                content.insert(content.end(), m_cfg.begin(), m_cfg.end());
                content.insert(content.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                content.insert(content.end(), &body[9], &body[size]);
                rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(content.data(), content.size());
                PostFrame(frame, ts, webrtc::VideoFrameType::kVideoFrameKey);
            }
            else if (frameType == 2) {
                webrtc::H265::NaluType nalu_type = webrtc::H265::ParseNaluType(body[9]);
                RTC_LOG(LS_INFO) << "RtmpVideoSource::onNewSession H265 Slice NALU type:" << nalu_type;                            
                std::vector<uint8_t> content;
                content.insert(content.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                content.insert(content.end(), &body[9], &body[size]);
                rtc::scoped_refptr<webrtc::EncodedImageBuffer> frame = webrtc::EncodedImageBuffer::Create(content.data(), content.size());
                PostFrame(frame, ts, webrtc::VideoFrameType::kVideoFrameDelta);
            }            
        }
    }

protected:
    RTMP m_rtmp;
    std::string m_url;

private:
    std::thread m_capturethread;
    std::vector<uint8_t> m_cfg;
};
