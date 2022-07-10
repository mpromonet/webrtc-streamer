/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** VideoDecoder.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include <string.h>
#include <vector>

#include "api/video/i420_buffer.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"

#include "VideoScaler.h"

#define FOURCC(a, b, c, d)                                \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

const uint32_t FOURCC_VP9 = FOURCC('V','P','9', 0);
#undef FOURCC

class VideoDecoder : public rtc::VideoSourceInterface<webrtc::VideoFrame>, public webrtc::DecodedImageCallback {
    private:
        class Frame
        {
            public:
                Frame(): m_timestamp_ms(0) {}
                Frame(const rtc::scoped_refptr<webrtc::EncodedImageBuffer> & content, uint64_t timestamp_ms, webrtc::VideoFrameType frameType) : m_content(content), m_timestamp_ms(timestamp_ms), m_frameType(frameType) {}
                Frame(const cricket::VideoFormat & format) : m_format(format) {}
            
                rtc::scoped_refptr<webrtc::EncodedImageBuffer>   m_content;
                uint64_t               m_timestamp_ms;
                webrtc::VideoFrameType m_frameType;
                cricket::VideoFormat   m_format;
        };

    public:
        VideoDecoder(const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, bool wait = false) : 
                m_scaler(opts),
                m_factory(videoDecoderFactory),
                m_stop(false),
                m_wait(wait),
                m_previmagets(0),
                m_prevts(0) {
            this->Start();                    
        }

        virtual ~VideoDecoder() {
            this->Stop();                    
        }

        int width() { return m_scaler.width();  }
        int height() { return m_scaler.height();  }

