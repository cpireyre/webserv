#include "CgiHandler.hpp"

CgiHandler::CgiHandler(HttpConnectionHandler conn) {
	if (conn.CgiType == PYTHON)
		_pathToInterpreter = conn.locInQuestion.cgiPathPython + "/python3";
	else if (conn.CgiType == PHP)
		_pathToInterpreter = conn.locInQuestion.cgiPathPHP + "/php-cgi";

	std::string currentPath = std::filesystem::current_path();
	std::string root = conn.serverInQuestion.getRootViaLocation("/");
	_pathToScript = currentPath + "/" + root + conn.getFilePath();

	_execveArgs[0] = (char * )_pathToInterpreter.c_str();
	_execveArgs[1] = (char * )_pathToScript.c_str();
	_execveArgs[2] = NULL;

	const std::map<std::string, std::string>	&headerMap = conn.getHeaders();

	_contentLength = "CONTENT_LENGTH=";
	if (headerMap.find("Content-Length") != headerMap.end())
		_contentLength += headerMap.at("Content-Length");

	
	_contentType = "CONTENT_TYPE=";
	if (headerMap.find("Content-Type") != headerMap.end())
		_contentType += headerMap.at("Content-Type");
	_queryString = "QUERY_STRING=" + conn.queryString;
	_pathInfo = "PATH_INFO=" + _pathToScript;
	_requestMethod = "REQUEST_METHOD=" + conn.getMethod();
	_scriptFileName = "SCRIPT_FILENAME=" + _pathToScript.substr(_pathToScript.find_last_of('/') + 1);
	_scriptName = "SCRIPT_NAME=" + root + conn.getFilePath();
	_redirectStatus = "REDIRECT_STATUS=200";
	_serverProtocol = "SERVER_PROTOCOL=HTTP/1.1";
	_gatewayInterface = "GATEWAY_INTERFACE=CGI/1.1";
	_remoteAddr = "REMOTE_ADDR=" + conn.serverInQuestion.getHost();
	_serverName = "SERVER_NAME=" + conn.serverInQuestion.getServerNames();
	_serverPort = "SERVER_PORT=" + conn.serverInQuestion.getPort();
	
	_execveEnv[0] = (char *) _contentLength.c_str();
	_execveEnv[1] = (char *) _contentType.c_str();
	_execveEnv[2] = (char *) _queryString.c_str();
	_execveEnv[3] = (char *) _serverProtocol.c_str();
	_execveEnv[4] = (char *) _gatewayInterface.c_str();
	_execveEnv[5] = (char *) _pathInfo.c_str();
	_execveEnv[6] = (char *) _requestMethod.c_str();
	_execveEnv[7] = (char *) _scriptFileName.c_str();
	_execveEnv[8] = (char *) _redirectStatus.c_str();
	_execveEnv[9] = (char *) _scriptName.c_str();
	_execveEnv[10] = (char *) _remoteAddr.c_str();
	_execveEnv[11] = (char *) _serverName.c_str();
	_execveEnv[12] = (char *) _serverPort.c_str();
	_execveEnv[13] = NULL;
}
