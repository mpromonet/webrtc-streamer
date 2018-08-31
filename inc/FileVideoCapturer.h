/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** FileVideoCapturer.h
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
#include "rtspconnectionclient.h"

#include "rtc_base/thread.h"
#include "media/base/codec.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"
#include "api/video_codecs/video_decoder.h"
#define ONE_FRAME_BUF_MAX 512 * 1024


class FileVideoCapturer : public cricket::VideoCapturer, public rtc::Thread, public webrtc::DecodedImageCallback
{
	public:
		FileVideoCapturer(const std::string & uri, const std::map<std::string,std::string> & opts);
		virtual ~FileVideoCapturer();
		unsigned char *Buf;
		unsigned char OneFrameBuf[ONE_FRAME_BUF_MAX];

		int FileSize;

		// overide webrtc::DecodedImageCallback
		virtual int32_t Decoded(webrtc::VideoFrame& decodedImage);

		// overide rtc::Thread
		virtual void Run();

		// overide cricket::VideoCapturer
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format);
		virtual void Stop();
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs);
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
		
		void DecoderThread();


	private:
		webrtc::InternalDecoderFactory        m_factory;
		std::unique_ptr<webrtc::VideoDecoder> m_decoder;
		std::vector<uint8_t>                  m_cfg;
		std::string                           m_codec;
		std::thread                           m_decoderthread;
};