        std::vector< std::vector<uint8_t> > getInitFrames(const std::string & codec, const char* sdp) {
            std::vector< std::vector<uint8_t> > frames;

            if (codec == "H264") {
                const char* pattern="sprop-parameter-sets=";
                const char* sprop=strstr(sdp, pattern);
                if (sprop)
                {
                    std::string sdpstr(sprop+strlen(pattern));
                    size_t pos = sdpstr.find_first_of(" ;\r\n");
                    if (pos != std::string::npos)
                    {
                        sdpstr.erase(pos);
                    }
                    webrtc::H264SpropParameterSets sprops;
                    if (sprops.DecodeSprop(sdpstr))
                    {
                        std::vector<uint8_t> sps;
                        sps.insert(sps.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        sps.insert(sps.end(), sprops.sps_nalu().begin(), sprops.sps_nalu().end());
                        frames.push_back(sps);

                        std::vector<uint8_t> pps;
                        pps.insert(pps.end(), H26X_marker, H26X_marker+sizeof(H26X_marker));
                        pps.insert(pps.end(), sprops.pps_nalu().begin(), sprops.pps_nalu().end());
                        frames.push_back(pps);
                    }
                    else
                    {
                        RTC_LOG(LS_WARNING) << "Cannot decode SPS:" << sprop;
                    }
                }
            }

            return frames;
        }

        void postFormat(const cricket::VideoFormat & format) {
			Frame frame(format);			
			{
				std::unique_lock<std::mutex> lock(m_queuemutex);
				m_queue.push(frame);
			}
			m_queuecond.notify_all();
        }

        void PostFrame(const rtc::scoped_refptr<webrtc::EncodedImageBuffer>& content, uint64_t ts, webrtc::VideoFrameType frameType) {
			Frame frame(content, ts, frameType);			
			{
				std::unique_lock<std::mutex> lock(m_queuemutex);
				m_queue.push(frame);
			}
			m_queuecond.notify_all();
        }

		// overide webrtc::DecodedImageCallback
	    virtual int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
            int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;

            RTC_LOG(LS_VERBOSE) << "VideoDecoder::Decoded size:" << decodedImage.size() 
                        << " decode ts:" << decodedImage.ntp_time_ms()
                        << " source ts:" << ts;

            // waiting 
            if ( (m_wait) && (m_prevts != 0) ) {
                int64_t periodSource = decodedImage.timestamp() - m_previmagets;
                int64_t periodDecode = ts-m_prevts;
                    
                RTC_LOG(LS_VERBOSE) << "VideoDecoder::Decoded interframe decode:" << periodDecode << " source:" << periodSource;
                int64_t delayms = periodSource-periodDecode;
                if ( (delayms > 0) && (delayms < 1000) ) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayms));			
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }                        

            m_scaler.OnFrame(decodedImage);

	        m_previmagets = decodedImage.timestamp();
	        m_prevts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
                       
            return 1;
        }

        void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink, const rtc::VideoSinkWants &wants) override {
            m_scaler.AddOrUpdateSink(sink, wants);
        }

        void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink) override {
            m_scaler.RemoveSink(sink);
        }

    protected:
        bool hasDecoder() {
            return (m_decoder.get() != NULL);
        }

        void createDecoder(const cricket::VideoFormat & format) {
            webrtc::VideoDecoder::Settings settings;
            webrtc::RenderResolution resolution(format.width, format.height);
            settings.set_max_render_resolution(resolution);
            if (format.fourcc == cricket::FOURCC_H264) {
                m_decoder=m_factory->CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
                settings.set_codec_type(webrtc::VideoCodecType::kVideoCodecH264);
            } else if (format.fourcc == FOURCC_VP9) {
                m_decoder=m_factory->CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
                settings.set_codec_type(webrtc::VideoCodecType::kVideoCodecVP9);	                
            }
            if (m_decoder.get() != NULL) {
                m_decoder->Configure(settings);
                m_decoder->RegisterDecodeCompleteCallback(this);
            }
        }

        Frame getFrame() {
            std::unique_lock<std::mutex> mlock(m_queuemutex);
            while (m_queue.empty())
            {
                m_queuecond.wait(mlock);
            }
            Frame frame = m_queue.front();
            m_queue.pop();		
            
            return frame;
        }

        void DecoderThread() 
        {
            while (!m_stop) {
                Frame frame = this->getFrame();

                if (frame.m_format.fourcc != 0) {
                    cricket::VideoFormat & format = frame.m_format;

                    if (this->hasDecoder()) {
                        if ((m_format.width != format.width) || (m_format.height != format.height)) {
                            RTC_LOG(LS_INFO) << "format changed => set format from " << m_format.ToString() << " to " << format.ToString();
                            m_decoder.reset(NULL);
                        }
                    }

                    if (!this->hasDecoder()) {
                        RTC_LOG(LS_INFO) << "VideoDecoder:DecoderThread set format:" << format.ToString();
                        m_format = format;

                        this->createDecoder(format);
                    }
                }                

                if (frame.m_content.get() != NULL) {
                    RTC_LOG(LS_VERBOSE) << "VideoDecoder::DecoderThread size:" << frame.m_content->size() << " ts:" << frame.m_timestamp_ms;
                    ssize_t size = frame.m_content->size();
                    
                    if (size) {
                        webrtc::EncodedImage input_image;
                        input_image.SetEncodedData(frame.m_content);		
                        input_image._frameType = frame.m_frameType;
                        input_image.ntp_time_ms_ = frame.m_timestamp_ms;
                        input_image.SetTimestamp(frame.m_timestamp_ms); // store time in ms that overflow the 32bits

                        if (this->hasDecoder()) {
                            int res = m_decoder->Decode(input_image, false, frame.m_timestamp_ms);
                            if (res != WEBRTC_VIDEO_CODEC_OK) {
                                RTC_LOG(LS_ERROR) << "VideoDecoder::DecoderThread failure:" << res;
                            }
                        } else {
                                RTC_LOG(LS_ERROR) << "VideoDecoder::DecoderThread no decoder";
                        }
                    }
                }
            }
        }

        void Start()
        {
            RTC_LOG(LS_INFO) << "VideoDecoder::start";
            m_stop = false;
            m_decoderthread = std::thread(&VideoDecoder::DecoderThread, this);
        }

        void Stop()
        {
            RTC_LOG(LS_INFO) << "VideoDecoder::stop";
            m_stop = true;
            Frame frame;			
            {
                std::unique_lock<std::mutex> lock(m_queuemutex);
                m_queue.push(frame);
            }
            m_queuecond.notify_all();
            m_decoderthread.join();
        }

    public:
        std::unique_ptr<webrtc::VideoDecoder>         m_decoder;

    protected:
        VideoScaler                                   m_scaler;
        std::unique_ptr<webrtc::VideoDecoderFactory>& m_factory;
        cricket::VideoFormat m_format;

		std::queue<Frame>                     m_queue;
		std::mutex                            m_queuemutex;
		std::condition_variable               m_queuecond;
		std::thread                           m_decoderthread;     
        bool                                  m_stop;   

        bool                                  m_wait;
        int64_t                               m_previmagets;	
        int64_t                               m_prevts;

};

class VideoSourceWithDecoder : public rtc::VideoSourceInterface<webrtc::VideoFrame>
{
public:
	VideoSourceWithDecoder(const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, bool wait = false): m_decoder(opts, videoDecoderFactory, wait) {}

    // overide rtc::VideoSourceInterface<webrtc::VideoFrame>
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink, const rtc::VideoSinkWants &wants)
    {
        m_decoder.AddOrUpdateSink(sink, wants);
    }

    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame> *sink)
    {
        m_decoder.RemoveSink(sink);
    }    

	int width() { return m_decoder.width();  }
    int height() { return m_decoder.height();  } 

protected:
    VideoDecoder                       m_decoder;	
};
