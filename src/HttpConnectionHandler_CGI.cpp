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

HandlerStatus HttpConnectionHandler::serveCgi() {
    CgiHandler cgiHandler(*this);
    cgiHandler.executeCgi();
    int fromFd = cgiHandler.getPipeFromCgi()[0];

    std::string raw;
    char buffer[8192];
    ssize_t n = read(fromFd, buffer, sizeof(buffer));
	
	// Per read man page: "When attempting to read from an empty pipe,
	// if some process has the pipe open for writing and O_NONBLOCK is set, read() will return -1."
	// So the following check will lead to false positives in error check when there is nothing to read.
	// How to fix it?
	if (n < 0) {
		close(fromFd);
		return S_Error;
	}
	send(clientSocket, buffer, n, 0);
	if (!n) {
		close(fromFd);
		return S_Done;
	}
	return S_Again;
}