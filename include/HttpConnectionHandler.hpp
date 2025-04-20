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

typedef enum {
	S_Error,
	S_Again,
	S_Done,
	S_ClosedConnection,
	S_ReadBody,
} HandlerStatus;

class HttpConnectionHandler
{
	private:
		std::string							method;
		std::string							path;
		std::string							originalPath;
		std::string							httpVersion;
		std::string							body;
		std::map<std::string, std::string>				headers;
		int								clientSocket;

		std::string							filePath; // Everything in URI before the question mark
		std::string							queryString; // Everything in URI after the question mark
		std::string							extension;
		CgiTypes							cgiType;

		Configuration							*conf;
		LocationBlock							*locBlock;

		//response stuff
		int		errorCode;
		std::string							PORT;
		std::string 							IP;

		//Parsing
		bool		getMethodPathVersion(std::istringstream &requestStream);
		bool		getHeaders(std::istringstream &requestStream);
		bool		getBody(std::string &rawRequest);
		std::string	getContentType(const std::string &path);

		//Creating HTTP response
		std::string	getDefaultErrorPage500();
		std::string	getDefaultErrorPage400();
		std::string getDefaultErrorPage408();
		std::string	getReasonPhrase(int statusCode);
		std::string	getCurrentHttpDate();

		void		handleGetRequest();
		void		handleGetDirectory();
		void		serveFile(std::string &filePath);

		void		handleDeleteRequest();
		void		deleteDirectory();

		void		handlePostRequest();
		bool		validateUploadRights();
		bool		handleFileUpload();
		bool		processMultipartPart(const std::string& part, std::string &responseBody);

		bool		checkLocation();
		int		matchServerName(const std::string& pattern, const std::string& host);
		void		findConfig();
		bool		isMethodAllowed(LocationBlock *block, std::string &method);
		LocationBlock	*findLocationBlock(std::vector<LocationBlock> &blocks, LocationBlock *current);

		CgiTypes	checkCgi();
		
	public:
		//Basics
		HttpConnectionHandler();
		~HttpConnectionHandler();
		HttpConnectionHandler(const HttpConnectionHandler&) = delete;
		void resetObject();

		std::string	rawRequest;

		HandlerStatus	parseRequest();
		HandlerStatus	readBody();
		void	handleRequest();

		//creating HTTP response
		std::string	createHttpErrorResponse(int error);
		std::string	createHttpResponse(int statusCode, const std::string &body, const std::string &contentType);
		std::string	createHttpRedirectResponse(int statusCode, const std::string &location);

		// Getters
		int						getErrorCode() const { return errorCode; }
		int						getClientSocket() const { return clientSocket; }
		const std::string				&getMethod() const { return method; }
		const std::string				&getPath() const { return path; }
		const std::string				&getOriginalPath() const { return originalPath; }
		const std::string				&getHttpVersion() const { return httpVersion; }
		const std::string				&getBody() const { return body; }
		const Configuration 				*getConf() const { return conf; }
		const LocationBlock				*getLocationBlock() const { return locBlock;}
		const std::string				&getFilePath() const { return filePath; }
		const std::string				&getQueryString() const { return queryString; }
		const std::string				&getExtension() const { return extension; }
		CgiTypes					getCgiType() const { return cgiType; }
		const std::map<std::string, std::string>	&getHeaders() const { return headers; }

		// Setters
		void	setClientSocket(int socket) { clientSocket = socket; }
		void	setConfig(Configuration *config) { conf = config; }
		void	setIP(std::string ip) { IP = ip; }
		void	setPORT(std::string port) { PORT = port; }

};

std::ostream& operator<<(std::ostream& os, const HttpConnectionHandler& handler);
