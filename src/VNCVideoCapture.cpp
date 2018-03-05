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
  VNCVideoCapturer* that = (VNCVideoCapturer *) rfbClientGetClientData(client, "this");
  return that->onGetPassword();
}

static rfbBool rfbInitClientWrapper(rfbClient* client, char *progname) {
  char *connect;
  asprintf(&connect, "%s:%i", host, port);
  char *args[] = { progname, connect };
  int len = 2;
  return rfbInitClient(client, &len, args);
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
	if (!rfbInitClientWrapper(client, uri.c_str)) {
		fprintf(stderr, "Something went wrong");
		exit(EXIT_FAILURE);
	}
	rfbClientSetClientData(client, "this", this);
	client->GetPassword = get_password;
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << "uri: " << uri;
}

VNCVideoCapturer::onError(char[] string) {
	RTC_LOG(LS_ERROR) << __PRETTY_FUNCTION__ << string;
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