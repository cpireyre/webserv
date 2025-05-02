#include "HttpConnectionHandler.hpp"
#include "Logger.hpp"

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
HandlerStatus	HttpConnectionHandler::parseRequest()
{
	char		buffer[8192];
	int		bRead;

	if (!conf)
		findInitialConfig();
	logInfo("Parsing connection on socket " + std::to_string(clientSocket));
	bRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
	if (bRead == 0)
		return S_ClosedConnection;
	if (bRead < 0) {
		logError("Reading from the socket");
		std::cout << clientSocket << std::endl;
		errorCode = 400;
		return S_Error;
	}
	buffer[bRead] = '\0';
	rawRequest += buffer;
	if (rawRequest.find("\r\n\r\n") == std::string::npos) {
		return S_Again;
	}
	else if (rawRequest.empty()) {
		logError("Empty request received");
		errorCode = 400;
		return S_Error;
	}

	std::istringstream requestStream(rawRequest);
	if (!getMethodPathVersion(requestStream)) {
		return S_Error;
	}
	if (!getHeaders(requestStream)) {
		return S_Error;
	}
	//if there is body still to be read, read it completely
	if (headers.count("Content-Length")) {
		std::string::size_type	bodyStart = rawRequest.find("\r\n\r\n");
		if (bodyStart == std::string::npos) {
			logError("Failed to find the end of the headers!");
			errorCode = 400;
			return S_Error;
		}
		bodyStart += 4;
		if (bodyStart < rawRequest.size()) {
			body = rawRequest.substr(bodyStart);
		}
	
		auto it = headers.find("Content-Length");
		if (it == headers.end()) {
			logError("Cant find Content-Length header in getBody()");
			errorCode = 400;
			return S_Error;
		}
		std::string contentLengthStr = it->second;
		int contentLengthInt;
		try
		{
			contentLengthInt = std::stoi(contentLengthStr);
			if (contentLengthInt < 0) {
				logError("Negative Content-Length");
				errorCode = 400;
				return S_Error;
			}
			else if (conf && static_cast<unsigned int>(contentLengthInt) > conf->getMaxClientBodySize())
			{
				logError("Request Body size bigger than max client body size");
				errorCode = 415;
				return S_Error;
			}
		}
		catch (const std::exception &e)
		{
			logError("Stoi Error");
			errorCode = 400;
			return S_Error;
		}
		size_t contentLength = static_cast<size_t>(contentLengthInt);

		if (body.size() < contentLength)
			return S_ReadBody;
		else if (body.size() > contentLength){
			logError("Body size bigger than Content-Length");
			errorCode = 400;
			return S_Error;
		}
		else
			return S_Done;
	}
	else if (headers.count("Transfer-Encoding") && headers["Transfer-Encoding"] == "chunked")
	{
		logInfo("Handling Chunked request");
		std::string::size_type bodyStartPos = rawRequest.find("\r\n\r\n");
		if (bodyStartPos == std::string::npos) {
			logError("Internal error: Headers parsed but \\r\\n\\r\\n not found.");
			errorCode = 500;
			return S_Error;
		}
		bodyStartPos += 4;
		std::string chunkData = rawRequest.substr(bodyStartPos);
		if (conf && chunkData.size() > conf->getMaxClientBodySize())
		{
			logError("Request Body size bigger than max client body size");
			errorCode = 415;
			return S_Error;
		}
		HandlerStatus status = handleFirstChunks(chunkData);
		if (status == S_Again) {
			return S_ReadBody;
		} else {
			return status;
		}
	}
	else if (method == "POST")
	{
		logError("POST request with no Content-Length or chunked header");
		errorCode = 411;
		return S_Error;
	}
	return S_Done;
}

/*
 */
bool	isHexa(char c)
{
	return std::isxdigit(static_cast<unsigned char>(c));
}

bool	allowedDecodeValues(int value)
{
	if (value >= '0' && value <= '9') return true;
	if (value >= 'A' && value <= 'Z') return true;
	if (value >= 'a' && value <= 'z') return true;

	switch (value) {
		case ':': case '/': case '?': case '#':
		case '[': case ']': case '@':
		case '!': case '$': case '&': case '\'':
		case '(': case ')': case '*': case '+':
		case ',': case ';': case '=':
		case '%': case ' ':
			return true;
		default:
			break;
	}
	// reject other control characters and non printable
	return false;
}

