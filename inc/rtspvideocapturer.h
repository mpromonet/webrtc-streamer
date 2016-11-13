/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspvideocapturer.h
** 
** -------------------------------------------------------------------------*/

#ifndef RTSPVIDEOCAPTURER_H_
#define RTSPVIDEOCAPTURER_H_

#include <string.h>
#include <vector>

#include "webrtc/media/base/videocapturer.h"
#include "webrtc/base/timeutils.h"


#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"


#define RTSP_CALLBACK(uri, resultCode, resultString) \
static void continueAfter ## uri(RTSPClient* rtspClient, int resultCode, char* resultString) { static_cast<RTSPConnection*>(rtspClient)->continueAfter ## uri(resultCode, resultString); } \
void continueAfter ## uri (int resultCode, char* resultString); \
/**/

class Environment : public BasicUsageEnvironment
{
	public:
		Environment();
		~Environment();
	
	
		void mainloop()
		{
			this->taskScheduler().doEventLoop(&m_stop);
		}
				
		void stop() { m_stop = 1; };	
		
	protected:
		char                     m_stop;		
};


/* ---------------------------------------------------------------------------
**  RTSP client connection interface
** -------------------------------------------------------------------------*/
class RTSPConnection : public RTSPClient
{
	public:
		/* ---------------------------------------------------------------------------
		**  RTSP client callback interface
		** -------------------------------------------------------------------------*/
		class Callback
		{
			public:
				virtual bool    onNewSession(const char* media, const char* codec) = 0;
				virtual bool    onData(unsigned char* buffer, ssize_t size) = 0;
				virtual ssize_t onNewBuffer(unsigned char* buffer, ssize_t size) { return 0; };
		};

	protected:
		/* ---------------------------------------------------------------------------
		**  RTSP client Sink
		** -------------------------------------------------------------------------*/
		class SessionSink: public MediaSink 
		{
			public:
				static SessionSink* createNew(UsageEnvironment& env, Callback* callback) { return new SessionSink(env, callback); }

			private:
				SessionSink(UsageEnvironment& env, Callback* callback);
				virtual ~SessionSink();

				void allocate(ssize_t bufferSize);

				static void afterGettingFrame(void* clientData, unsigned frameSize,
							unsigned numTruncatedBytes,
							struct timeval presentationTime,
							unsigned durationInMicroseconds)
				{
					static_cast<SessionSink*>(clientData)->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
				}
				
				void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds);

				virtual Boolean continuePlaying();

			private:
				size_t                 m_bufferSize;
				u_int8_t*              m_buffer;
				Callback*              m_callback; 	
				ssize_t                m_markerSize;
		};
	
	public:
		RTSPConnection(UsageEnvironment& env, Callback* callback, const std::string & rtspURL, int verbosityLevel = 255);
		virtual ~RTSPConnection();

	protected:
		void sendNextCommand(); 
				
		RTSP_CALLBACK(DESCRIBE,resultCode,resultString);
		RTSP_CALLBACK(SETUP,resultCode,resultString);
		RTSP_CALLBACK(PLAY,resultCode,resultString);
		
	protected:
		MediaSession*            m_session;                   
		MediaSubsession*         m_subSession;             
		MediaSubsessionIterator* m_subSessionIter;
		Callback*                m_callback; 	
};


Environment::Environment() : BasicUsageEnvironment(*BasicTaskScheduler::createNew()), m_stop(0)
{
}

Environment::~Environment()
{
	TaskScheduler* scheduler = &this->taskScheduler();
	this->reclaim();
	delete scheduler;	
}

RTSPConnection::SessionSink::SessionSink(UsageEnvironment& env, Callback* callback) 
	: MediaSink(env)
	, m_bufferSize(0)
	, m_buffer(NULL)
	, m_callback(callback) 
	, m_markerSize(0)
{
	allocate(1024*1024);
}

RTSPConnection::SessionSink::~SessionSink()
{
	delete [] m_buffer;
}

void RTSPConnection::SessionSink::allocate(ssize_t bufferSize)
{
	m_bufferSize = bufferSize;
	m_buffer = new u_int8_t[m_bufferSize];
	if (m_callback)
	{
		m_markerSize = m_callback->onNewBuffer(m_buffer, m_bufferSize);
		LOG(INFO) << "markerSize:" << m_markerSize;
	}
}


void RTSPConnection::SessionSink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds)
{
	LOG(LS_VERBOSE) << "NOTIFY size:" << frameSize;
	if (numTruncatedBytes != 0)
	{
		delete [] m_buffer;
		LOG(INFO) << "buffer too small " << m_bufferSize << " allocate bigger one\n";
		allocate(m_bufferSize*2);
	}
	else if (m_callback)
	{
		if (!m_callback->onData(m_buffer, frameSize+m_markerSize))
		{
			LOG(WARNING) << "NOTIFY failed";
		}
	}
	this->continuePlaying();
}

Boolean RTSPConnection::SessionSink::continuePlaying()
{
	Boolean ret = False;
	if (source() != NULL)
	{
		source()->getNextFrame(m_buffer+m_markerSize, m_bufferSize-m_markerSize,
				afterGettingFrame, this,
				onSourceClosure, this);
		ret = True;
	}
	return ret;	
}


		
RTSPConnection::RTSPConnection(UsageEnvironment& env, Callback* callback, const std::string & rtspURL, int verbosityLevel) 
				: RTSPClient(env, rtspURL.c_str(), verbosityLevel, NULL, 0
#if LIVEMEDIA_LIBRARY_VERSION_INT > 1371168000 
					,-1
#endif
					)
				, m_session(NULL)
				, m_subSessionIter(NULL)
				, m_callback(callback)
{
	// initiate connection process
	this->sendNextCommand();
}

