#include "HttpConnectionHandler.hpp"

std::string HttpConnectionHandler::getReasonPhrase(int statusCode)
{
    static const std::map<int, std::string> reasonPhrases =
    {
	    {200, "OK"},
	    {301, "Moved Permanently"},
	    {400, "Bad Request"},
	    {401, "Unauthorized"},
	    {403, "Forbidden"},
	    {404, "Not Found"},
	    {405, "Method Not Allowed"},
	    {409, "Conflict"},
	    {412, "Precondition Failed"},
	    {413, "Payload Too Large"},
	    {500, "Internal Server Error"},
	    {501, "Not Implemented"},
	    {503, "Service Unavailable"}
	    // add more when needed!
    };

    auto it = reasonPhrases.find(statusCode);
    if (it != reasonPhrases.end()) {
	    return it->second;
    }
    return "Unknown Status";
}

std::string HttpConnectionHandler::getCurrentHttpDate()
{
    std::time_t t = std::time(nullptr);
    struct tm* tm_info = std::gmtime(&t);

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%a, %d %b %Y %H:%M:%S GMT");
    
    return oss.str();
}

std::string HttpConnectionHandler::createHttpErrorResponse(int error)
{
	std::ostringstream			response;
	std::map<int, std::string>	errorPages = conf->getErrorPages();

	response << "HTTP/1.1 " << error << " " << getReasonPhrase(error) << "\r\n";
	std::string errorPath;
	if (errorPages.find(error) != errorPages.end()) {
		errorPath = errorPages[error];
	}
	else {
		logError("Error page " + std::to_string(error) + " Not found");
		exit(42);
	}
	
	std::ifstream file(errorPath.c_str());
	if (!file.is_open()) {
		logError("Cant open error page location " + errorPath);
		exit(42);
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string body = buffer.str();

	response << "Date: " << getCurrentHttpDate() <<  "\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: close\r\n\r\n";
	response << body;

	return response.str();
}