bool	HttpConnectionHandler::stringPercentDecoding(const std::string &original, std::string &decoded)
{
	decoded.clear();
	size_t	size = original.size();
	bool	slashCollapse = false;

	for (size_t i = 0; i < size; i++)
	{
		if (original[i] == '%') {
			if (i + 2 >= size || !isHexa(original[i + 1]) || !isHexa(original[i + 2])) {
				return false;
			}
			std::string hexStr = original.substr(i + 1, 2);
			try {
				int decodedValue = std::stoi(hexStr, nullptr, 16);
				if (decodedValue == '/') {
					if (slashCollapse) {
						i += 2;
						continue;
					}
					slashCollapse = true;
				}
				else {
					slashCollapse = false;
				}
				if (!allowedDecodeValues(decodedValue))
					return false;
				decoded += static_cast<unsigned char>(decodedValue);
			}
			catch (...) {
				return false;
			}
			i += 2;
		}
		else if(original[i] == '/') {
			if (slashCollapse) {
				continue;
			}
			decoded += original[i];
			slashCollapse = true;

		} 
		else {
			decoded += original[i];
			slashCollapse = false;
		}
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
		logError("Failed to receive first line");
		errorCode = 400;
		return false;
	}

	std::regex	httpRegex(R"(^([A-Z]+) (\/\S*) (HTTP\/\d(?:\.\d)?)\r$)");
	std::smatch	matches;
	if (std::regex_match(firstLine, matches, httpRegex)) {
		method = matches[1];
		path = matches[2];
		httpVersion = matches[3];
	}
	else {
		logError("Invalid syntax on request first line: " + firstLine);
		errorCode = 400;
		return false;
	}

	if (method != "GET" && method != "POST" && method != "DELETE") {
		logError("Invalid method: " + method);
		errorCode = 501;
		return false;
	}
	else if (path.size() > MAX_URI_LENGTH) {
		logError("URI too long");
		errorCode = 414;
		return false;
	}
	else if (httpVersion != "HTTP/1.1") {
		logError("Unsupported HTTP version: " + httpVersion);
		errorCode = 505;
		return false;
	}

	std::string decodedPath;
	if (!stringPercentDecoding(path,decodedPath))
	{
		logError("Error in path decoding syntax");
		errorCode = 400;
		return false;
	}
	path = decodedPath;
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
	unsigned int	totalHeaderSize = 0;

	while (std::getline(requestStream, headerLine) && !headerLine.empty())
	{
		if (headerLine == "\r") {
			break;
		}
		totalHeaderSize += headerLine.size();

		std::smatch headerMatches;
		if (std::regex_match(headerLine, headerMatches, headerRegex)) {
			std::string headerName = headerMatches[1];
			if (headerName[0] == '-' || headerName[headerName.size() - 1] == '-') {
				logError("Invalid header: " + headerName);
				errorCode = 400;
				return false;
			}
			std::string headerValue = headerMatches[2];
			headers[headerName] = headerValue;
			if (headerName == "Host") {
				findConfig();
			}
			if (conf && totalHeaderSize > conf->getMaxClientHeaderSize()) {
				logError("Total Header size limit reached");
				errorCode = 431;
				return false;
			}	
		}
		else {
			logError("Invalid header format: " + headerLine);
			errorCode = 400;
			return false;
		}
	}
	if (headers.find("Host") == headers.end()) {
		logError("Missing Host header");
		errorCode = 400;
		return false;
	}
	return true;
}

/* @brief Converts a hexadecimal string to a size_t
 *
 * is used to parse chunk sizes from HTTP/1.1 requests that use chunked encoding
 *
 *  - trims leading and trailing whitespace
 *  - validates that all remaining characters are hexadecimal
 *  - Rejects negative values (e.g., strings starting with '-').
 *  - Uses std::stringstream in hexadecimal mode to parse the value into a size_t safely
 *
 * @param hexStr The input string containing the hexadecimal number
 * @param out The resulting parsed size_t value if the conversion succeeds
 * @return true if the input was valid hex and parsed successfully, false otherwise
 */
bool	HttpConnectionHandler::hexStringToSizeT(const std::string &hexStr, size_t &out)
{
    std::string trimmed;

    size_t start = hexStr.find_first_not_of(" \t\r\n");
    size_t end = hexStr.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos)
        return false;
    trimmed = hexStr.substr(start, end - start + 1);

    if (!trimmed.empty() && trimmed[0] == '-')
        return false;
    for (char c : trimmed) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    std::stringstream ss(trimmed);
    size_t result;
    ss >> std::hex >> result;

    if (ss.fail() || !ss.eof()) {
        return false;
    }

    out = result;
    return true;
}

