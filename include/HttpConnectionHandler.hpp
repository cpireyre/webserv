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

class HttpConnectionHandler
{
	private:
		std::string				method;
		std::string				path;
		std::string				httpVersion;
		std::string				body;
		std::map<std::string, std::string>	headers;
		int					clientSocket;

		bool	getMethodPathVersion(std::istringstream &requestStream);
		bool	getHeaders(std::istringstream &requestStream);
		bool	getBody(std::string &rawRequest);

		std::string getContentType(const std::string &path);
		void	handleGetRequest();
		void	handlePostRequest();
		void	handleDeleteRequest();
		bool	handleFileUpload();
		bool	processMultipartPart(const std::string& part,
				std::string &responseBody);

	public:
		// Additions to Jere's work
		bool					isCgi;
		CgiTypes				type;
		std::string				pathToInterpreter;
		Configuration			serverInQuestion;
		LocationBlock			locInQuestion;
		// End of additions

		//Parse Http request
		bool	parseRequest();

		//create Http response
		void	handleRequest();
		std::string	createHttpResponse(int statusCode, const std::string &body, const std::string &contentType);

		// Getters
		const int getClientSocket() const { return clientSocket; }
		const std::string getMethod() const { return method; }
		const std::string getPath() const { return path; }
		const std::string getHttpVersion() const { return httpVersion; }
		const std::string getBody() const { return body; }
		const std::map<std::string, std::string> &getHeaders() const { return headers; }

		// Setters
		void setClientSocket(int socket) { clientSocket = socket; }

		//loggers
		static void logError(const std::string& message)
		{
			std::time_t now = std::time(nullptr);
			char timeBuf[20];
			strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
			std::cerr << "[" << timeBuf << "] ERROR: " << message << std::endl;
		}

		static void logInfo(const std::string& message)
		{
			std::time_t now = std::time(nullptr);
			char timeBuf[20];
			strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
			std::cout << "[" << timeBuf << "] INFO: " << message << std::endl;
		}
};
