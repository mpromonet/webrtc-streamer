/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** decoder.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "api/video/i420_buffer.h"
#include "modules/video_coding/include/video_error_codes.h"

class Decoder : public webrtc::DecodedImageCallback {
    private:
        class Frame
        {
            public:
                Frame(): m_timestamp_ms(0) {}
                Frame(std::vector<uint8_t> && content, uint64_t timestamp_ms, webrtc::VideoFrameType frameType) : m_content(content), m_timestamp_ms(timestamp_ms), m_frameType(frameType) {}
            
                std::vector<uint8_t>   m_content;
                uint64_t               m_timestamp_ms;
                webrtc::VideoFrameType m_frameType;
        };

    public:
        Decoder(rtc::VideoBroadcaster& broadcaster, const std::map<std::string,std::string> & opts, bool wait) : 
                m_broadcaster(broadcaster),
                m_width(0), m_height(0), 
                m_roi_x(0), m_roi_y(0), m_roi_width(0), m_roi_height(0),
                m_stop(false),
                m_wait(wait),
                m_previmagets(0),
                m_prevts(0) {

            if (opts.find("width") != opts.end()) {
                m_width = std::stoi(opts.at("width"));
            }
            if (opts.find("height") != opts.end()) {
                m_height = std::stoi(opts.at("height"));
            }
            if (opts.find("roi_x") != opts.end()) {
                m_roi_x = std::stoi(opts.at("roi_x"));
            if (m_roi_x<0) {
                    RTC_LOG(LS_ERROR) << "Ignore roi_x=" << m_roi_x << ", it muss be >=0";
                    m_roi_x = 0;
                }
            }	
            if (opts.find("roi_y") != opts.end()) {
                m_roi_y = std::stoi(opts.at("roi_y"));
                if (m_roi_y<0) {
                    RTC_LOG(LS_ERROR) << "Ignore roi_<=" << m_roi_y << ", it muss be >=0";
                    m_roi_y = 0;
                }
            }	
            if (opts.find("roi_width") != opts.end()) {
                m_roi_width = std::stoi(opts.at("roi_width"));
                if (m_roi_width<=0) {
                    RTC_LOG(LS_ERROR) << "Ignore roi_width<=" << m_roi_width << ", it muss be >0";
                    m_roi_width = 0;
                }
            }	
            if (opts.find("roi_height") != opts.end()) {
                m_roi_height = std::stoi(opts.at("roi_height"));
                if (m_roi_height<=0) {
                    RTC_LOG(LS_ERROR) << "Ignore roi_height<=" << m_roi_height << ", it muss be >0";
                    m_roi_height = 0;
                }
            }
        }

        virtual ~Decoder() {
        }

        void DecoderThread() 
        {
            while (!m_stop) {
                std::unique_lock<std::mutex> mlock(m_queuemutex);
                while (m_queue.empty())
                {
                    m_queuecond.wait(mlock);
                }
                Frame frame = m_queue.front();
                m_queue.pop();		
                mlock.unlock();
                
                RTC_LOG(LS_VERBOSE) << "Decoder:DecoderThread size:" << frame.m_content.size() << " ts:" << frame.m_timestamp_ms;
                uint8_t* data = frame.m_content.data();
                ssize_t size = frame.m_content.size();
                
                if (size) {
                    webrtc::EncodedImage input_image(data, size, size);		
                    input_image._frameType = frame.m_frameType;
                    input_image._completeFrame = true;			
                    input_image.SetTimestamp(frame.m_timestamp_ms); // store time in ms that overflow the 32bits
                    int res = m_decoder->Decode(input_image, false, frame.m_timestamp_ms);
                    if (res != WEBRTC_VIDEO_CODEC_OK) {
                        RTC_LOG(LS_ERROR) << "Decoder::DecoderThread failure:" << res;
                    }
                }
            }
        }

        void Start()
        {
            RTC_LOG(INFO) << "Decoder::start";
            m_stop = false;
            m_decoderthread = std::thread(&Decoder::DecoderThread, this);
        }

        void Stop()
        {
            RTC_LOG(INFO) << "Decoder::stop";
            m_stop = true;
            Frame frame;			
            {
                std::unique_lock<std::mutex> lock(m_queuemutex);
                m_queue.push(frame);
            }
            m_queuecond.notify_all();
            m_decoderthread.join();
        }

