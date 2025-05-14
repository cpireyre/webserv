#include "HttpConnectionHandler.hpp"
#include "CgiHandler.hpp"
#include "Logger.hpp"
#include <sys/wait.h>

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

HandlerStatus HttpConnectionHandler::serveCgi(CgiHandler &cgiHandler) {
	int status = waitpid(cgiHandler.cgiPid, &status, WNOHANG);
	if (status == 0)
	{
		usleep(250);
		return S_Again;
	}
	// cgiHandler.executeCgi();
	int fromFd = cgiHandler.getPipeFromCgi()[0];
	char buffer[8192];
	bzero(buffer, sizeof(buffer));
	logDebug("Serving CGI");

	ssize_t n = read(fromFd, buffer, sizeof(buffer));
	logDebug("Read %d bytes", n);
	// Per read man page: "When attempting to read from an empty pipe,
	// if some process has the pipe open for writing and O_NONBLOCK is set, read() will return -1."
	// So the following check will lead to false positives in error check when there is nothing to read.
	// How to fix it?
	if (n < 0) {
		close(fromFd);
		return S_Error;
	}
	response.append(buffer, n);
	if (!n) {
		std::string header = "HTTP/1.1 200 OK\r\n";
		header += "Content-Length: " + std::to_string(response.size()) + "\r\n\r\n";
		response.insert(0, header);
		close(fromFd);
		return S_Done;
	}
	return S_Again;
}
