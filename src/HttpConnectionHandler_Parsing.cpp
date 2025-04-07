#include "HttpConnectionHandler.hpp"

/* 
 * Need to check for missing headers?
 * like host header for HTTP/1.1, or Content-Length for POST/PUT
 * extra spaces accepted ot not? currently strict formatting
 * need check if the are body or not, conditions for that?
 * ignore body if its no used?
 * line end is gonna be \r\n or \n - need to change regex in case
 * handling body size too big?
 * actual responses (400 bad request etc)
 */

/*  Parses an HTTP request from the client socket
 *
 * reads data from the client socket into a buffer, turns it
 * into a raw request string. and processes the HTTP method, headers, and body (if needed)
 *
 * It stops reading once the request is fully received (encountering "\r\n\r\n") or if an error occurs.
 * the method handles both GET and
 * methods that may include a body (POST for now) also handle parsing and capturing body
 *
 * Returns:
 * - true if the request was successfully parsed.
 * - false if there was an error reading the request, the request is empty, or if parsing the
 *   HTTP method, headers, or body fails.
 *
 * Example:
 *   if (handler.parseRequest()) {
 *       // Process the request
 *   } else {
 *       // Handle the error
 *   }
 */
bool	HttpConnectionHandler::parseRequest()
{
	char		buffer[8192];
	std::string	rawRequest;
	int			bRead;

	while ((bRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0)
	{
		buffer[bRead] = '\0';
		rawRequest += buffer;
		if (rawRequest.find("\r\n\r\n") != std::string::npos) {
			break;
		}
	}
	if (bRead <= 0) {
		HttpConnectionHandler::logError("Reading from the socket");
		return false;
	}
	else if (rawRequest.empty()) {
		HttpConnectionHandler::logError("Empty request received");
		return false;
	}

	std::istringstream requestStream(rawRequest);
	if (!getMethodPathVersion(requestStream)) {
		return false;
	}
	if (!getHeaders(requestStream)) {
		return false;
	}
	//if there is body, read it completely
	if (method == "POST" && headers.count("Content-Length")) {
		if (!getBody(rawRequest))
			return false;
	}
	return true;
}

/* parses the first line of the HTTP request to get the HTTP method, path, and version
 * 
 * reads the first line of the request and uses regex to match the 
 * HTTP method (GET, POST, DELETE), path ("/index.html", /), and the HTTP version ("HTTP/1.1", "HTTP/1.0")
 * validates method and version.
 * checks for correct syntax, current is single space between, path starts with / etc.
 *
 * Params (stringstream containing the HTTP request)
 *
 * Returns:
 * - true if the first line is successfully parsed and the method is valid.
 * - false if parsing fails, the format is invalid, or the method is unsupported.
 */
bool	HttpConnectionHandler::getMethodPathVersion(std::istringstream &requestStream)
{
	std::string	firstLine;

	if (!std::getline(requestStream, firstLine) || firstLine.empty()) {
		HttpConnectionHandler::logError("Failed to receive first line");
		return false;
	}

	std::regex	httpRegex(R"(^([A-Z]+) (\/\S+) (HTTP\/[1]\.[1,0])\r$)");
	std::smatch	matches;
	if (std::regex_match(firstLine, matches, httpRegex)) {
		method = matches[1];
		path = matches[2];
		httpVersion = matches[3];
	}
	else {
		HttpConnectionHandler::logError("Invalid syntax on request first line: " + firstLine);
		return false;
	}

	if (method != "GET" && method != "POST" && method != "DELETE") { //405
		HttpConnectionHandler::logError("Invalid method: " + method);
		return false;
	}

	return true;
}

/* parses the HTTP headers from the request stream.
 * 
 * reads each line from the request stream and extracts headers using a regex
 * it validates header names (ensuring they don't start or end with '-', WIP cant contain two '-'?)
 * stores headers in a map (header : value).
 * also checks that the "Host" header is present in "HTTP/1.1" requests, as required by the HTTP specification.
 * syntax accepts currently any amount of whitespace between : and value
 * 
 * Returns:
 * - true if headers are successfully parsed and the "Host" header is present for "HTTP/1.1" requests.
 * - false if any header is improperly formatted or required headers are missing.
 */
bool	HttpConnectionHandler::getHeaders(std::istringstream &requestStream)
{
	std::string	headerLine;
	std::regex	headerRegex(R"(^([A-Za-z0-9\-]+): (.+)\r$)");

	while (std::getline(requestStream, headerLine) && !headerLine.empty())
	{
		if (headerLine == "\r") {
			break;
		}
		std::smatch headerMatches;
		if (std::regex_match(headerLine, headerMatches, headerRegex)) {
			std::string headerName = headerMatches[1];
			if (headerName[0] == '-' || headerName[headerName.size() - 1] == '-') {
				HttpConnectionHandler::logError("Invalid header: " + headerName);
				return false;
			}
			std::string headerValue = headerMatches[2];
			headers[headerName] = headerValue;
		}
		else {
			HttpConnectionHandler::logError("Invalid header format: " + headerLine);
			std::cerr << "Invalid header format: " << headerLine << std::endl;
			return false;
		}
	}

	//check host header exist in 1.1
	if (httpVersion == "HTTP/1.1" && headers.find("Host") == headers.end()) {
		HttpConnectionHandler::logError("Missing Host header");
		return false;
	}
	else {
		conf = &serverMap[0];
	}
	return true;
}

/* parses the body of the HTTP request
 *
 * reads the body of an HTTP request from the socket using the "Content-Length"
 * header to determine how much data to read. It handles both extracting any existing body data
 * from the `rawRequest` string and then reading the remaining bytes from the socket until the full
 * body is received. If an error occurs (invalid "Content-Length" , connection issues), it returns false
 * checks if the initial read from socket captured whole body already, if not reads from socket again
 * !!having some trouble hanging if reading from empty socket, need attention!!
 * !!also might have trouble if Content_length is  incorrect!! (select, poll)?
 *
 * Returns:
 * - true if the body is successfully parsed and fully received.
 * - false if an error occurs (invalid "Content-Length", connection issue, ...)
 */
bool	HttpConnectionHandler::getBody(std::string &rawRequest)
{
	int		contentLength;
	int		bRead = 0;
	char		buffer[8192];
	body.clear();

	try
	{
		contentLength = std::stoi(headers["Content-Length"]);
		if (contentLength < 0) {
			HttpConnectionHandler::logError("Negative Content-Length");
			return false;
		}
	}
	catch (...)
	{
		HttpConnectionHandler::logError("Invalid Content-Length value:" + headers["Content-Length"]);
		return false;
	}

	// check if there is already part of body in rawReq and where body starts
	std::string::size_type	bodyStart = rawRequest.find("\r\n\r\n");
	if (bodyStart == std::string::npos) {
		HttpConnectionHandler::logError("Failed to find the end of the headers!");
		return false;
	}
	bodyStart += 4;
	if (bodyStart < rawRequest.size()) {
		body = rawRequest.substr(bodyStart);
	}

	//read until full body is received
	while (body.size() < static_cast<unsigned int>(contentLength))
	{
		int readSize = std::min(sizeof(buffer) - 1, contentLength - body.size());
		bRead = recv(clientSocket, buffer, readSize, 0);
		if (bRead <= 0) {
			HttpConnectionHandler::logError("Connection closed while reading body");
			return false;
		}
		body.append(buffer, bRead);
	}
	return true;
}
