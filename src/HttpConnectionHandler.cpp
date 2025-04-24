#include "HttpConnectionHandler.hpp"
#include "Logger.hpp"

HttpConnectionHandler::HttpConnectionHandler()
	: method(""), path(""), originalPath(""), httpVersion(""), body(""),
	clientSocket(-1), filePath(""), queryString(""), extension(""),
	cgiType(NONE), conf(nullptr), locBlock(nullptr), errorCode(0),
	PORT("0000"), IP("0000"), response(""), fileServ(false), bSent(0) {}

//add socket closing to destructor if needed
HttpConnectionHandler::~HttpConnectionHandler() {}

/* Clears the object
 * current implementation leaves socket and conf as it was
 */
void HttpConnectionHandler::resetObject()
{
	method.clear();
	path.clear();
	originalPath.clear();
	httpVersion.clear();
	body.clear();
	headers.clear();
	filePath.clear();
	queryString.clear();
	extension.clear();
	cgiType = NONE;
	locBlock = nullptr;
	errorCode = 0;
	rawRequest.clear();
	chunkRemainder.clear();
	response.clear();
	bSent = 0;
	fileServ = false;
}

std::ostream& operator<<(std::ostream& os, const HttpConnectionHandler& handler)
{
	os << "=== HttpConnectionHandler Debug Info ===\n";
	os << "Method: " << handler.getMethod() << "\n";
	os << "Path: " << handler.getPath() << "\n";
	os << "HTTP Version: " << handler.getHttpVersion() << "\n";
	os << "--- Headers ---\n";
	for (const auto& [key, value] : handler.getHeaders())
		os << key << ": " << value << "\n";
	os << "--- Body ---\n" << handler.getBody() << "\n";
	os << "Error Code: " << handler.getErrorCode() << "\n";
	os << "Client Socket: " << handler.getClientSocket() << "\n";
	os << "Original Path: " << handler.getOriginalPath() << "\n";
	os << "File Path: " << handler.getFilePath() << "\n";
	os << "Query String: " << handler.getQueryString() << "\n";
	os << "Extension: " << handler.getExtension() << "\n";
	os << "CGI Type: " << static_cast<int>(handler.getCgiType()) << "\n";
	if (handler.getConf())
		os << "conf: set\n";
	else
		os << "conf: NULL\n";
	if (handler.getLocationBlock())
		os << "Location block: set\n";
	else
		os << "Location block: NULL\n";
	os << "=========================================\n";

	return os;
}

HeadersMap HttpConnectionHandler::createDefaultHeaders()
{
	HeadersMap	res;
	
	res["Date"] = getCurrentHttpDate();
	headers["Connection"] = "Keep-Alive";
	return (res);
}

/* Will calculate and append Content-Length header with the right value. */
string HttpConnectionHandler::
serializeResponse(int status, HeadersMap& headers, const string& body)
{
	std::ostringstream	response;

	headers["Content-Length"] = std::to_string(body.size());
	response << "HTTP/1.1 " << status << " " << getReasonPhrase(status) << "\r\n";
	for (const auto& [key, value] : headers)
		response << key << ": " << value << "\r\n";
	response << "\r\n" << body;
	return (response.str());
}
