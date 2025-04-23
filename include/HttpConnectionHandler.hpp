#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>
#include <map>
#include <sys/socket.h>
#include <unistd.h>
#include <ctime>
#include "CgiHandler.hpp"
#include "Configuration.hpp"

extern std::vector<Configuration> serverMap;
using std::string;

typedef enum {
	S_Error,
	S_Again,
	S_Done,
	S_ClosedConnection,
	S_ReadBody,
} HandlerStatus;

typedef std::map<string, string> HeadersMap;
class HttpConnectionHandler
{
	private:
		string							method;
		string							path;
		string							originalPath;
		string							httpVersion;
		string							body;
		std::map<string, string>				headers;
		HeadersMap							responseHeaders;
		int								clientSocket;

		string							filePath; // Everything in URI before the question mark
		string							queryString; // Everything in URI after the question mark
		string							extension;
		CgiTypes							cgiType;

		Configuration							*conf;
		LocationBlock							*locBlock;

		//response stuff
		int		errorCode;
		string							PORT;
		string 							IP;

		//Parsing
		bool		getMethodPathVersion(std::istringstream &requestStream);
		bool		getHeaders(std::istringstream &requestStream);
		bool		getBody(string &rawRequest);
		string	getContentType(const string &path);

		//Creating HTTP response
		string	getDefaultErrorPage500();
		string	getDefaultErrorPage400();
		string	getDefaultErrorPage408();
		string	getReasonPhrase(int statusCode);
		string	getCurrentHttpDate();

		void		handleGetRequest();
		void		handleGetDirectory();
		void		serveFile(string &filePath);

		void		handleDeleteRequest();
		void		deleteDirectory();

		void		handlePostRequest();
		bool		validateUploadRights();
		bool		handleFileUpload();
		bool		processMultipartPart(const string& part, string &responseBody);

		bool		checkLocation();
		int		matchServerName(const string& pattern, const string& host);
		void		findConfig();
		bool		isMethodAllowed(LocationBlock *block, string &method);
		LocationBlock	*findLocationBlock(std::vector<LocationBlock> &blocks, LocationBlock *current);

		CgiTypes	checkCgi();
		
	public:
		//Basics
		HttpConnectionHandler();
		~HttpConnectionHandler();
		HttpConnectionHandler(const HttpConnectionHandler&) = delete;
		void resetObject();

		string	rawRequest;

		HandlerStatus	parseRequest();
		HandlerStatus	readBody();
		void	handleRequest();

		//creating HTTP response
		string	createHttpErrorResponse(int error);
		string	createHttpResponse(int statusCode, const string &body, const string &contentType);
		string	createHttpRedirectResponse(int statusCode, const string &location);
		HeadersMap createDefaultHeaders();
		string getErrorPageBody(int error);

		/* Will calculate and append Content-Length header with the right value. */
		string serializeResponse(int status, HeadersMap& headers, const string& body);

		// Getters
		int						getErrorCode() const { return errorCode; }
		int						getClientSocket() const { return clientSocket; }
		const string				&getMethod() const { return method; }
		const string				&getPath() const { return path; }
		const string				&getOriginalPath() const { return originalPath; }
		const string				&getHttpVersion() const { return httpVersion; }
		const string				&getBody() const { return body; }
		const Configuration 				*getConf() const { return conf; }
		const LocationBlock				*getLocationBlock() const { return locBlock;}
		const string				&getFilePath() const { return filePath; }
		const string				&getQueryString() const { return queryString; }
		const string				&getExtension() const { return extension; }
		CgiTypes					getCgiType() const { return cgiType; }
		const std::map<string, string>	&getHeaders() const { return headers; }

		// Setters
		void	setClientSocket(int socket) { clientSocket = socket; }
		void	setErrorCode(int err) { errorCode = err; }
		void	setConfig(Configuration *config) { conf = config; }
		void	setIP(string ip) { IP = ip; }
		void	setPORT(string port) { PORT = port; }

};

std::ostream& operator<<(std::ostream& os, const HttpConnectionHandler& handler);
