#include "CgiHandler.hpp"

CgiHandler::CgiHandler(HttpConnectionHandler conn) {
	if (conn.type == PYTHON)
		_interpreterPath = conn.locInQuestion.cgiPathPython + "/python3";
	else if (conn.type == PHP)
		_interpreterPath = conn.locInQuestion.cgiPathPHP + "/php-cgi";

	_contentLength = conn.getHeaders().at("Content-Length");
	_contentType = conn.getHeaders().at("Content-Type");
	_gatewayInterface = "CGI/1.1";

}