RTSPConnection::~RTSPConnection()
{
	delete m_subSessionIter;
	Medium::close(m_session);
}
		
void RTSPConnection::sendNextCommand() 
{
	if (m_subSessionIter == NULL)
	{
		// no SDP, send DESCRIBE
		this->sendDescribeCommand(continueAfterDESCRIBE); 
	}
	else
	{
		m_subSession = m_subSessionIter->next();
		if (m_subSession != NULL) 
		{
			// still subsession to SETUP
			if (!m_subSession->initiate()) 
			{
				LOG(WARNING) << "Failed to initiate " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << envir().getResultMsg();
				this->sendNextCommand();
			} 
			else 
			{					
				LOG(INFO) << "Initiated " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession";
			}

			this->sendSetupCommand(*m_subSession, continueAfterSETUP);
		}
		else
		{
			// no more subsession to SETUP, send PLAY
			this->sendPlayCommand(*m_session, continueAfterPLAY);
		}
	}
}

void RTSPConnection::continueAfterDESCRIBE(int resultCode, char* resultString)
{
	if (resultCode != 0) 
	{
		LOG(WARNING) << "Failed to DESCRIBE: " << resultString;
	}
	else
	{
		LOG(INFO) << "Got SDP:\n" << resultString;
		m_session = MediaSession::createNew(envir(), resultString);
		m_subSessionIter = new MediaSubsessionIterator(*m_session);
		this->sendNextCommand();  
	}
	delete[] resultString;
}

void RTSPConnection::continueAfterSETUP(int resultCode, char* resultString)
{
	if (resultCode != 0) 
	{
		LOG(WARNING) << "Failed to SETUP: " << resultString;
	}
	else
	{				
		m_subSession->sink = SessionSink::createNew(envir(), m_callback);
		if (m_subSession->sink == NULL) 
		{
			LOG(WARNING) << "Failed to create a data sink for " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << envir().getResultMsg() << "\n";
		}
		else if (m_callback->onNewSession(m_subSession->mediumName(), m_subSession->codecName()))
		{
			LOG(WARNING) << "Created a data sink for the \"" << m_subSession->mediumName() << "/" << m_subSession->codecName() << "\" subsession";
			m_subSession->sink->startPlaying(*(m_subSession->readSource()), NULL, NULL);
		}
	}
	delete[] resultString;
	this->sendNextCommand();  
}	

void RTSPConnection::continueAfterPLAY(int resultCode, char* resultString)
{
	if (resultCode != 0) 
	{
		LOG(WARNING) << "Failed to PLAY: " << resultString;
	}
	else
	{
		LOG(INFO) << "PLAY OK";
	}
	delete[] resultString;
}

#include "webrtc/base/optional.h"
#include "webrtc/common_video/h264/sps_parser.h"

class RTSPVideoCapturer : public cricket::VideoCapturer, public RTSPConnection::Callback, public rtc::Thread
{
	public:
		RTSPVideoCapturer(const std::string & uri) : m_connection(m_env,this,uri.c_str())
		{
			LOG(INFO) << "===========================RTSPVideoCapturer" << uri ;
		}
	  
		virtual ~RTSPVideoCapturer() 
		{
		}
		
		virtual bool onNewSession(const char* media, const char* codec)
		{
			LOG(INFO) << "===========================onNewSession" << media << "/" << codec;
			bool success = false;
			if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
			{
				success = true;
			}
			return success;			
		}
		
		virtual bool onData(unsigned char* buffer, ssize_t size) 
		{			
			std::cout << "===========================onData" << size << std::endl;
			
			//if (!GetCaptureFormat()) 
			{
				rtc::Optional<webrtc::SpsParser::SpsState> sps = webrtc::SpsParser::ParseSps(buffer, size);
				if (!sps)
				{	
					std::cout << "cannot parse sps" << std::endl;
				}
				else
				{	
					std::cout << "add new format " << sps->width << "x" << sps->height << std::endl;
					std::vector<cricket::VideoFormat> formats;
					formats.push_back(cricket::VideoFormat(sps->width, sps->height, cricket::VideoFormat::FpsToInterval(25), cricket::FOURCC_H264));
					SetSupportedFormats(formats);				
				}
			}
			if (!GetCaptureFormat()) 
			{
				return false;
			}

			cricket::CapturedFrame frame;
			frame.width = GetCaptureFormat()->width;
			frame.height = GetCaptureFormat()->height;
			frame.fourcc = GetCaptureFormat()->fourcc;
			frame.data_size = size;

			std::unique_ptr<char[]> data(new char[size]);
			frame.data = data.get();
			memcpy(frame.data, buffer, size);

			SignalFrameCaptured(this, &frame);
			return true;
		}

		virtual cricket::CaptureState Start(const cricket::VideoFormat& format) 
		{
			SetCaptureFormat(&format);
			SetCaptureState(cricket::CS_RUNNING);
			rtc::Thread::Start();
			return cricket::CS_RUNNING;
		}
	  
		virtual void Stop() 
		{
			m_env.stop();
			rtc::Thread::Stop();
			SetCaptureFormat(NULL);
			SetCaptureState(cricket::CS_STOPPED);
		}
		
		void Run()
		{	
			m_env.mainloop();
		}
	  
		virtual bool GetPreferredFourccs(std::vector<unsigned int>* fourccs) 
		{
			fourccs->push_back(cricket::FOURCC_H264);
			return true;
		}
	  
		virtual bool IsScreencast() const { return false; };
		virtual bool IsRunning() { return this->capture_state() == cricket::CS_RUNNING; }
	  
	private:
		Environment    m_env;
		RTSPConnection m_connection;
		
};

#endif 

