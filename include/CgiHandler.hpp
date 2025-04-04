#pragma once

#include <string>
// #include "HttpConnectionHandler.hpp"

class HttpConnectionHandler;

enum CgiTypes
{
	PYTHON,
	PHP,
	NONE
};

class CgiHandler
{
	private:
		std::string _contentLength;
		std::string _contentType;
		std::string _gatewayInterface;
		std::string _pathInfo;
		std::string _queryString;
		std::string _remoteAddr;
		std::string _requestMethod;
		std::string _scriptFileName;
		std::string	_scriptName;
		std::string _redirectStatus;
		std::string	_serverName;
		std::string	_serverPort;
		std::string _serverProtocol;

		std::string _pathToInterpreter;
		std::string _pathToScript;

		char 	*_execveArgs[3] = {};
		char 	*_execveEnv[16] = {};
	public:
		CgiHandler(HttpConnectionHandler conn);
		void executeCgi();
};
