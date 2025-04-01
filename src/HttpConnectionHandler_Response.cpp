#include "HttpConnectionHandler.hpp"

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
	response << "Content-Length: " << body.size() << "\r\n";
	response << "Content-Type: " << contentType << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

/* determines the content type based on the file extension
 *
 * checks the file extension (from the given file path) to the corresponding MIME content type
 * if the file extension is not recognized, defaults to "application/octet-stream"
 *
 * @param path The file path (e.g., "/images/photo.jpg").
 *
 * @return The MIME type associated with the file extension (e.g., "text/html", "image/jpeg")
 *
 * Example usage:
 *   std::string contentType = getContentType("/path/to/file.jpg");
 *   // contentType will be "image/jpeg".
 */
std::string HttpConnectionHandler::getContentType(const std::string &path)
{
    size_t dotPos = path.find_last_of(".");
    if (dotPos == std::string::npos) {
		return "application/octet-stream";
    }

    std::string extension = path.substr(dotPos);

    if (extension == ".html") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".ico") return "image/x-icon";
    if (extension == ".json") return "application/json";
    if (extension == ".xml") return "application/xml";
    if (extension == ".pdf") return "application/pdf";
    if (extension == ".zip") return "application/zip";
    if (extension == ".txt") return "text/plain";
    if (extension == ".mp3") return "audio/mpeg";
    if (extension == ".mp4") return "video/mp4";
    if (extension == ".webp") return "image/webp";

    return "application/octet-stream";
}

/* handles an HTTP GET request by serving the requested file
 *
 * attempts to open the requested file and send it back to the client
 * with appropriate HTTP headers (Content-Length, Content-Type, etc)
 * if the file does not exist, a 404 Not Found response is sent.
 *
 * 1. tries to find the requested file based on the path
 * 2. if the file is found, it reads the file's size, sends the headers, and sends
 *    the file's content in chunks to the client
 * 3. If the file is not found, a 404 Not Found error page is sent
 */
void	HttpConnectionHandler::handleGetRequest()
{
	std::string filePath = "." + path;
	if (filePath == "./") {
		filePath = "./index.html";
	}

	if (access(filePath.c_str(), F_OK) != 0) {
		std::string errorResponse = createHttpResponse(404, "<h1>Not found</h1>", "text/html");
		send(clientSocket, errorResponse.c_str(), errorResponse.size(), 0);
		return;
	}
	else if (access(filePath.c_str(), R_OK) != 0) {
		std::string errorResponse = createHttpResponse(403, "<h1>Permission denied.</h1>", "text/html");
		send(clientSocket, errorResponse.c_str(), errorResponse.size(), 0);
		return;
	}


	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		std::string errorResponse = createHttpResponse(404, "<h1>404 Not Found</h1>", "text/html");
		send(clientSocket, errorResponse.c_str(), errorResponse.size(), 0);
		return;
	}

	// need to change from seek, doesnt work for compelx files?
	file.seekg(0, std::ios::end);
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string contentType = getContentType(filePath);

	// send HTTP headers
	std::ostringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\r\n";
	headerStream << "Content-Length: " << fileSize << "\r\n";
    	headerStream << "Content-Type: " << contentType << "\r\n";
    	headerStream << "Connection: close\r\n";
    	headerStream << "\r\n";

    	std::string headerStr = headerStream.str();
    	send(clientSocket, headerStr.c_str(), headerStr.size(), 0); // send headers

    	// send file content in chunks?
    	char buffer[8192];
    	while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        	send(clientSocket, buffer, file.gcount(), 0);
    	}
	file.close();
}

/* processes a single part of a multipart/form-data body, typically used for file uploads
 *
 * extracts the filename and file data from a given part of the multipart body,
 * saves the file to the disk, and returns whether the processing was successful
 *
 * The part is expected to follow the format of a multipart file upload, which includes a filename
 * and the file content. The function extracts the file data and saves it to the uploads/ directory
 * atm theres no check for uploads directory existing and fails if it doesnt
 *
 * @param part The part of the multipart body, typically containing a file and associated metadata
 *
 * @return true if part was processed successfully, false otherwise
 */
