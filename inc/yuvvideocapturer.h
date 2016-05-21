/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** yuvvideocapturer.h
** 
** -------------------------------------------------------------------------*/

#ifndef YUVVIDEOCAPTURER_H_
#define YUVVIDEOCAPTURER_H_

#include "webrtc/base/thread.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/asyncinvoker.h"
#include "webrtc/media/base/yuvframegenerator.h"

class YuvVideoCapturer : public cricket::VideoCapturer, public rtc::Thread
{
	public:
		YuvVideoCapturer() : 
				frame_generator_(NULL),
				width_(640),
				height_(480), frame_index_(0)
		{
			std::cout << "===========================YuvVideoCapturer" << std::endl;
			SetCaptureFormat(NULL);
			
			int size = width_ * height_;
			int qsize = size / 4;
			frame_generator_ = new cricket::YuvFrameGenerator(width_, height_, true);
			frame_data_size_ = size + 2 * qsize;
			captured_frame_.data = new char[frame_data_size_];
			captured_frame_.fourcc = cricket::FOURCC_IYUV;
			captured_frame_.pixel_height = 1;
			captured_frame_.pixel_width = 1;
			captured_frame_.width = width_;
			captured_frame_.height = height_;
			captured_frame_.data_size = frame_data_size_;
			
			std::vector<cricket::VideoFormat> formats;
			formats.push_back(cricket::VideoFormat(width_, height_, cricket::VideoFormat::FpsToInterval(25), cricket::FOURCC_IYUV));
			SetSupportedFormats(formats);
		}
	  
		virtual ~YuvVideoCapturer() 
		{
			Stop();
		}
		

		void SignalFrameCapturedOnStartThread() 
		{
			std::cout << "===========================YuvVideoCapturer::SignalFrameCapturedOnStartThread" << std::endl;
			SignalFrameCaptured(this, &captured_frame_);
			std::cout << "===========================YuvVideoCapturer::SignalFrameCapturedOnStartThread" << std::endl;			
		}
		
		void Run()
		{
			std::cout << "===========================YuvVideoCapturer::mainloop" << std::endl;
			while(IsRunning())
			{
				uint8_t* buffer = new uint8_t[frame_data_size_];
				frame_generator_->GenerateNextFrame(buffer, frame_index_);
				frame_index_++;
				memmove(captured_frame_.data, buffer, frame_data_size_);
				delete[] buffer;			
				
				async_invoker_->AsyncInvoke<void>(
					start_thread_,
					rtc::Bind(&YuvVideoCapturer::SignalFrameCapturedOnStartThread, this));
				
				ProcessMessages(40);
			}
		}
				
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format) 
		{
			std::cout << "===========================YuvVideoCapturer::Start" << std::endl;
			start_thread_ = rtc::Thread::Current();
			async_invoker_.reset(new rtc::AsyncInvoker());
			
			SetCaptureFormat(&format);
			SetCaptureState(cricket::CS_RUNNING);
			rtc::Thread::Start();
			return cricket::CS_RUNNING;
		}
	  
		virtual void Stop() 
		{
			rtc::Thread::Stop();
			async_invoker_.reset();
			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CS_STOPPED);
		}
	  
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) 
		{
			fourccs->push_back(cricket::FOURCC_IYUV);
			return true;
		}
	  
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	  
	private:
		int width_;
		int height_;
		int frame_data_size_;
		cricket::YuvFrameGenerator* frame_generator_;
		cricket::CapturedFrame captured_frame_;
		int frame_index_;
		rtc::Thread* start_thread_;
		rtc::scoped_ptr<rtc::AsyncInvoker> async_invoker_;
};

#endif  
