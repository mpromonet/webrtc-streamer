/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** HttpServerRequestHandler.h
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <list>
#include <map>
#include <functional>

#include "json/json.h"
#include "CivetServer.h"


/* ---------------------------------------------------------------------------
**  http callback
** -------------------------------------------------------------------------*/
class HttpServerRequestHandler : public CivetServer
{
	public:
		typedef std::function<Json::Value(const struct mg_request_info *req_info, const Json::Value &)> httpFunction;
	
		HttpServerRequestHandler(std::map<std::string,httpFunction>& func, const std::vector<std::string>& options); 
};


