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

/* handles creating HTTP response when DELETE or GET request gets 301 for example
 * when trying to access directory with no trailing /, example DELETE /directory HTTP/1.1
 */
std::string	HttpConnectionHandler::createHttpRedirectResponse(int statusCode, const std::string &location)
{
    std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " Moved Permanently\r\n";
    response += "Location: " + location + "/\r\n";
    response += "Content-Type: text/html\r\n";
    std::string body = "<h1>301 Moved Permanently</h1><p>Try " + path + "/</p>";
    response += "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n";
    response += body;

    return response;
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

/* recursively finds the longest matching location block for path in object
 *
 * starts by checking the top level location blocks and, if it finds a match, recurses into
 * the nested locations and  tries to find a longer
 *
 * @param blocks  The vector of LocationBlock objects to search in.
 * @param current Pointer to the currently longest matched LocationBlock (initialized as nullptr).
 *
 * @return pointer to the longest matching location block struct, null if nothing found(shouldnt happen)
 *
 * Example usage:
 *   LocationBlock *longestBlock = findLocationBlock(conf.getLocationBlocks(), nullptr);
 *   if (!matchedBlock) {
 *       // some error if  needed
 *   }
 */
LocationBlock *HttpConnectionHandler::findLocationBlock(std::vector<LocationBlock> &blocks, LocationBlock *current)
{
	for (LocationBlock &block : blocks) {
		if (path.compare(0, block.path.length(), block.path) == 0)
		{
			current = &block;
			return (findLocationBlock(block.nestedLocations, current));
		}
	}
	return current;
}

/* Takes location blocks allowed methods vector and cross references the
 *method currently being executed
 *
 * @param block	 LocationBlock associated to the current path
 * @param method Current method beng executed
 *
 * return true if method allowed,  flase otherwise
 */
bool	HttpConnectionHandler::isMethodAllowed(LocationBlock *block, std::string &method)
{
	if (std::find(block->methods.begin(), block->methods.end(), method) != block->methods.end()) {
		return true;
	}
	else {
		return false;
	}
}

/* checks that the location block allows the current method to be executed
 * 
 * first finds longest matching location block form conf file with findLocationBlock()
 * checks that the current method is allowed  withing that blocks allowed methods
 * remaps the path according to the root path found  in the block
 * makes sure the true path doesnt try to traverse down with /../ for example
 * currently only checking /.. might need better protection
 *
 * example of true path
 * request GET /kapouet/pouic/toto/pouet HTTP/1.1 --> Path = /kapouet/pouic/toto/pouet
 * longest matching location block /kapouet that is rooted to /tmp/www
 * True path will become /tmp/www + /pouic/toto/pouet
 */
bool	HttpConnectionHandler::checkLocation()
{
	LocationBlock *block = findLocationBlock(conf->getLocationBlocks(), nullptr);
	if (!block) {
		logError("No locaton block matched (should't happen?)");
		std::string response = createHttpResponse(500, "<h1>500 Internal Server Error</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return false;
	}
	locBlock = block;

	if (!isMethodAllowed(block, method)) {
		logError("Method " + method + " not allowed in location " + block->path);
		std::string response = createHttpResponse(405, "<h1>405 Method not Allowed</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return false;
	}

	std::string relativePath = path.substr(block->path.length());
	path =  "./" + block->root + "/" + relativePath;

	if (path.find("/..") != std::string::npos) {
		logError("Path: " + path + "contains escape sequence /..");
		std::string response = createHttpResponse(403, "<h1>403 Forbidden</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return false;
	}
	return true;
}

/* function to serve file in GET requested
 *
 * if file is ok to be served, this fucntion is called.
 * checking if it exists and we have permissions if done before this.
 * opens file, checks size for content len, and sends appropriate http response with
 * the file content. sens headers first and then file content in chunks of 8kb
 */
void	HttpConnectionHandler::serveFile(std::string &filePath)
{
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		std::string errorResponse = createHttpResponse(404, "<h1>404 Not Found</h1>", "text/html");
		send(clientSocket, errorResponse.c_str(), errorResponse.size(), 0);
		return;
	}

	// Determine file size
	file.seekg(0, std::ios::end);
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// Get content type
	std::string contentType = getContentType(filePath);

	// Send HTTP headers
	std::ostringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\r\n";
	headerStream << "Content-Length: " << fileSize << "\r\n";
	headerStream << "Content-Type: " << contentType << "\r\n";
	headerStream << "Connection: close\r\n";
	headerStream << "\r\n";

	std::string headerStr = headerStream.str();
	send(clientSocket, headerStr.c_str(), headerStr.size(), 0); // Send headers

	// Send file content in chunks
	char buffer[8192];
	while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
		send(clientSocket, buffer, file.gcount(), 0);
	}
	file.close();
}

/* function to handle GET method on directory. two options:
 * 1. if directory contains one of index files, serve that
 * 2. check if auto index is on on locaton block, return auto-index of directory
 * 	else 403

 * need to catch filesystem errors?
 */
void	HttpConnectionHandler::handleGetDirectory()
{
	std::stringstream indexStream(conf->getIndex());

	// if conf.index contains file in this directory, serve it (first match order)
	std::string token;
	while (indexStream >> token) {
		std::string current = path + token;
		if (std::filesystem::exists(current) && std::filesystem::is_regular_file(current)) {
			serveFile(current);
			return ;
		}
	}

	// check block.dirListing -> off -> 403
	if (!locBlock->dirListing) {
		logError("Get request for directory " + path + ", but file serving not allowed");
		std::string response = createHttpResponse(403, "<h1>Permission denied.</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	// serve directory listing
	std::vector<std::string> dirListing;
	try {
		for (const auto &entry : std::filesystem::directory_iterator(path)) {
			// entry.path() gives the full path
			// entry.path().filename() gives just the name part
			dirListing.push_back(entry.path().filename().string());
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		logError("Get request for directory " + path + ", filesystem error");
		std::string response = createHttpResponse(403, "<h1>Permission denied.</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	std::string htmlListing = "<html>\r\n<head><title>Index of " + path +
		"</title><head>\r\n<body>\r\n<h1>Index of " + path + "</h1><hr>\r\n<ul>\r\n";
	if (locBlock->path != "/") {
		htmlListing += "<li><a href=\"../\">../</a></li>\n";
	}
	for (const auto &file : dirListing) {
		std::string filePath = path + file;
		std::string fileLink = (std::filesystem::is_directory(filePath)) ? file + "/" : file;
		htmlListing += "<li><a href=\"" + fileLink + "\">" + file + "</a></li>\n";
	}
	htmlListing += "</ul>\r\n<hr>\r\n</body>\r\n</html>\r\n";

	std::string response = createHttpResponse(200, htmlListing, "text/html");
	send(clientSocket, response.c_str(), response.size(), 0);
}

/* handles an HTTP GET request by serving the requested file
 *
 * attempts to open the requested file and send it back to the client
 * with appropriate HTTP headers (Content-Length, Content-Type, etc)
 * if the file does not exist, a 404 Not Found response is sent.
 *
 * also supports index serving and directory auto indexing if  allowed
 *
 */
void	HttpConnectionHandler::handleGetRequest()
{
	std::string filePath = path;

	if (access(filePath.c_str(), F_OK) != 0) {
		std::string response = createHttpResponse(404, "<h1>Not found</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}
	else if (access(filePath.c_str(), R_OK) != 0) {
		std::string response = createHttpResponse(403, "<h1>Permission denied.</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}
	
	if (path.back() == '/') {
		handleGetDirectory();
		return ;
	}


	if (std::filesystem::is_directory(filePath)) { //if request /directory was actually directory without trailing / WIP
		logInfo("301 moved permanently, not handled yet. sending generic response");
		std::string response = createHttpResponse(301, "<h1>301 Moved permanently.</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	serveFile(filePath);
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

		processMultipartPart(part, fileResponse);

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

/* handles DELETE request on /diretory or /directory/

 * summary on diff cases: server receives DELETE /uploads/images/
 * If path does NOT exist                     -> 404 Not Found
 * If path exists but is NOT a directory      -> 409 Conflict or 400 Bad Request
 * If URL doesn’t end in `/`                  -> 409 Conflict
 * If directory is NOT empty                  -> 409 Conflict
 * If no write/delete permission              -> 403 Forbidden
 * If all checks pass (and dir is empty)      -> 204 No Content
 */
void	HttpConnectionHandler::deleteDirectory()
{
	std::error_code ec;

	if (path.empty() || path.back() != '/') {
		logError("DELETE directory requested but path doesn't end with '/': " + path);
		std::string response = createHttpRedirectResponse(301, path);
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}
	
	if (access(path.c_str(), W_OK) != 0) {
		logError("Permission denied to delete directory: " + path);
		std::string response = createHttpResponse(403, "<h1>403 Forbidden: No write access</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	if (!std::filesystem::is_empty(path, ec)) {
		logError("Cannot delete non-empty directory: " + path);
		std::string response = createHttpResponse(409, "<h1>409 Conflict: Directory not empty</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	if (!std::filesystem::remove(path, ec)) {
		logError("Failed to delete directory: " + (ec ? ec.message() : "Unknown error"));
		std::string response = createHttpResponse(500, "<h1>500 Internal Server Error: Cannot delete directory</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}

	std::string response = createHttpResponse(204, "", "text/html");
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
 *
 * is handling auto redirection expected? /directory -> /directory/ 301
 */
void	HttpConnectionHandler::handleDeleteRequest()
{
	std::string filePath = path;
	std::error_code ec;

	// file exists
    	if (!std::filesystem::exists(filePath, ec)) {
        	logError("DELETE requested but file not found: " + filePath);
        	std::string response = createHttpResponse(404, "<h1>404 Not Found</h1>", "text/html");
        	send(clientSocket, response.c_str(), response.size(), 0);
        	return;
	}
	//its directory or somethung unsupported
	if (std::filesystem::is_directory(filePath, ec)) {
		deleteDirectory();
		return;
	}
	else if (!std::filesystem::is_regular_file(filePath, ec)) {
        	logError("DELETE request but file is not directory or regular file: " + filePath);
        	std::string response = createHttpResponse(409, "<h1>409 Delete request on unknown file type</h1>", "text/html");
        	send(clientSocket, response.c_str(), response.size(), 0);
        	return;
	}

	//handle regular file deletion
	if (path.empty() || path.back() != '/') {
		logError("DELETE file requested but path ends with '/': " + path);
		std::string response = createHttpResponse(404, "<h1>404 File /filename/ not possbile</h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
		return;
	}
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
	originalPath = path;
	if (!checkLocation()) {
		return;
	}
	//cgi here probably
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
		std::string response = createHttpResponse(501, "<h1>501 Method: " + method + "Not Implemented </h1>", "text/html");
		send(clientSocket, response.c_str(), response.size(), 0);
	}
}