/* @brief Parses the chunks of an HTTP request body using chunked transfer encoding
 *
 * This function is called after headers have been parsed and begins processing the body
 * it attempts to parse one or more complete chunks, extracting their sizes and
 * appending the corresponding data to the body buffer
 * handles partial data  by returning S_ReadBody and saving
 * the current position in `chunkRemainder`. It also handles malformed input
 * by logging an error and returning S_Error
 *
 * The HTTP chunked transfer encoding sends the body as a series of:
 * 		<chunk-size-in-hex>\r\n
 * 		<chunk-data>\r\n
 * until a final chunk of size 0 is received.
 * 		0\r\n
 *		\r\n
 *
 * @return HandlerStatus:
 *   - S_Done: Final 0 sized chunk received, body complete
 *   - S_ReadBody: More data is needed to finish parsing chunks
 *   - S_Error: Malformed request or internal error encountered
 */
HandlerStatus	HttpConnectionHandler::handleFirstChunks(std::string &chunkData)
{
	size_t chunkDataLen = 0;

	while (true)
	{
		//finding the chunk hex size string
		std::string::size_type chunkSizeEnd = chunkData.find("\r\n", chunkDataLen);
		if (chunkSizeEnd == std::string::npos) {
			logInfo("Chunk size line incomplete");
			chunkRemainder = chunkData.substr(chunkDataLen);
			return S_Again;
		}

		//turn hexa strig into num
		std::string hexaStr = chunkData.substr(chunkDataLen, chunkSizeEnd - chunkDataLen);
		size_t hexaSize;
		if (!hexStringToSizeT(hexaStr, hexaSize))
		{
			logError("Invalid character or negative value in hexadecimal: " + hexaStr);
			errorCode = 400;
			return S_Error;
		}
		
		size_t chunkDataStart = chunkSizeEnd + 2;
		size_t totalChunkLen = chunkDataStart + hexaSize + 2;
		if (hexaSize == 0)
		{
			if (chunkData.substr(chunkDataStart, 2) != "\r\n") {
				logError("Missing final CRLF after last chunk");
				errorCode = 400;
				return S_Error;
			}
			return S_Done;
		}
		if (totalChunkLen > chunkData.size()) {
			logInfo("Partial chunk in buffer, saving and reading more");
			chunkRemainder = chunkData.substr(chunkDataLen);
			return S_Again;
		}
		body += chunkData.substr(chunkDataStart, hexaSize);
		//move to start of next chunk
		chunkDataLen = totalChunkLen;
		logInfo("Handled 1 chunk");
	}
}

/*
 */
HandlerStatus	HttpConnectionHandler::readBody()
{
	char		buffer[8192];
	int		bRead;


	logInfo("Handle body called");
	bRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
	if (bRead == 0)
		return S_ClosedConnection;
	if (bRead < 0) {
		logError("Reading from the socket");
		errorCode = 400;
		return S_Error;
	}
	buffer[bRead] = '\0';

	if (conf && bRead + body.size() > conf->getMaxClientBodySize())
	{
		logError("Request Body size bigger than max client body size");
		errorCode = 415;
		return S_Error;
	}

	if (headers.count("Transfer-Encoding") && headers["Transfer-Encoding"] == "chunked")
	{
		chunkRemainder.append(buffer, bRead);
		return handleFirstChunks(chunkRemainder);
	}

	body.append( buffer, bRead);

	auto it = headers.find("Content-Length");
	if (it == headers.end()) {
		logError("Cant find Content-Length header in getBody()");
		errorCode = 400;
		return S_Error;
	}
	std::string contentLengthStr = it->second;
	int contentLengthInt;
	try
	{
		contentLengthInt = std::stoi(contentLengthStr);
		if (contentLengthInt < 0) {
			logError("Negative Content-Length");
			errorCode = 400;
			return S_Error;
		}
	}
	catch (const std::exception &e)
	{
		logError("Stoi Error");
		errorCode = 400;
		return S_Error;
	}
	size_t contentLength = static_cast<size_t>(contentLengthInt);
	if (body.size() < contentLength) {
		return S_Again;
	}
	else if (body.size() > contentLength) {
		logError("Body size bigger than Content-Length");
		errorCode = 400;
		return S_Error;
	}
	else {
		return S_Done;
	}
}
