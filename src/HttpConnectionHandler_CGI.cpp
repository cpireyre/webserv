#include "HttpConnectionHandler.hpp"
#include "CgiHandler.hpp"
#include "Logger.hpp"
#include <sys/wait.h>

static std::string	removePhpCgiHeaders(std::string cgiResponse);

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
	int CgiExitCode = 0;
	int CgiProcessStatus = 0;
	CgiProcessStatus = waitpid(cgiHandler.cgiPid, &CgiExitCode, WNOHANG);
	if (CgiProcessStatus == 0)
		return S_Again;
	if (WEXITSTATUS(CgiExitCode) == EXIT_FAILURE)
	{
		close(cgiHandler.getPipeFromCgi()[0]);
		return S_Error;
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
		if (cgiType == PHP) response = removePhpCgiHeaders(response);
		std::string header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: text/html\r\n";
		header += "Content-Length: " + std::to_string(response.size()) + "\r\n\r\n";
		response.insert(0, header);
		close(fromFd);
		return S_Done;
	}
	return S_Again;
}

static std::string	removePhpCgiHeaders(std::string cgiResponse)
{
  const size_t sep = cgiResponse.find("\r\n\r\n");
  if (sep == string::npos)
    return (cgiResponse);
  return cgiResponse.substr(sep + 4, cgiResponse.size());
}
