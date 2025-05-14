#include "HttpConnectionHandler.hpp"
#include "CgiHandler.hpp"
#include "Logger.hpp"

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
    // cgiHandler.executeCgi();
    int fromFd = cgiHandler.getPipeFromCgi()[0];
	int offset = 0;
	char buffer[8192];
	if (cgiHandler.hasSentHeader == false)
	{
		char header[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n";
		memcpy(buffer, header, strlen(header));
		offset += strlen(header);
		cgiHandler.hasSentHeader = true;
		send(clientSocket, buffer, strlen(header), 0);
		return S_Again;
	}

    ssize_t n = read(fromFd, buffer + offset, sizeof(buffer) - offset);

	// Per read man page: "When attempting to read from an empty pipe,
	// if some process has the pipe open for writing and O_NONBLOCK is set, read() will return -1."
	// So the following check will lead to false positives in error check when there is nothing to read.
	// How to fix it?
	if (n < 0) {
		close(fromFd);
		return S_Error;
	}
	std::stringstream output;
	output << std::hex << n << "\r\n";
	output << buffer << "\r\n";
	n += offset;
	send(clientSocket, output.str().c_str(), output.str().size(), 0);
	write(2, buffer, n);
	if (!n) {
		close(fromFd);
		return S_Done;
	}
	return S_Again;
}
