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
#include "api/environment/environment_factory.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/h264_sprop_parameter_sets.h"
#include "rtc_base/third_party/base64/base64.h"

#include "SessionSink.h"
#include "VideoScaler.h"

class VideoDecoder : public rtc::VideoSourceInterface<webrtc::VideoFrame>, public webrtc::DecodedImageCallback {
    private:
        class Frame
        {
            public:
                Frame(): m_timestamp_ms(0) {}
                Frame(const rtc::scoped_refptr<webrtc::EncodedImageBuffer> & content, uint64_t timestamp_ms, webrtc::VideoFrameType frameType) : m_content(content), m_timestamp_ms(timestamp_ms), m_frameType(frameType) {}
                Frame(const std::string & format, int width, int height) : m_format(format), m_width(width), m_height(height) {}
            
                rtc::scoped_refptr<webrtc::EncodedImageBuffer>   m_content;
                uint64_t                                         m_timestamp_ms;
                webrtc::VideoFrameType                           m_frameType;
                std::string                                      m_format;
                int                                              m_width;
                int                                              m_height;
        };

    public:
        VideoDecoder(const std::map<std::string,std::string> & opts, std::unique_ptr<webrtc::VideoDecoderFactory>& videoDecoderFactory, bool wait = false) : 
                m_scaler(opts),
                m_env(webrtc::CreateEnvironment()),
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

        static std::vector<uint8_t> extractParameters(const std::string & buffer)
        {
            std::vector<uint8_t> binary;
            std::string value(buffer);
            size_t pos = value.find_first_of(" ;\r\n");
            if (pos != std::string::npos)
            {
                value.erase(pos);
            }
            rtc::Base64::DecodeFromArray(value.data(), value.size(), rtc::Base64::DO_STRICT, &binary, nullptr);
            binary.insert(binary.begin(), H26X_marker, H26X_marker+sizeof(H26X_marker));

            return binary;
        }


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
            } else if (codec == "H265") {
                const char* pattern="sprop-vps=";
                const char* spropvps=strstr(sdp, pattern);
                const char* spropsps=strstr(sdp, "sprop-sps=");
                const char* sproppps=strstr(sdp, "sprop-pps=");
                if (spropvps && spropsps && sproppps)
                {
                    std::string vpsstr(spropvps+strlen(pattern));
                    std::vector<uint8_t> vps = extractParameters(vpsstr);
                    frames.push_back(vps);

                    std::string spsstr(spropsps+strlen(pattern));
                    std::vector<uint8_t> sps = extractParameters(spsstr);
                    frames.push_back(sps);

                    std::string ppsstr(sproppps+strlen(pattern));
                    std::vector<uint8_t> pps = extractParameters(ppsstr);
                    frames.push_back(pps);
                }
            }


            return frames;
        }

        void postFormat(const std::string & format, int width, int height) {
            Frame frame(format, width, height);
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

        void createDecoder(const std::string & format, int width, int height) {
            webrtc::VideoDecoder::Settings settings;
            webrtc::RenderResolution resolution(width, height);
            settings.set_max_render_resolution(resolution);
            if (format == "H264") {
                m_decoder=m_factory->Create(m_env,webrtc::SdpVideoFormat(cricket::kH264CodecName));
                settings.set_codec_type(webrtc::VideoCodecType::kVideoCodecH264);
            } else if (format == "H265") {
                m_decoder=m_factory->Create(m_env,webrtc::SdpVideoFormat(format));
                settings.set_codec_type(webrtc::VideoCodecType::kVideoCodecH265);
            } else if (format == "VP9") {
                m_decoder=m_factory->Create(m_env,webrtc::SdpVideoFormat(cricket::kVp9CodecName));
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

                if (!frame.m_format.empty()) {

                    if (this->hasDecoder()) {
                        if ((m_format != frame.m_format) || (m_width != frame.m_width) || (m_height != frame.m_height)) {
                            RTC_LOG(LS_INFO) << "format changed => set format from " << m_format << " " << m_width << "x" << m_height << " to " << frame.m_format << " " << frame.m_width << "x" << frame.m_height;
                            m_decoder.reset(NULL);
                        }
                    }

                    if (!this->hasDecoder()) {
                        RTC_LOG(LS_INFO) << "VideoDecoder:DecoderThread set format:" << frame.m_format << " " << frame.m_width << "x" << frame.m_height;
                        m_format = frame.m_format;
                        m_width = frame.m_width;
                        m_height = frame.m_height;

                        this->createDecoder(frame.m_format, frame.m_width, frame.m_height);
                    }
                }                

                if (frame.m_content.get() != NULL) {
                    RTC_LOG(LS_VERBOSE) << "VideoDecoder::DecoderThread size:" << frame.m_content->size() << " ts:" << frame.m_timestamp_ms;
                    ssize_t size = frame.m_content->size();
                    
                    if (size) {
                        webrtc::EncodedImage input_image;
                        input_image.SetEncodedData(frame.m_content);		
                        input_image.SetFrameType(frame.m_frameType);
                        input_image.ntp_time_ms_ = frame.m_timestamp_ms;
                        input_image.SetRtpTimestamp(frame.m_timestamp_ms); // store time in ms that overflow the 32bits

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
        const webrtc::Environment                     m_env;
        std::unique_ptr<webrtc::VideoDecoderFactory>& m_factory;

        std::string                          m_format;
        int                                  m_width;
        int                                  m_height;

		std::queue<Frame>                     m_queue;
		std::mutex                            m_queuemutex;
		std::condition_variable               m_queuecond;
		std::thread                           m_decoderthread;     
        bool                                  m_stop;   

        bool                                  m_wait;
        int64_t                               m_previmagets;	
        int64_t                               m_prevts;

};
