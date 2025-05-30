#pragma once

#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <iostream>
#include <cstdlib>

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
		std::string _cookie;

		std::string _pathToInterpreter;

		char 		*_execveArgs[3] = {};
		char 		*_execveEnv[16] = {};

		int  		_pipeToCgi[2];
		int  		_pipeFromCgi[2];
		std::string _postData;  // data to send to CGI process, if any
		size_t		_postDataOffset = 0; // How many bytes have been written so far
		int			_waitpidRes;

	public:
		std::string _pathToScript;
		pid_t		cgiPid;
		CgiHandler();
		int*		getPipeToCgi();
		int*		getPipeFromCgi();
		pid_t		getCgiPid();
		std::string	getPostData();
		size_t		getPostDataOffset();
		int			getWaitpidRes();
		CgiHandler(const HttpConnectionHandler &conn);
		void executeCgi();
		void printCgiInfo();
		bool		hasSentHeader;
    void CgiResetObject(void);
    void populate(const HttpConnectionHandler &conn);
};