        void createDecoder(const std::string & codec) {
            if (codec == "H264") {
                    m_decoder=m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kH264CodecName));
                    webrtc::VideoCodec codec_settings;
                    codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecH264;
                    m_decoder->InitDecode(&codec_settings,2);
                    m_decoder->RegisterDecodeCompleteCallback(this);
            } else if (codec == "VP9") {
                m_decoder=m_factory.CreateVideoDecoder(webrtc::SdpVideoFormat(cricket::kVp9CodecName));
                webrtc::VideoCodec codec_settings;
                codec_settings.codecType = webrtc::VideoCodecType::kVideoCodecVP9;
                m_decoder->InitDecode(&codec_settings,2);
                m_decoder->RegisterDecodeCompleteCallback(this);	                
            }
        }
        void destroyDecoder() {
            m_decoder.reset(NULL);
        }

        bool hasDecoder() {
            return (m_decoder.get() != NULL);
        }

        void PostFrame(std::vector<uint8_t>&& content, uint64_t ts, webrtc::VideoFrameType frameType) {
			Frame frame(std::move(content), ts, frameType);			
			{
				std::unique_lock<std::mutex> lock(m_queuemutex);
				m_queue.push(frame);
			}
			m_queuecond.notify_all();
        }

		// overide webrtc::DecodedImageCallback
		virtual int32_t Decoded(webrtc::VideoFrame& decodedImage) override {
            int64_t ts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;

            RTC_LOG(LS_VERBOSE) << "Decoder::Decoded size:" << decodedImage.size() 
                        << " decode ts:" << decodedImage.ntp_time_ms()
                        << " source ts:" << ts;

            // waiting 
            if ( (m_wait) && (m_prevts != 0) ) {
                int64_t periodSource = decodedImage.timestamp() - m_previmagets;
                int64_t periodDecode = ts-m_prevts;
                    
                RTC_LOG(LS_VERBOSE) << "Decoder::Decoded interframe decode:" << periodDecode << " source:" << periodSource;
                int64_t delayms = periodSource-periodDecode;
                if ( (delayms > 0) && (delayms < 1000) ) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(delayms));			
                }
            }                        

            if(m_roi_x >= decodedImage.width()) {
                RTC_LOG(LS_ERROR) << "The ROI position protrudes beyond the right edge of the image. Ignore roi_x.";
                m_roi_x = 0;
            }
            if(m_roi_y >= decodedImage.height()) {
                RTC_LOG(LS_ERROR) << "The ROI position protrudes beyond the bottom edge of the image. Ignore roi_y.";
                m_roi_y = 0;
            }
            if(m_roi_width != 0 && (m_roi_width + m_roi_x) > decodedImage.width()) {
                RTC_LOG(LS_ERROR) << "The ROI protrudes beyond the right edge of the image. Ignore roi_width.";
                m_roi_width = 0;
            }
            if(m_roi_height != 0 && (m_roi_height + m_roi_y) > decodedImage.height()) {
                RTC_LOG(LS_ERROR) << "The ROI protrudes beyond the bottom edge of the image. Ignore roi_height.";
                m_roi_height = 0;
            }
            
            if(m_roi_width == 0) {
                m_roi_width = decodedImage.width() - m_roi_x;
            }
            if(m_roi_height == 0) {
                m_roi_height = decodedImage.height() - m_roi_y;
            }

            // source image is croped but destination image size is not set
            if((m_roi_width != decodedImage.width() || m_roi_height != decodedImage.height()) && (m_height == 0 && m_width == 0)) {
                m_height = m_roi_height;
                m_width = m_roi_width;
            }

            if ( (m_height == 0) && (m_width == 0) ) {
                m_broadcaster.OnFrame(decodedImage);
            } else {
                int height = m_height;
                int width = m_width;
                if (height == 0) {
                    height = (m_roi_height * width) / m_roi_width;
                }
                else if (width == 0) {
                    width = (m_roi_width * height) / m_roi_height;
                }
                int stride_y = width;
                int stride_uv = (width + 1) / 2;
                rtc::scoped_refptr<webrtc::I420Buffer> scaled_buffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);
                if(m_roi_width != decodedImage.width() || m_roi_height != decodedImage.height()) {
                    scaled_buffer->CropAndScaleFrom(*decodedImage.video_frame_buffer()->ToI420(), m_roi_x, m_roi_y, m_roi_width, m_roi_height);
                } else {
                    scaled_buffer->ScaleFrom(*decodedImage.video_frame_buffer()->ToI420());
                }
                webrtc::VideoFrame frame = webrtc::VideoFrame(scaled_buffer, decodedImage.timestamp(),
                    decodedImage.render_time_ms(), webrtc::kVideoRotation_0);
                        
                m_broadcaster.OnFrame(frame);
            }     

	        m_previmagets = decodedImage.timestamp();
	        m_prevts = std::chrono::high_resolution_clock::now().time_since_epoch().count()/1000/1000;
                       
            return 1;
        }

        rtc::VideoBroadcaster&                m_broadcaster;
        webrtc::InternalDecoderFactory        m_factory;
        std::unique_ptr<webrtc::VideoDecoder> m_decoder;

		int                                   m_width;
		int                                   m_height;
		int                                   m_roi_x;
		int                                   m_roi_y;
		int                                   m_roi_width;
		int                                   m_roi_height;

		std::queue<Frame>                     m_queue;
		std::mutex                            m_queuemutex;
		std::condition_variable               m_queuecond;
		std::thread                           m_decoderthread;     
        bool                                  m_stop;   

        bool                                  m_wait;
        int64_t                               m_previmagets;	
        int64_t                               m_prevts;

};
