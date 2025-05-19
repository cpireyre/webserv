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

#define MAX_URI_LENGTH 1024

typedef enum {
	S_Error,
	S_Again,
	S_Done,
	S_ClosedConnection,
	S_ReadBody,
} HandlerStatus;

typedef std::map<string, string> HeadersMap;

struct ParsedPartInfo
{
    bool		isFile = false;
    std::string		contentDispositionFilename;
    std::string_view	data;
};

struct FileUploadResult
{
    bool success = false;
    std::string originalFilename;
    std::string finalFilename;
    std::string savedPath;
    size_t fileSize = 0;
    std::string message;
};

class HttpConnectionHandler
{
	private:
		string							method;
		string							path;
		string							originalPath;
		string							httpVersion;
		string							body;
		std::map<string, string>				headers;
		std::string						chunkRemainder;
		int							clientSocket;

		string							filePath; // Everything in URI before the question mark
		string							queryString; // Everything in URI after the question mark
		string							extension;
		CgiTypes						cgiType;

		Configuration						*conf;
		LocationBlock						*locBlock;

		//response stuff
		int							errorCode;
		string							PORT;
		string 							IP;

		string							response;
		bool							fileServ;
		int							bSent;

		//Parsing
		bool		getMethodPathVersion(std::istringstream &requestStream);
		bool		getHeaders(std::istringstream &requestStream);
		bool		getBody(std::string &rawRequest);
		std::string	getContentType(const std::string &path);
		HandlerStatus	handleFirstChunks(std::string &chunkData);
		bool		hexStringToSizeT(const std::string& hexStr, size_t& out);
		bool		stringPercentDecoding(const std::string &original,std::string &decoded);

		//Creating HTTP response
		string	getDefaultErrorPage500();
		string	getDefaultErrorPage400();
		string	getDefaultErrorPage408();
		string	getReasonPhrase(int statusCode);
		string	getCurrentHttpDate();

		void		handleGetRequest();
		void		handleGetDirectory();
		void		checkFileToServe(string &filePath);

		void		handleDeleteRequest();
		void		deleteDirectory();

		void		handlePostRequest();
		bool		validateUploadRights();
		bool		handleFileUpload();
		FileUploadResult uploadFile(ParsedPartInfo partInfo);


		int			matchServerName(const std::string& pattern, const std::string& host);
		void		findConfig();
		bool		isMethodAllowed(LocationBlock *block, string &method);
		LocationBlock	*findLocationBlock(std::vector<LocationBlock> &blocks, LocationBlock *current);

		
	public:
		//Basics
		HttpConnectionHandler();
		~HttpConnectionHandler();
		HttpConnectionHandler(const HttpConnectionHandler&) = delete;
		void resetObject();

		void		findInitialConfig();

		string	rawRequest;

		HandlerStatus	parseRequest();
		HandlerStatus	readBody();
		void	handleRequest();
		bool		checkLocation();
		CgiTypes		checkCgi();
		HandlerStatus	serveCgi(CgiHandler &cgiHandler);

		//creating HTTP response
		string	createHttpErrorResponse(int error);
		string	createErrorResponse(int error); //placeholder for know
		string	createHttpResponse(int statusCode, const string &body, const string &contentType);
		string	createHttpRedirectResponse(int statusCode, const string &location);
		HeadersMap createDefaultHeaders();
		string getErrorPageBody(int error);
		HandlerStatus serveFile();

		/* Will calculate and append Content-Length header with the right value. */
		string serializeResponse(int status, HeadersMap& headers, const string& body);

		// Getters
		int						getErrorCode() const { return errorCode; }
		int						getClientSocket() const { return clientSocket; }
		int						getBSent() const { return bSent; }
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
		const string				&getResponse() const { return response; }
		bool				getFileServ() const { return fileServ; }
		CgiTypes					getCgiType() const { return cgiType; }
		const std::map<string, string>	&getHeaders() const { return headers; }

		// Setters
		void	setResponse(std::string newResponse) {response = newResponse;}
		void	setClientSocket(int socket) { clientSocket = socket; }
		void	setErrorCode(int err) { errorCode = err; }
		void	setConfig(Configuration *config) { conf = config; }
		void	setIP(string ip) { IP = ip; }
		void	setPORT(string port) { PORT = port; }

};

std::ostream& operator<<(std::ostream& os, const HttpConnectionHandler& handler);
