/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VideoScaler.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "media/base/video_broadcaster.h"
#include "api/media_stream_interface.h"
#include "api/video/i420_buffer.h"

#include "VideoSource.h"

class VideoScaler :  public webrtc::VideoSinkInterface<webrtc::VideoFrame>,  public VideoSource 
{
public:

    VideoScaler(const std::map<std::string, std::string> &opts) :
                m_width(0), m_height(0), 
                m_rotation(webrtc::kVideoRotation_0),
                m_roi_x(0), m_roi_y(0), m_roi_width(0), m_roi_height(0) 
    {
        if (opts.find("width") != opts.end())
        {
            m_width = std::stoi(opts.at("width"));
        }
        if (opts.find("height") != opts.end())
        {
            m_height = std::stoi(opts.at("height"));
        }
        if (opts.find("rotation") != opts.end())
        {
            int rotation = std::stoi(opts.at("rotation"));
            switch (rotation) {
                case 90: m_rotation = webrtc::kVideoRotation_90; break;
                case 180: m_rotation = webrtc::kVideoRotation_180; break;
                case 270: m_rotation = webrtc::kVideoRotation_270; break;
            }
        }        
        if (opts.find("roi_x") != opts.end())
        {
            m_roi_x = std::stoi(opts.at("roi_x"));
            if (m_roi_x < 0)
            {
                RTC_LOG(LS_ERROR) << "Ignore roi_x=" << m_roi_x << ", it muss be >=0";
                m_roi_x = 0;
            }
        }
        if (opts.find("roi_y") != opts.end())
        {
            m_roi_y = std::stoi(opts.at("roi_y"));
            if (m_roi_y < 0)
            {
                RTC_LOG(LS_ERROR) << "Ignore roi_<=" << m_roi_y << ", it muss be >=0";
                m_roi_y = 0;
            }
        }
        if (opts.find("roi_width") != opts.end())
        {
            m_roi_width = std::stoi(opts.at("roi_width"));
            if (m_roi_width <= 0)
            {
                RTC_LOG(LS_ERROR) << "Ignore roi_width<=" << m_roi_width << ", it muss be >0";
                m_roi_width = 0;
            }
        }
        if (opts.find("roi_height") != opts.end())
        {
            m_roi_height = std::stoi(opts.at("roi_height"));
            if (m_roi_height <= 0)
            {
                RTC_LOG(LS_ERROR) << "Ignore roi_height<=" << m_roi_height << ", it muss be >0";
                m_roi_height = 0;
            }
        }
    }

    virtual ~VideoScaler()
    {
    }

    webrtc::scoped_refptr<webrtc::I420Buffer> createOutputBuffer() {
        int height = m_height;
        int width = m_width;
        if ( (height == 0) && (width == 0) )
        {
            height = m_roi_height;
            width = m_roi_width;
        }
        else if (height == 0)
        {
            height = (m_roi_height * width) / m_roi_width;
        }
        else if (width == 0)
        {
            width = (m_roi_width * height) / m_roi_height;
        }
        return webrtc::I420Buffer::Create(width, height);
    }

    void OnFrame(const webrtc::VideoFrame &frame) override
    {
        if ((m_roi_x != 0) && (m_roi_x >= frame.width()))
        {
            RTC_LOG(LS_WARNING) << "The ROI position protrudes beyond the right edge of the image. Ignore roi_x.";
            m_roi_x = 0;
        }
        if ((m_roi_y !=0) && (m_roi_y >= frame.height()))
        {
            RTC_LOG(LS_WARNING) << "The ROI position protrudes beyond the bottom edge of the image. Ignore roi_y.";
            m_roi_y = 0;
        }
        if ((m_roi_width != 0) && ((m_roi_width + m_roi_x) > frame.width()))
        {
            RTC_LOG(LS_WARNING) << "The ROI protrudes beyond the right edge of the image. Ignore roi_width.";
            m_roi_width = 0;
        }
        if ((m_roi_height != 0) && ((m_roi_height + m_roi_y) > frame.height()))
        {
            RTC_LOG(LS_WARNING) << "The ROI protrudes beyond the bottom edge of the image. Ignore roi_height.";
            m_roi_height = 0;
        }

        if (m_roi_width == 0)
        {
            m_roi_width = frame.width() - m_roi_x;
        }
        if (m_roi_height == 0)
        {
            m_roi_height = frame.height() - m_roi_y;
        }

        if ( ((m_roi_width != frame.width()) && (m_roi_height == frame.height()) && (m_rotation == webrtc::kVideoRotation_0)) || (frame.video_frame_buffer()->type() == webrtc::VideoFrameBuffer::Type::kNative) )
        {
            m_broadcaster.OnFrame(frame);
        }
        else
        {
            webrtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer = this->createOutputBuffer();
            if (m_roi_width != frame.width() || m_roi_height != frame.height())
            {
                RTC_LOG(LS_VERBOSE) << "crop:" << m_roi_x << "x" << m_roi_y << " " << m_roi_width << "x" << m_roi_height << " scale: " << scaled_buffer->width() << "x" << scaled_buffer->height();
                scaled_buffer->CropAndScaleFrom(*frame.video_frame_buffer()->ToI420(), m_roi_x, m_roi_y, m_roi_width, m_roi_height);
            }
            else
            {
                RTC_LOG(LS_VERBOSE) << "scale: " << scaled_buffer->width() << "x" << scaled_buffer->height();
                scaled_buffer->ScaleFrom(*frame.video_frame_buffer()->ToI420());
            }
            
            webrtc::VideoFrame scaledFrame = webrtc::VideoFrame::Builder()
                .set_video_frame_buffer(scaled_buffer)
                .set_rotation(m_rotation)
                .set_timestamp_rtp(frame.rtp_timestamp())
                .set_timestamp_us(frame.timestamp_us())
                .set_id(frame.timestamp_us())
                .build();

            m_broadcaster.OnFrame(scaledFrame);
        }
    }

    int width() { return m_roi_width;  }
    int height() { return m_roi_height;  }

private:
    int                    m_width;
    int                    m_height;
    webrtc::VideoRotation  m_rotation;
    int                    m_roi_x;
    int                    m_roi_y;
    int                    m_roi_width;
    int                    m_roi_height;    
};

