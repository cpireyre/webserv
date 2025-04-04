#include "CgiHandler.hpp"
#include "HttpConnectionHandler.hpp"
#include "Configuration.hpp"

CgiHandler::CgiHandler(HttpConnectionHandler conn) {
	if (conn.getCgiType() == PYTHON)
		_pathToInterpreter = conn.getLocationBlock()->cgiPathPython + "/python3";
	else if (conn.getCgiType() == PHP)
		_pathToInterpreter = conn.getLocationBlock()->cgiPathPHP + "/php-cgi";

	std::string currentPath = std::filesystem::current_path();
	std::string root = conn.getConf()->getRootViaLocation("/");
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
	_queryString = "QUERY_STRING=" + conn.getQueryString();
	_pathInfo = "PATH_INFO=" + _pathToScript;
	_requestMethod = "REQUEST_METHOD=" + conn.getMethod();
	_scriptFileName = "SCRIPT_FILENAME=" + _pathToScript.substr(_pathToScript.find_last_of('/') + 1);
	_scriptName = "SCRIPT_NAME=" + root + conn.getFilePath();
	_redirectStatus = "REDIRECT_STATUS=200";
	_serverProtocol = "SERVER_PROTOCOL=HTTP/1.1";
	_gatewayInterface = "GATEWAY_INTERFACE=CGI/1.1";
	_remoteAddr = "REMOTE_ADDR=" + conn.getConf()->getHost();
	_serverName = "SERVER_NAME=" + conn.getConf()->getServerNames();
	_serverPort = "SERVER_PORT=" + conn.getConf()->getPort();
	
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

void CgiHandler::executeCgi() {
	return;
}



		// // Additions to Jere's work
		// NO NEED bool					isCgi; // Default false. True if the request is for a CGI script
		// CgiTypes				CgiType; // Type of CGI script (PHP or Python or none)
		// NO NEED Configuration			serverInQuestion; // Server configuration for the current request
		// NO NEED LocationBlock			locInQuestion; // Location block for the current request

		// DONE std::string 			filePath; //  Requested path (differs from Jere's "path" because anything after the path (file name, question mark statements etc.) it will be pruned)
		// DONE std::string				extension; // .php or .py or empty
		// DONE std::string				queryString; // Everything from the URI after '?' character

		// const std::string getFilePath() const { return filePath; }
		// // End of additions