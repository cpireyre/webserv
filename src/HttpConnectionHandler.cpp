#include "HttpConnectionHandler.hpp"

HttpConnectionHandler::HttpConnectionHandler()
	: method(""), path(""), originalPath(""), httpVersion(""), body(""),
	clientSocket(-1), filePath(""), queryString(""), extension(""),
	cgiType(NONE), conf(nullptr), locBlock(nullptr), errorCode(0)
{}

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
