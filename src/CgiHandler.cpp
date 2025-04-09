#include "CgiHandler.hpp"
#include "HttpConnectionHandler.hpp"
#include "Configuration.hpp"
#include "Logger.hpp"
#include <sys/wait.h>

int*		CgiHandler::getPipeToCgi() { return _pipeToCgi; }
int*		CgiHandler::getPipeFromCgi() { return _pipeFromCgi; }
pid_t		CgiHandler::getCgiPid() { return _cgiPid; }
std::string	CgiHandler::getPostData() { return _postData; }
size_t		CgiHandler::getPostDataOffset() { return _postDataOffset; }
int			CgiHandler::getWaitpidRes() { return _waitpidRes; }

CgiHandler::CgiHandler(HttpConnectionHandler conn) {
	if (conn.getCgiType() == PYTHON)
		_pathToInterpreter = conn.getLocationBlock()->cgiPathPython + "/python3";
	else if (conn.getCgiType() == PHP)
		_pathToInterpreter = conn.getLocationBlock()->cgiPathPHP + "/php-cgi";

	std::string currentPath = std::filesystem::current_path();
	std::string root = conn.getConf()->getRootViaLocation("/");
	_pathToScript = currentPath + root + conn.getFilePath();

	_execveArgs[0] = (char * )_pathToInterpreter.c_str();
	_execveArgs[1] = (char * )_pathToScript.c_str();
	_execveArgs[2] = NULL;

	_postData = conn.getBody();

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

void CgiHandler::printCgiInfo() {
	std::cout << GREEN << "Execve Args: " << DEFAULT_COLOR << std::endl;
	for (int i = 0; _execveArgs[i] != NULL; i++)
		std::cout << _execveArgs[i] << std::endl;
	std::cout << GREEN << "Execve Env: " << DEFAULT_COLOR << std::endl;
	for (int i = 0; _execveEnv[i] != NULL; i++)
		std::cout << _execveEnv[i] << std::endl;
}

void CgiHandler::executeCgi() {
    // a pipe for sending data to CGI process (parent writes, child reads)
    if (pipe(_pipeToCgi) == -1) {
        std::cerr << "Error creating pipe to CGI" << std::endl;
        return;
    }

    // a pipe for recieving data from the CGI process (child writes, parent reads)
    if (pipe(_pipeFromCgi) == -1) {
        std::cerr << "Error creating pipe from CGI" << std::endl;
        close(_pipeToCgi[0]);
		_pipeToCgi[0] = -1;
        close(_pipeToCgi[1]);
		_pipeToCgi[1] = -1;
        return;
    }

    _cgiPid = fork();
    if (_cgiPid < 0) {
        std::cerr << "Error forking process" << std::endl;
        close(_pipeToCgi[0]);
		_pipeToCgi[0] = -1;
		close(_pipeToCgi[1]);
		_pipeToCgi[1] = -1;
        close(_pipeFromCgi[0]);
		_pipeFromCgi[0] = -1;
		close(_pipeFromCgi[1]);
		_pipeFromCgi[1] = -1;
        return;
    }

    if (_cgiPid == 0) {
        close(_pipeToCgi[1]);   // child doesn't write to pipeToCgi.
		_pipeToCgi[1] = -1;
        close(_pipeFromCgi[0]); // child doesn't read from pipeFromCgi.
		_pipeFromCgi[0] = -1;

        // redirect stdin to read from _pipeToCgi[0]
        if (dup2(_pipeToCgi[0], STDIN_FILENO) == -1) {
            std::cerr << "dup2 error for STDIN in child" << std::endl;
            exit(EXIT_FAILURE);
        }
        // redirect stdout to write to _pipeFromCgi[1]
        if (dup2(_pipeFromCgi[1], STDOUT_FILENO) == -1) {
            std::cerr << "dup2 error for STDOUT in child" << std::endl;
            exit(EXIT_FAILURE);
        }
		// we dupped them, so we can close them
        close(_pipeToCgi[0]);
		_pipeToCgi[0] = -1;
        close(_pipeFromCgi[1]);
		_pipeFromCgi[1] = -1;
		for (int i = 0; i < 13; i++)
			std::cerr << "\t\t\t===>" << _execveEnv[i] << std::endl;
        execve(_execveArgs[0], _execveArgs, _execveEnv);
        std::cerr << "execve error in child" << std::endl; // handle better
        exit(EXIT_FAILURE);
    } else {
        close(_pipeToCgi[0]);   // parent does not read from pipeToCgi.
		_pipeToCgi[0] = -1;
        close(_pipeFromCgi[1]); // parent does not write to pipeFromCgi.
		_pipeFromCgi[1] = -1;

        // set the parent's pipe file descriptors to non-blocking mode.
		// chatgpt says this is how you do it:
        int flags = fcntl(_pipeToCgi[1], F_GETFL, 0);
        if (flags == -1)
			flags = 0;
        fcntl(_pipeToCgi[1], F_SETFL, flags | O_NONBLOCK);

        flags = fcntl(_pipeFromCgi[0], F_GETFL, 0);
        if (flags == -1)
			flags = 0;
        fcntl(_pipeFromCgi[0], F_SETFL, flags | O_NONBLOCK);

        // Attempt to write POST data (if any) to the CGI process in a non-blocking fashion.
        if (!_postData.empty()) {
            ssize_t written = write(_pipeToCgi[1],
                                    _postData.c_str() + _postDataOffset,
                                    _postData.size() - _postDataOffset);
            if (written > 0) {
                _postDataOffset += written;
                if (_postDataOffset == _postData.size()) {
                    // all POST data has been written, close the write end to signal EOF.
                    close(_pipeToCgi[1]);
                    _pipeToCgi[1] = -1;
                }
            }
            else {
				// do something
			}
        } else {
            // no POST data to send, close the write end.
            close(_pipeToCgi[1]);
            _pipeToCgi[1] = -1;
        }

		int status;
		_waitpidRes = waitpid(_cgiPid, &status, 0);
    }
}
