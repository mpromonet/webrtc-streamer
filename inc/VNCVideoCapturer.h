
#ifndef VNCVIDEOCAPTURER_H_
#define VNCVIDEOCAPTURER_H_

#include <rfb/rfbclient.h>
#include <string>
#include "url.h"
#include "rtc_base/thread.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"

class VNCVideoCapturer : public cricket::VideoCapturer, public rtc::Thread {
	public:
		VNCVideoCapturer(const std::string uri);
		virtual ~VNCVideoCapturer();

		// overide rtc::Thread
		virtual void Run();

		// overide cricket::VideoCapturer
		virtual cricket::CaptureState Start(const cricket::VideoFormat& format);
		virtual void Stop();
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs);
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; };

		// libvncclient callbacks
		char* onGetPassword();
		void onFrameBufferUpdate();

		// internal
		bool onStart();
		void onError(std::string error);
		void onClick(int x, int y, int button);
		void onPress(unsigned int code, bool down);
	private:
		std::string uri;
		Url url;
		rfbClient* client;
};


#endif