bool	HttpConnectionHandler::processMultipartPart(const std::string& part,
		std::string &responseBody)
{
	// filename
    	size_t filenamePos = part.find("filename=\"");
    	if (filenamePos == std::string::npos) {
		return true; // Ignore non file parts for now?
	}
	size_t filenameEnd = part.find("\"", filenamePos + 10);
    	if (filenameEnd == std::string::npos) {
		return false;
	}
	std::string filename = part.substr(filenamePos + 10, filenameEnd - (filenamePos + 10));

    	// start of file data
    	size_t dataStart = part.find("\r\n\r\n", filenameEnd);
    	if (dataStart == std::string::npos) {
		return false;
	}
    	dataStart += 4;

    	// end of file data
    	size_t dataEnd = part.find("\r\n", dataStart);
    	if (dataEnd == std::string::npos) {
		return false;
	}

    	// extract file data
    	std::string fileData = part.substr(dataStart, dataEnd - dataStart);

	// check if file already exists, corret behaviour overwrite or not?
	if (access(("uploads/" + filename).c_str(), F_OK) == 0) {
		responseBody = "{ \"status\": \"error\", \"message\": \"File already exists: " + filename + "\" }";
		return false;
	}

    	// save the file
    	std::ofstream outFile("uploads/" + filename, std::ios::binary);
    	if (!outFile) {
		responseBody = "{ \"status\": \"error\", \"message\": \"No permission to save file: " + filename + "\" }";
        	std::cerr << "Error: Could not create file: " << filename << std::endl;
        	return false;
    	}
    	outFile.write(fileData.c_str(), fileData.size());
    	outFile.close();

    	std::cout << "File uploaded: " << filename << std::endl;

    	std::ostringstream jsonResponse;
    	jsonResponse << "{ \"status\": \"success\", \"filename\": \""
	    << filename << "\", \"size\": " << fileData.size() << " }";
	responseBody = jsonResponse.str();

	return true;
}

/* handles the file upload process in a multipart/form-data request
 *
 * extracts the multipart data from the request body, processes each part using
 * processMultipartPart(), and saves uploaded files to the server
 *
 * @return true if all parts are successfully processed, false otherwise
 *
 * example of post handled by this:
 * 
 * POST /upload HTTP/1.1
 * Host: example.com
 * Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW
 * Content-Length: 112
 * 
 * ------WebKitFormBoundary7MA4YWxkTrZu0gW
 * Content-Disposition: form-data; name="file"; filename="example.txt"
 * Content-Type: text/plain
 *
 * Hello, this is the content of the example file.
 *
 * ------WebKitFormBoundary7MA4YWxkTrZu0gW--
 */
bool	HttpConnectionHandler::handleFileUpload()
{
	std::string contentType = headers["Content-Type"];
	size_t	boundaryPos = contentType.find("boundary=");
	if (boundaryPos == std::string::npos) {
		std::cout << "Error: No boundary found in multipart/form-data" << std::endl;
		return false;
	}

	std::string boundary = "--" + contentType.substr(boundaryPos + 9);
	size_t	startPos = body.find(boundary);
	if (startPos == std::string::npos) {
		std::cout << "Error: No boundary found in body" << std::endl;
		return false;
	}

	startPos += boundary.length();
	std::ostringstream jsonResponse;
	jsonResponse << "{ \"uploads\": [";

	bool	firstFile = true;
	while (true)
	{
		// locate next boundary
		size_t nextBoundary = body.find(boundary, startPos);
		if (nextBoundary == std::string::npos) {
			break;
		}
		// extract part data
		std::string part = body.substr(startPos, nextBoundary - startPos);
		startPos = nextBoundary + boundary.length();
		// process each part
		std::string fileResponse;

		bool success = processMultipartPart(part, fileResponse);
		(void)success;

		if (!firstFile) {
			jsonResponse << ",";
		}
        	jsonResponse << fileResponse;
        	firstFile = false;
	}

	jsonResponse << "] }";

	std::string response = createHttpResponse(200, jsonResponse.str(), "application/json");
	send(clientSocket, response.c_str(), response.size(), 0);
	return true;
}

/* handles an HTTP POST request by processing the body based on the content type
 *
 * This function differentiates between diff types of POST requests, including form data, JSON data,
 * and file uploads atm. It processes the body accordingly, responding with appropriate messages based on the content
 *
 * If the body is empty, a 400 Bad Request response is returned. If the content type is form data or JSON,
 * the body is echoed back to the client. If the content type is multipart/form-data (file uploads),
 * it processes the file upload and responds accordingly. For unsupported content types, a 415 Unsupported Media Type
 * response is returned
 */
