#include <string>
#include <regex>
#pragma once

class Url {
  public:
		Url(std::string url) {
      // Official regex specified in https://tools.ietf.org/html/rfc3986#page-50
      std::regex url_regex (
        R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)",
        std::regex::extended
      );
      std::smatch match_result;
      bool matched = std::regex_match(url, match_result, url_regex);
      if (!matched) {
        this->isEmpty = true;
        return;
      }
      this->host = match_result[4];

      int index = this->host.find('@');
      if (index != std::string::npos) {
        std::string auth_credentials = this->host.substr(0, index - 1);
        this->host = this->host.substr(index, this->host.length() - index);
      } else {
        this->username = "";
        this->password = "";
      }

      this->scheme = match_result[2];
      this->path = match_result[5];
      this->query = match_result[7];
      this->isEmpty = false;
    }

    bool isDomainOf(std::string domain) {
      auto host = this->host;
      int index = host.find(':');
      if (index != std::string::npos) {
        // take out port
        host = host.substr(index, host.length() - index);
      }

      // ensure host ends with the domain
      return host.compare(host.size() - domain.size(), domain.size(), domain) == 0;
    }

    std::string toString() {
      if (isEmpty) {
        return "";
      }

      return scheme + "://" 
        + (username.length() || password.length() ? username + ":" + password : "")
        + host + "/"
        + path + "?"
        + query;
    }
	
    std::string username;
    std::string password;
    std::string scheme;
    std::string host;
    std::string path;
    std::string query;
    bool isEmpty;
};
