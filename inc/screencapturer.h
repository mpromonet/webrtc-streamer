/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
**
** -------------------------------------------------------------------------*/

#pragma once

#include "rtc_base/thread.h"
#include "media/base/videocapturer.h"
#include "api/video/i420_buffer.h"

#include "libyuv/video_common.h"
#include "libyuv/convert.h"

#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_capture_options.h"


class ScreenCapturer : public cricket::VideoCapturer, public rtc::Thread, public webrtc::DesktopCapturer::Callback 
{
	public:
		ScreenCapturer() {
			m_capturer = webrtc::DesktopCapturer::CreateScreenCapturer(webrtc::DesktopCaptureOptions::CreateDefault());
		}
		virtual ~ScreenCapturer() {}


		// overide webrtc::DesktopCapturer::Callback
		virtual void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) {
			
			if (result == webrtc::DesktopCapturer::Result::SUCCESS) {
				int width = frame->rect().width();
				int height = frame->rect().height();
				rtc::scoped_refptr<webrtc::I420Buffer> I420buffer = webrtc::I420Buffer::Create(width, height);

				const int conversionResult = libyuv::ConvertToI420(frame->data(), frame->stride()*webrtc::DesktopFrame::kBytesPerPixel,
					(uint8*)I420buffer->DataY(), I420buffer->StrideY(),
					(uint8*)I420buffer->DataU(), I420buffer->StrideU(),
					(uint8*)I420buffer->DataV(), I420buffer->StrideV(),
					0, 0,
					width, height,
					width, height,
					libyuv::kRotate0, ::libyuv::FOURCC_ARGB);									
						
				if (conversionResult >= 0) {
					webrtc::VideoFrame videoFrame(I420buffer, webrtc::VideoRotation::kVideoRotation_0, rtc::TimeMicros());
					this->OnFrame(videoFrame, width, height);
				} else {
					RTC_LOG(LS_ERROR) << "ScreenCapturer:OnCaptureResult conversion error:" << conversionResult;
				}				

			} else {
				RTC_LOG(LS_ERROR) << "ScreenCapturer:OnCaptureResult capture error:" << result;
			}

		}
		
		// overide rtc::Thread
		virtual void Run() {
			while (IsRunning()) {
				m_capturer->CaptureFrame();
			}
		}

		// overide cricket::VideoCapturer
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format) {
			SetCaptureFormat(&format);
			SetCaptureState(cricket::CS_RUNNING);
			rtc::Thread::Start();
			m_capturer->Start(this);
			return cricket::CS_RUNNING;
		}
		
		virtual void Stop() {
			rtc::Thread::Stop();
			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CS_STOPPED);			
		}
		
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) { return true; }
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	
	private:
		std::unique_ptr<webrtc::DesktopCapturer> m_capturer;
		
};