void	HttpConnectionHandler::handlePostRequest()
{
	if (body.empty()) {
		std::string response = createHttpResponse(400, "<h1>400 Bad Request</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	std::string contentType = headers["Content-Type"];
	std::string responseBody;

	// if form data (application/x-www-form-urlencoded)
	if (contentType == "application/x-www-form-urlencoded") {
		responseBody = "<h1>Received Form Data:</h1><p>" + body + "</p>";
	}
	// if JSON data (application/json)
	else if (contentType == "application/json") {
		responseBody = "<h1>Received JSON:</h1><p>" + body + "</p>";
	}
	// handle file uploads
	else if (contentType.find("multipart/form-data") != std::string::npos) {
		if (!handleFileUpload()) {
			std::string response = createHttpResponse(400, "<h1>400 Bad Request (File Upload Failed)</h1>", "text/html");
			send(clientSocket, response.c_str(), response.size(), 0);
			return;
		}
		return ;
	}
    	// Unknown content type / for now atleast
	else {
		responseBody = "<h1>415 Unsupported Media Type</h1>";
		std::string response = createHttpResponse(415, responseBody, "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}
	std::string response = createHttpResponse(200, responseBody, "text/html");
	send(clientSocket, response.c_str(), response.size(), 0);
}

/* handles the HTTP DELETE
 *
 * checks if the file exists, verifies if it is a regular file (not a directory),
 * and attempts to delete the file. If an error occurs (such as the file not being found
 * or permission issues), an appropriate HTTP response is returned.
 *
 * Example of DELETE request handled by this:
 *
 * DELETE /data/example.txt HTTP/1.1
 * Host: example.com
 */
void	HttpConnectionHandler::handleDeleteRequest()
{
	std::string filePath = "." + path;

	//only allow testing in /uploads/ for now
	if (filePath.compare(0, 10, "./uploads/") != 0) {
		logError("DELETE only works for /uploads/ for now");
		std::string response = createHttpResponse(500, "<h1>500 Internal Server Error</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	std::error_code ec;

	// file exists
    	if (!std::filesystem::exists(filePath, ec)) {
        	logError("DELETE requested but file not found: " + filePath);
        	std::string response = createHttpResponse(404, "<h1>404 Not Found</h1>", "text/html");
        	send(clientSocket, response.c_str(), response.size(), 0);
        	return;
	}
	//its regular file
	if (!std::filesystem::is_regular_file(filePath, ec)) {
        	logError("DELETE requested on non-regular file: " + filePath);
        	std::string response = createHttpResponse(403, "<h1>403 Forbidden - Cannot delete directory</h1>", "text/html");
        	send(clientSocket, response.c_str(), response.size(), 0);
        	return;
	}

	// try to delete
	if (!std::filesystem::remove(filePath, ec)) {
		logError("Failed to delete file: " + (ec ? ec.message() : "Unknown error"));
		std::string response;
		if (ec == std::errc::permission_denied) {
			response = createHttpResponse(403, "<h1>403 Permission Denied</h1>", "text/html");
		}
		else {
			response = createHttpResponse(500, "<h1>500 Internal Server Error</h1>", "text/html");
		}
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

    std::string response = createHttpResponse(200, "<h1>File Deleted Successfully</h1>", "text/html");
    send(clientSocket, response.c_str(), response.size(), 0);
}


/* handles the incoming HTTP request based on its method (GET, POST atm).
 *
 * depending on the HTTP method ("GET" or "POST"),  delegates the handling
 * to the appropriate method handleGetRequest or handlePostRequest
 * If the method is not supported, it responds with a 405 Method Not Allowed error.
 */
void	HttpConnectionHandler::handleRequest() 
{
    if (method == "GET") {
	    handleGetRequest();
    }
    else if (method == "POST") {
	    handlePostRequest();
    }
    else if (method == "DELETE") {
		handleDeleteRequest();
    }
    else {
	    std::string response = createHttpResponse(405, "<h1>405 Method Not Allowed</h1>", "text/html");
	    send(clientSocket, response.c_str(), response.size(), 0);
    }
}
