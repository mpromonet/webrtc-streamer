/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <map>
#include <tuple>
#include <list>

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/p2p/base/stunserver.h"

#include "mongoose.h"

#include "conductor.h"
#include "peer_connection_client.h"


typedef int (*callback)(struct mg_connection *conn);
typedef int (*callback_notify)(struct mg_connection *conn, char* buffer, ssize_t size);
struct url_handler
{
	const char* uri;
	callback handle_req;
	callback handle_close;
	callback_notify handle_notify;
};

static int id = 1;
struct peer
{
	peer(const std::string& name) : m_name(name),m_connected(1) {};
	std::string m_name;
	int m_connected;
};

std::map<int,peer> map;
std::list< std::tuple<int,int,std::string> > msgList;

void answer(struct mg_connection *conn, int id, const std::string& body)
{
	std::ostringstream os;
	
	os << "HTTP/1.1 200 OK\r\n";
	
	os << "Cache-Control: no-cache\r\n";
	os << "Content-Type: text/plain\r\n";
	os << "Content-Length: " << body.size() << "\r\n";
	if (id != 0) os << "Pragma: " << id << "\r\n";
	os << "\r\n";
	os << body;
	
	std::string msg(os.str());
	std::cout << "send peer:"<<  id << " " << body	<< std::endl;
	mg_write(conn, msg.c_str(), msg.size());
}

void post(const std::string& body)
{
	for (std::map<int,peer>::iterator it = map.begin() ; it!= map.end(); ++it)
	{
		msgList.push_back(std::tuple<int,int,std::string>(it->first,it->first,body));
	}
}

int handle_signin(struct mg_connection *conn) 
{
	std::string name(conn->query_string);
	std::cout << name	<< std::endl;
	int peerid = id++;
	peer newpeer(name);
	std::ostringstream os;
	os << newpeer.m_name << "," << peerid << "," <<  newpeer.m_connected << "\r\n";

	for (std::map<int,peer>::iterator it = map.begin() ; it!= map.end(); ++it)
	{
		os << it->second.m_name << "," << it->first << "," <<  it->second.m_connected << "\r\n";
	}
	
	std::string body(os.str());
	answer(conn, peerid, body);
	
	map.insert(std::pair<int,peer>(peerid,newpeer));
	post(body);
	
	return MG_TRUE;
}

int handle_signout(struct mg_connection *conn) 
{
	char peer_id[64];
	mg_get_var(conn, "peer_id", peer_id, sizeof(peer_id));
	int peerid = atoi(peer_id);
	std::map<int,peer>::iterator it = map.find(peerid);
	if (it != map.end())
	{
		it->second.m_connected = 0;
		
		std::ostringstream os;
		os << it->second.m_name << "," << it->first << "," <<  it->second.m_connected << "\r\n";
		std::string body(os.str());
		answer(conn, peerid, body);				
		
		map.erase(it);		
		post(body);
	}
	
	return MG_TRUE;
}

int handle_wait(struct mg_connection *conn) 
{
	char peer_id[64];
	mg_get_var(conn, "peer_id", peer_id, sizeof(peer_id));
	int peerid = atoi(peer_id);
		
	std::ostringstream os;
	std::list< std::tuple<int,int,std::string> >::iterator it = msgList.begin();
	for ( ; it != msgList.end(); ++it)
	{
		if (std::get<1>(*it) == peerid)
		{
			os << std::get<2>(*it) << "\r\n";
			std::string body(os.str());
			answer(conn, std::get<0>(*it), body);
			
			msgList.erase(it);
			std::cout << "nb message "<<  msgList.size()	<< std::endl;
			break;
		}
	}
	
	return MG_TRUE;
}

int handle_message(struct mg_connection *conn) 
{
	char peer_id[64];
	mg_get_var(conn, "peer_id", peer_id, sizeof(peer_id));
	char to[64];
	mg_get_var(conn, "to", to, sizeof(to));
	std::cout << "handle_message "<<  peer_id << "=>" << to	<< std::endl;
		
	msgList.push_back(std::tuple<int,int,std::string>(atoi(peer_id), atoi(to), std::string(conn->content,conn->content_len)));
	return MG_TRUE;
}

