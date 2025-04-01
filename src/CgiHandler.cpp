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

}
