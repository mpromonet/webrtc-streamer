#include <rfb/rfbclient.h>
#include "VNCVideoCapturer.h"

#include "rtc_base/thread.h"
#include "rtc_base/logging.h"
#include "api/video_codecs/video_decoder.h"
#include "media/base/videocapturer.h"
#include "media/engine/internaldecoderfactory.h"


char *host = "127.0.0.1";
int port = 5900;

static char * get_password (rfbClient *client) {
  VNCVideoCapturer* that = (VNCVideoCapturer *) rfbClientGetClientData(client, (void *) "this");
  return that->onGetPassword();
}

char* VNCVideoCapturer::onGetPassword() {
  if (getenv("VNCPASS") != NULL) {
    return strdup(getenv("VNCPASS"));
  }

  this->onError("No password defined");
  return NULL;
}

VNCVideoCapturer::VNCVideoCapturer(const std::string & uri): client(rfbGetClient(8,3,4)) 
{
	client->GetPassword = get_password;

	// remove vnc:// prefix from uri before passing along
	std::string url = uri.substr(6, std::string::npos);
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Trying to initialize VNC with: " << url;

	const char *args[] = { "VNCVideoCapture", url.c_str() };
	int len = 2;
	if (!rfbInitClient(client, &len, (char **) args)) {
		onError("Something went wrong in initializing RFB client ");
		return;
	}
	rfbClientSetClientData(client, (void *) "this", (void *) this);
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "Initialized VNC Successfully: " << url;
}

void VNCVideoCapturer::onError(char str[]) {
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << str;
	exit(EXIT_FAILURE);
}

VNCVideoCapturer::~VNCVideoCapturer()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "~~~VNCVideoCapturer";
}

// overide rtc::Thread
void VNCVideoCapturer::Run()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Running ...";
}

cricket::CaptureState VNCVideoCapturer::Start(const cricket::VideoFormat& format)
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Start ...";
	SetCaptureFormat(&format);
	SetCaptureState(cricket::CS_RUNNING);
	rtc::Thread::Start();
	return cricket::CS_RUNNING;
}

void VNCVideoCapturer::Stop()
{
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "VNCVideoCapturer Capture Stop ...";
	rtc::Thread::Stop();
	SetCaptureFormat(NULL);
	SetCaptureState(cricket::CS_STOPPED);
}

bool VNCVideoCapturer::GetPreferredFourccs(std::vector<unsigned int>* fourccs)
{
	return true;
}