int handle_dump(struct mg_connection *conn) 
{	
	std::ostringstream os;
	for (std::map<int,peer>::iterator it = map.begin() ; it!= map.end(); ++it)
	{
		os << it->first << "," << it->second.m_name << "," << it->second.m_connected << "\n";
	}		
	
	std::list< std::tuple<int,int,std::string> >::iterator it = msgList.begin();
	for ( ; it != msgList.end(); ++it)
	{
		os << std::get<0>(*it) << "=>" << std::get<1>(*it) << " " << std::get<2>(*it) << "\n";
	}
	std::string msg(os.str());
	mg_send_data(conn, msg.c_str(), msg.size());
	return MG_TRUE;
}

url_handler urls[] = {
	{ "/sign_in" , handle_signin  , NULL, NULL },
	{ "/sign_out", handle_signout , NULL, NULL },
	{ "/message" , handle_message , NULL, NULL },
	{ "/wait"    , handle_wait    , NULL, NULL },
	{ "/dump"    , handle_dump    , NULL, NULL },
	{ NULL       , NULL           , NULL, NULL },
};

const url_handler* find_url(const char* uri)
{
	const url_handler* url = NULL;
	if (uri != NULL)
	{
		for (int i=0; urls[i].uri ; ++i)
		{			
			if (strcmp(urls[i].uri, uri) == 0)
			{
				url = &urls[i];
				break;
			}
		}
	}
	return url;
}


/* ---------------------------------------------------------------------------
**  mongoose callback
** -------------------------------------------------------------------------*/
static int ev_handler(struct mg_connection *conn, enum mg_event ev) 
{
	int ret = MG_FALSE;
	switch (ev) 
	{
		case MG_AUTH: ret = MG_TRUE; break;
		case MG_REQUEST: 
		{
			const url_handler* url = find_url(conn->uri);
			if (url && url->handle_req)
			{
				ret = url->handle_req(conn);
			}
		}
		break;
		case MG_CLOSE:
		{
			const url_handler* url = find_url(conn->uri);
			if (url && url->handle_close)
			{
				ret = url->handle_close(conn);
			}
		}
		break;
		default: break;
	} 
	return ret;
}

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) {
	const char* port     = "8080";
	const char* device   = "/dev/video0";
	const char* stunport = "3478";
	
	int c = 0;     
	while ((c = getopt (argc, argv, "hP:")) != -1)
	{
		switch (c)
		{
			case 'P': port = optarg; break;
			case 'h':
			default:
				std::cout << argv[0] << " [-P http port] [device]" << std::endl;
				std::cout << "\t -P port  : HTTP port (default "<< port << ")" << std::endl;
				std::cout << "\t device   : capture device (default "<< device << ")" << std::endl;
				exit(0);
		}
	}
	if (optind<argc)
	{
		device = argv[optind];
	}	

	rtc::LogMessage::LogContext(rtc::LS_SENSITIVE);
	rtc::Thread* thread = rtc::Thread::Current();
	rtc::InitializeSSL();
	
	// webrtc server
	PeerConnectionClient client;
	rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(&client,device));
	conductor->StartLogin("127.0.0.1", atoi(port));	

	// http server
	struct mg_server *server = mg_create_server(NULL, ev_handler);
	mg_set_option(server, "listening_port", port);
	std::string currentPath(get_current_dir_name());
	mg_set_option(server, "document_root", currentPath.c_str());
	std::cout << "Started on port:" << mg_get_option(server, "listening_port") << " webroot:" << mg_get_option(server, "document_root") << std::endl; 
	
	// STUN server
	rtc::SocketAddress server_addr;
	server_addr.SetIP("0.0.0.0");
	server_addr.SetPort(atoi(stunport));
	cricket::StunServer* stunserver = NULL;
	rtc::AsyncUDPSocket* server_socket = rtc::AsyncUDPSocket::Create(thread->socketserver(), server_addr);
	if (server_socket) 
	{
		stunserver = new cricket::StunServer(server_socket);
		std::cout << "STUN Listening at " << server_addr.ToString() << std::endl;
	}
  
	// mainloop
	while(thread->ProcessMessages(10))
	{
		 mg_poll_server(server, 10);
	}

	conductor->Close();
	
	rtc::CleanupSSL();
	return 0;
}

