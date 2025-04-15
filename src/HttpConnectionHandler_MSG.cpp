#include "HttpConnectionHandler.hpp"

std::string HttpConnectionHandler::getCurrentHttpDate()
{
    std::time_t t = std::time(nullptr);
    struct tm* tm_info = std::gmtime(&t);

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%a, %d %b %Y %H:%M:%S GMT");
    
    return oss.str();
}

std::string HttpConnectionHandler::getDefaultErrorPage500()
{
	std::string body =
		"<html><head><title>500 Internal Server Error</title></head>"
		"<body><h1>500 Internal Server Error</h1>"
		"<p>Sorry, something went wrong while handling your request.</p>"
		"</body></html>";

	std::ostringstream oss;
	oss << "HTTP/1.1 500 Internal Server Error\r\n";
	oss << "Content-Type: text/html; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.length() << "\r\n";
	oss << "Connection: Keep-Alive\r\n";
	oss << "Date: " << getCurrentHttpDate() << "\r\n";
	oss << "\r\n";
	oss << body;
	return oss.str();
}

std::string HttpConnectionHandler::getDefaultErrorPage400()
{
	std::string body =
		"<html><head><title>400 Bad request</title></head>"
		"<body><h1>400 Bad request</h1>"
		"<p>Sorry, something went wrong while handling your request.</p>"
		"</body></html>";

	std::ostringstream oss;
	oss << "HTTP/1.1 400 Bad request\r\n";
	oss << "Content-Type: text/html; charset=UTF-8\r\n";
	oss << "Content-Length: " << body.length() << "\r\n";
	oss << "Connection: Keep-Alive\r\n";
	oss << "Date: " << getCurrentHttpDate() << "\r\n";
	oss << "\r\n";
	oss << body;
	return oss.str();
}

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
	    {408, "Request Timeout"},
	    {409, "Conflict"},
	    {412, "Precondition Failed"},
	    {413, "Payload Too Large"},
	    {500, "Internal Server Error"},
	    {501, "Not Implemented"},
	    {503, "Service Unavailable"},
	    {505, "HTTP Version not supported"}
	    // add more when needed!
    };

    auto it = reasonPhrases.find(statusCode);
    if (it != reasonPhrases.end()) {
	    return it->second;
    }
    return "Unknown Status";
}

/* creates an HTTP response string with the given status code, body content, and content type
 *
 * generates basic HTTP response string for the server to send to client
 * the response includes a status line (HTTP version, status code, and status text),
 * headers for content length, content type, and connection status, followed by the body
 * very basic version, done for GET initially. missing time stamp also?
 *
 * @param statusCode The HTTP status code (200 for OK, 404 for Not Found etc)
 * @param body The body of the HTTP response (HTML content, error message, etc)
 * @param contentType The content type of the response body ("text/html", "application/json", etc).
 *
 * @return A string representing the full HTTP response, ready to be sent to the client.
 *
 * Example:
 *   std::string response = createHttpResponse(200, "<h1>Success</h1>", "text/html");
 */
std::string	HttpConnectionHandler::createHttpResponse(int statusCode, const std::string &body, const std::string &contentType)
{
	std::string statusText = statusCode < 400 ? "OK" : "Not Found";

	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
	response << "Date: " << getCurrentHttpDate() <<  "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Connection: Keep-Alive\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

/* handles creating HTTP response when DELETE or GET request gets 301 for example
 * when trying to access directory with no trailing /, example DELETE /directory HTTP/1.1
 */
std::string	HttpConnectionHandler::createHttpRedirectResponse(int statusCode, const std::string &location)
{
	std::ostringstream response;
	if (statusCode == 301) {
		response << "HTTP/1.1 " << std::to_string(statusCode) << " Moved Permanently\r\n";
		response << "Location: " << location + "/\r\n";
	}
	else if (statusCode == 307) {
		response << "HTTP/1.1 " << std::to_string(statusCode) << " Temporary Redirection\r\n";
		response << "Location: " << location << "\r\n";
	}
	response << "Date: " << getCurrentHttpDate() <<  "\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: 0\r\n\r\n";
	return response.str();
}


std::string HttpConnectionHandler::createHttpErrorResponse(int error)
{
	std::ostringstream		response;
	if (!conf) {
		logError("Error response func called with no configuration file match yet, ok if happens during parsing");
		return getDefaultErrorPage400();
	}
	std::map<int, std::string>	errorPages = conf->getErrorPages();

	response << "HTTP/1.1 " << error << " " << getReasonPhrase(error) << "\r\n";
	std::string errorPath;
	if (errorPages.find(error) != errorPages.end()) {
		errorPath = errorPages[error];
	}
	else {
		logError("Error page " + std::to_string(error) + " Not found");
		return getDefaultErrorPage500();
	}

	errorPath = "." + errorPath;
	std::ifstream file(errorPath.c_str());
	if (!file.is_open()) {
		logError("Cant open error page location " + errorPath);
		return getDefaultErrorPage500();
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string body = buffer.str();

	response << "Date: " << getCurrentHttpDate() <<  "\r\n";
	if (error == 405) {
		response << "Allow:";
		for (const auto &method : locBlock->methods) {
			response << " " << method;
		}
		response << "\r\n";
	}
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Connection: Keep-Alive\r\n\r\n";
	response << body;

	return response.str();
}
