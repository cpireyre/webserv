#include "HttpConnectionHandler.hpp"
#include "CgiHandler.hpp"

CgiTypes HttpConnectionHandler::checkCgi() {

	cgiType = NONE;

	if (originalPath.find_first_of('?') == originalPath.npos) {
		filePath = originalPath;
		queryString.clear();
	}
	else {
		filePath = originalPath.substr(0, originalPath.find_first_of('?'));
		queryString = originalPath.substr(originalPath.find_first_of('?') + 1);
	}

	if (locBlock->cgiPathPython != "" && filePath.find(".py") != std::string::npos) 
		cgiType = PYTHON;
	else if (locBlock->cgiPathPHP != "" && filePath.find(".php") != std::string::npos)
		cgiType = PHP;

	return cgiType;
}