/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerRequestHandler.cpp
** 
** -------------------------------------------------------------------------*/

#include <string.h>
#include <iostream>
    
#include "HttpServerRequestHandler.h"


int log_message(const struct mg_connection *conn, const char *message) 
{
    fprintf(stderr, "%s\n", message);
    return 0;
}

static struct CivetCallbacks _callbacks;
const struct CivetCallbacks * getCivetCallbacks() 
{
    memset(&_callbacks, 0, sizeof(_callbacks));
    _callbacks.log_message = &log_message;
    return &_callbacks;
}


/* ---------------------------------------------------------------------------
**  Civet HTTP callback 
** -------------------------------------------------------------------------*/
class RequestHandler : public CivetHandler
{
  public:
    bool handle(CivetServer *server, struct mg_connection *conn)
    {
        bool ret = false;
        const struct mg_request_info *req_info = mg_get_request_info(conn);
        
        std::cout << "uri:" << req_info->request_uri << std::endl;
        
        HttpServerRequestHandler* httpServer = (HttpServerRequestHandler*)server;
        
        HttpServerRequestHandler::httpFunction fct = httpServer->getFunction(req_info->request_uri);
        if (fct != NULL)
        {
            Json::Value  jmessage;			
            
            // read input
            Json::Value  in = this->getInputMessage(req_info, conn);
            
            // invoke API implementation
            Json::Value out(fct(req_info, in));
            
            // fill out
            if (out.isNull() == false)
            {
                std::string answer(Json::StyledWriter().write(out));
                std::cout << "answer:" << answer << std::endl;	

                mg_printf(conn,"HTTP/1.1 200 OK\r\n");
                mg_printf(conn,"Access-Control-Allow-Origin: *\r\n");
                mg_printf(conn,"Content-Type: text/plain\r\n");
                mg_printf(conn,"Content-Length: %zd\r\n", answer.size());
                mg_printf(conn,"Connection: close\r\n");
                mg_printf(conn,"\r\n");
                mg_printf(conn,answer.c_str());	
                
                ret = true;
            }			
        }		
        
        return ret;
    }
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        return handle(server, conn);
    }
    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        return handle(server, conn);
    }

  private:
    Json::Value getInputMessage(const struct mg_request_info *req_info, struct mg_connection *conn) {
        Json::Value  jmessage;

        // read input
        long long tlen = req_info->content_length;
        if (tlen > 0)
        {
            std::string body;
            long long nlen = 0;
            const long long bufSize = 1024;
            char buf[bufSize];
            while (nlen < tlen) {
                long long rlen = tlen - nlen;
                if (rlen > bufSize) {
                    rlen = bufSize;
                }
                rlen = mg_read(conn, buf, (size_t)rlen);
                if (rlen <= 0) {
                    break;
                }
                body.append(buf, rlen);

                nlen += rlen;
            }

            // parse in
            Json::Reader reader;
            if (!reader.parse(body, jmessage))
            {
                std::cout << "Received unknown message:" << body << std::endl;
            }
        }
        return jmessage;
    }	
};

/* ---------------------------------------------------------------------------
**  Constructor
** -------------------------------------------------------------------------*/
HttpServerRequestHandler::HttpServerRequestHandler(std::map<std::string,httpFunction>& func, const std::vector<std::string>& options) 
    : CivetServer(options, getCivetCallbacks()), m_func(func)
{
    // register handlers
    for (auto it : m_func) {
        this->addHandler(it.first, new RequestHandler());
    } 	
}	
    

HttpServerRequestHandler::httpFunction HttpServerRequestHandler::getFunction(const std::string& uri)
{
    httpFunction fct = NULL;
    std::map<std::string,httpFunction>::iterator it = m_func.find(uri);
    if (it != m_func.end())
    {
        fct = it->second;
    }
    
    return fct;
}

