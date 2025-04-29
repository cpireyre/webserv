#include "HttpConnectionHandler.hpp"
#include "Logger.hpp"

string HttpConnectionHandler::getCurrentHttpDate()
{
    std::time_t t = std::time(nullptr);
    struct tm* tm_info = std::gmtime(&t);

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%a, %d %b %Y %H:%M:%S GMT");
    
    return oss.str();
}

string HttpConnectionHandler::getDefaultErrorPage500()
{
	return createHttpErrorResponse(500);
}

string HttpConnectionHandler::getDefaultErrorPage400()
{
	return createHttpErrorResponse(400);
}

string HttpConnectionHandler::getDefaultErrorPage408()
{
	return createHttpErrorResponse(408);
}

string HttpConnectionHandler::getReasonPhrase(int statusCode)
{
    static const std::map<int, string> reasonPhrases =
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
 *   string response = createHttpResponse(200, "<h1>Success</h1>", "text/html");
 */
string	HttpConnectionHandler::createHttpResponse(int statusCode, const string &body, const string &contentType)
{
	HeadersMap h = createDefaultHeaders();
	h["Content-Type"] = contentType;

	return serializeResponse(statusCode, h, body);
}

/* handles creating HTTP response when DELETE or GET request gets 301 for example
 * when trying to access directory with no trailing /, example DELETE /directory HTTP/1.1
 */
string	HttpConnectionHandler::createHttpRedirectResponse(int statusCode, const string &location)
{
	HeadersMap h = createDefaultHeaders();

	if (statusCode == 301) {
		h["Location"] = location + "/";
	}
	else if (statusCode == 307) {
		h["Location"] = location;
	}
	h["Content-Type"] = "text/html";
	return serializeResponse(statusCode, h, "");
}

string HttpConnectionHandler::getErrorPageBody(int error)
{
	if (!conf) /* Should return hardcoded defaults in my opinion */
	{
		if (error == 408)
			return "<html><head><title>408 Request Timeout</title></head>"
        "<body><h1>408 Request Timeout</h1>"
        "<p>Your browser took too long to send a complete request.</p>"
        "</body></html>";
		return "<html><head><title>400 Bad request</title></head>"
		"<body><h1>400 Bad request</h1>"
		"<p>Sorry, something went wrong while handling your request.</p>"
		"</body></html>";
	}
	std::map<int, string> errorPages = conf->getErrorPages();
	string errorPath;
	
	if (errorPages.find(error) != errorPages.end())
	{
		errorPath = "." + errorPath;
		std::ifstream file(errorPath.c_str());
		if (file.is_open()) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			return buffer.str();
		}
		logError("Can't open error page location " + errorPath);
	}
	else
		logError("Error page " + std::to_string(error) + " Not found");

	return "<html><head><title>500 Internal Server Error</title></head>"
		"<body><h1>500 Internal Server Error</h1>"
		"<p>Sorry, something went wrong while handling your request.</p>"
		"</body></html>";
}

string HttpConnectionHandler::createHttpErrorResponse(int error)
{
	string body = getErrorPageBody(error);
	HeadersMap h = createDefaultHeaders();
	h["Content-Type"] = "text/html";
	h["Connection"] = "close";

	if (error == 405 && locBlock)
	{
		string allowValue;
		for (const auto &method : locBlock->methods)
		{
			if (!allowValue.empty())
				allowValue += " ";
			allowValue += method;
		}
		h["Allow"] = allowValue;
	}
	
	return serializeResponse(error, h, body);
}

string HttpConnectionHandler::createErrorResponse(int error)
{
	fileServ = true;
	HeadersMap h = createDefaultHeaders();
	h["Content-Type"] = "text/html";
	h["Connection"] = "close";

	if (error == 405 && locBlock)
	{
		string allowValue;
		for (const auto &method : locBlock->methods)
		{
			if (!allowValue.empty())
				allowValue += " ";
			allowValue += method;
		}
		h["Allow"] = allowValue;
	}

	if (!conf)
	{
		//send hard coded 400 back
		fileServ = false;
		return  "HTTP/1.1 400 Bad Request\r\n"
			"Content-Type: text/html; charset=UTF-8\r\n"
			"Content-Length: 162\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html>"
			"<head><title>400 Bad Request</title></head>"
			"<body>"
			"<h1>400 Bad Request</h1>\r\n"
			"<p>Your browser sent a request that this server could not understand.</p>"
			"</body>"
			"</html>";
	}
	std::map<int, string> errorPages = conf->getErrorPages();
	string errorPath;

	if (errorPages.count(error))
	{
		errorPath = "." + errorPages[error];
		std::ifstream file(errorPath.c_str());
		if (file.is_open()) {
			file.seekg (0, file.end);
			std::streamsize conLen = file.tellg();
			std::stringstream buffer;
			buffer << "HTTP/1.1 " << error << " " << getReasonPhrase(error) << "\r\n";
			for (const auto& [key, value] : headers)
				buffer << key << ": " << value << "\r\n";
			buffer << "Content-Length: " << conLen << "\r\n";
			buffer << "\r\n";
			path = errorPath;
			return buffer.str();
		}
		else {
			logError("Can't open error page location " + errorPath);
		}
	}
	else
		logError("Error page " + std::to_string(error) + " Not found, need to be added to configs defaults!!!!!");
	fileServ = false;
	return  "HTTP/1.1 500 Internal Server Error\r\n"
		"Content-Type: text/html; charset=UTF-8\r\n"
		"Content-Length: 178\r\n"
		"Connection: close\r\n"
		"\r\n"
		"<html>"
		"<head><title>500 Internal Server Error</title></head>"
		"<body>"
		"<h1>500 Internal Server Error</h1>\r\n"
		"<p>Sorry, something went wrong while handling your request.</p>"
		"</body>"
		"</html>";
}
