#include "HttpConnectionHandler.hpp"
#include "Configuration.hpp"
#include "CgiHandler.hpp"
#include "Logger.hpp"

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
std::string HttpConnectionHandler::getContentType(const std::string &filepath)
{
	size_t dotPos = filepath.find_last_of(".");
	if (dotPos == std::string::npos) {
		return "application/octet-stream";
	}

	std::string fileExtension = filepath.substr(dotPos);

	if (fileExtension == ".html") return "text/html";
	if (fileExtension == ".css") return "text/css";
	if (fileExtension == ".js") return "application/javascript";
	if (fileExtension == ".png") return "image/png";
	if (fileExtension == ".jpg" || fileExtension == ".jpeg") return "image/jpeg";
	if (fileExtension == ".gif") return "image/gif";
	if (fileExtension == ".svg") return "image/svg+xml";
	if (fileExtension == ".ico") return "image/x-icon";
	if (fileExtension == ".json") return "application/json";
	if (fileExtension == ".xml") return "application/xml";
	if (fileExtension == ".pdf") return "application/pdf";
	if (fileExtension == ".zip") return "application/zip";
	if (fileExtension == ".txt") return "text/plain";
	if (fileExtension == ".mp3") return "audio/mpeg";
	if (fileExtension == ".mp4") return "video/mp4";
	if (fileExtension == ".webp") return "image/webp";

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
bool	HttpConnectionHandler::isMethodAllowed(LocationBlock *block, std::string &str)
{
	if (std::find(block->methods.begin(), block->methods.end(), str) != block->methods.end()) {
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
	originalPath = path;
	if (!block) {
		logError("No locaton block matched (should't happen?)");
		errorCode = 400; //?
		return false;
	}
	locBlock = block;
	//check redirections
	if (block->returnCode == 307) {
		response = createHttpRedirectResponse(307, block->returnURL);
		return false;
	}

	if (!isMethodAllowed(block, method)) {
		logError("Method " + method + " not allowed in location " + block->path);
		errorCode = 405;
		return false;
	}
	if (method == "POST" && !block->uploadDir.empty())
	{
		path = "./" + block->uploadDir;
		return true;
	}

	std::string relativePath = path.substr(block->path.length());
	path =  "./" + block->root + "/" + relativePath;

	if (path.find("/..") != std::string::npos) {
		logError("Path: " + path + "contains escape sequence /..");
		errorCode = 403;
		return false;
	}
	return true;
}

HandlerStatus	HttpConnectionHandler::serveFile()
{
	logInfo("serving file");
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		//what to do if file serving fails middle of sending body
		return S_Error;
	} 

	char buffer[8192];
	file.seekg(bSent); //error handling for reading out of scope
	file.read(buffer, sizeof(buffer)); //how to error check?
	/*if (file.bad() || file.fail())
	{
		logError("Reading error?");
		return S_Error;
	}*/
	int sent = send(clientSocket, buffer, file.gcount(), 0);
	if (sent < 0)
		return S_Error;
	bSent += sent;
	//file.close();
	if (file.eof())
		return S_Done;
	return S_Again;
}

/* function to serve file in GET requested
 *
 * if file is ok to be served, this fucntion is called.
 * checking if it exists and we have permissions if done before this.
 * opens file, checks size for content len, and sends appropriate http response with
 * the file content. sens headers first and then file content in chunks of 8kb
 */
void	HttpConnectionHandler::checkFileToServe(std::string &str)
{
	std::ifstream file(str, std::ios::binary);
	if (!file.is_open()) {
		errorCode = 404;
		return;
	}

	// Determine file size
	file.seekg(0, std::ios::end);
	std::streampos fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	// Get content type
	std::string contentType = getContentType(str);

	// Send HTTP headers
	std::ostringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\r\n";
	headerStream << "Content-Length: " << fileSize << "\r\n";
	headerStream << "Content-Type: " << contentType << "\r\n";
	headerStream << "Connection: Keep-Alive\r\n";
	headerStream << "\r\n";

	response = headerStream.str();
	fileServ = true;
	path = str;
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
			checkFileToServe(current);
			return ;
		}
	}

	// check block.dirListing -> off -> 403
	if (!locBlock->dirListing) {
		logError("Get request for directory " + path + ", but file serving not allowed");
		errorCode = 403;
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
		errorCode = 403;
		return;
	}

	std::string htmlListing = "<html>\r\n<head><title>Index of " + path +
		"</title><head>\r\n<body>\r\n<h1>Index of " + path + "</h1><hr>\r\n<ul>\r\n";
	if (locBlock->path != "/") {
		htmlListing += "<li><a href=\"../\">../</a></li>\n";
	}
	for (const auto &file : dirListing) {
		std::string currentFile = path + file;
		std::string fileLink = (std::filesystem::is_directory(currentFile)) ? file + "/" : file;
		htmlListing += "<li><a href=\"" + fileLink + "\">" + file + "</a></li>\n";
	}
	htmlListing += "</ul>\r\n<hr>\r\n</body>\r\n</html>\r\n";

	response = createHttpResponse(200, htmlListing, "text/html");
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
	std::string fileToServe = path;

	if (access(fileToServe.c_str(), F_OK) != 0) {
		errorCode = 404;
		return;
	}
	else if (access(fileToServe.c_str(), R_OK) != 0) {
		errorCode = 403;
		return;
	}
	
	if (path.back() == '/') {
		handleGetDirectory();
		return ;
	}
	if (std::filesystem::is_directory(fileToServe)) {
		logInfo("Redirecting to " + path + "/");
		response = createHttpRedirectResponse(301, originalPath);
		return;
	}
	checkFileToServe(fileToServe);
}

/* checks the path to directory and validates its able to handle POST request
 *
 * checks the directory exists, its actual directory, and creates
 * tmp file there to make sure its writable
 *
 * 
 */
bool	HttpConnectionHandler::validateUploadRights()
{
	logInfo("checking POST directory rights:  Path is " + path);
	std::filesystem::path fsPath(path);

	if (!std::filesystem::exists(fsPath)) {
		logError("POST request path: " + path + " doesnt't exist");
		return false;
	}
	else if (!std::filesystem::is_directory(path)) {
		logError("POST request path: " + path + " is not directory");
		return false;
	}
	std::filesystem::path tmp = fsPath / "tmp_permission_check";
	std::ofstream fd(tmp);
	if (!fd) {
		logError("POST request path: " + path + " not writable");
		return false;
	}
	fd.close();
	std::filesystem::remove(tmp);
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
		logError("Empty body in POST request");
		errorCode = 400;
		return;
	}
	if (!validateUploadRights()) {
		logError("No rights for upload");
		errorCode = 500;
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
			errorCode = 400;
			return;
		}
		return ;
	}
    	// Unknown content type / for now atleast
	else {
		responseBody = "<h1>415 Unsupported Media Type</h1>";
		errorCode = 415;
		return;
	}
	response = createHttpResponse(200, responseBody, "text/html");
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
		response = createHttpRedirectResponse(301, path);
		return;
	}
	
	if (access(path.c_str(), W_OK) != 0) {
		logError("Permission denied to delete directory: " + path);
		errorCode = 403;
		return;
	}

	if (!std::filesystem::is_empty(path, ec)) {
		logError("Conflict: Delete attempt on non-empty directory: " + path);
		errorCode = 409;
		return;
	}

	if (!std::filesystem::remove(path, ec)) {
		logError("Failed to delete directory: " + (ec ? ec.message() : "Unknown error"));
		errorCode = 500;
		return;
	}
	response = createHttpResponse(204, "", "text/html");
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
	std::string fileToDelete = path;
	std::error_code ec;

	// file exists
    	if (!std::filesystem::exists(fileToDelete, ec)) {
        	logError("DELETE requested but file not found: " + fileToDelete);
		errorCode = 404;
        	return;
	}
	//its directory or something unsupported
	if (std::filesystem::is_directory(fileToDelete, ec)) {
		deleteDirectory();
		return;
	}
	else if (!std::filesystem::is_regular_file(fileToDelete, ec)) {
        	logError("DELETE request but file is not directory or regular file: " + fileToDelete);
		errorCode = 409;
        	return;
	}

	//handle regular file deletion
	if (path.empty() || path.back() == '/') {
		logError("DELETE file requested but path ends with '/': " + path);
		errorCode = 404;
		return;
	}
	if (!std::filesystem::remove(fileToDelete, ec)) {
		logError("Failed to delete file: " + (ec ? ec.message() : "Unknown error"));
		if (ec == std::errc::permission_denied) {
			errorCode = 403;
		}
		else {
			errorCode = 500;
		}
		return;
	}
	response = createHttpResponse(200, "<h1>File Deleted Successfully</h1>", "text/html");
}

/* checks for name matches in the following priority:
 * 1. Exact match
 * 2. Wildcard at the beginning of the pattern ("*.example.com")
 * 3. Wildcard at the end of the pattern ("www.example.*")
 *
 * only a single wildcard at the very beginning or very end is supported.
 * a pattern consisting of only "*" is ambiguous and returns 0
 *
 * @param pattern: the server name fomr configs
 * @param host:    the name form host header
 * @return int match priority value
 * 1: exact match (highest prio)
 * 2: Wildcard match at the beginning (*suffix)
 * 3: Wildcard match at the end (prefix*)
 * 0: No match found
 */
int	HttpConnectionHandler::matchServerName(const std::string& pattern, const std::string& host)
{
    if (host.empty()) {
	    return 0;
    }
    // pattern of just "*" is ambigious at best, returning no match
    if (pattern == "*") {
        return 0;
    }
    // priority 1: exact match
    if (pattern == host) {
        return 1;
    }
    // priority 2: wildcard at the beginning (*suffix)
    if (pattern.length() > 1 && pattern.front() == '*' && pattern.back() != '*') {
        std::string suffix = pattern.substr(1);
        if (host.length() >= suffix.length() &&
            host.compare(host.length() - suffix.length(), suffix.length(), suffix) == 0){
            return 2;
        }
    }
    // priority 3: wildcard at the end (prefix*)
    if (pattern.length() > 1 && pattern.back() == '*' && pattern.front() != '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        if (host.length() >= prefix.length() && host.compare(0, prefix.length(), prefix) == 0){
            return 3;
        }
    }
    return 0;
}

/* responsible for findings the configuration file
 * current implementation is  called when host header file is found during parsing
 * priority for match from low to high is:
 * 1. IP + PORT match (first found is default until better match is found)
 * 2. full or partial servername match, prio explained in matchServerName() function.
 * 3. fallback if nothing matches is serverMap[0]
 *
 * future improvements: sometimes Host can come in with
 *	trailing dot: example.com.
 *	port: example.com:8080
 */
void	HttpConnectionHandler::findConfig()
{
	Configuration	*result = nullptr;
	std::string	header;
	int		bestMatch = 4;
	int		current = 4;
	try
	{
		header = headers.at("Host");
	}
	catch(const std::out_of_range &e)
	{
		logError("Can't find host header, bug in code with current logic");
	}
	for (auto &server : serverMap)
	{
		if (IP == server.getHost() && PORT == server.getPort())
		{
			//default first match
			if (!result)
				result = &server;
			std::stringstream	ss(server.getServerNames());
			std::string		token;
			while (ss >> token)
			{
				if ((current = matchServerName(token, header))) {
					if (current < bestMatch) {
						bestMatch = current;
						result = &server;
					}
				}
			}
		}
	}
	if (!result) {
		logError("No match for request IP:PORT, defaulting to servermap[0]");
		conf = &serverMap[0];
	}
	else {
		conf = result;
	}
	logInfo("Final config using server " + conf->getServerNames());
}

void	HttpConnectionHandler::findInitialConfig()
{
	Configuration	*result = nullptr;

	logInfo("Trying to find config before header " + IP + ":" + PORT);
	for (auto &server : serverMap)
	{
		if (IP == server.getHost() && PORT == server.getPort())
			if (!result)
				result = &server;
	}
	if (!result) {
		logError("No match for request IP:PORT, defaulting to servermap[0]");
		conf = &serverMap[0];
	}
	else {
		conf = result;
	}
	logInfo("Final config using server " + conf->getServerNames());
}

/* handles the incoming HTTP request based on its method (GET, POST atm).
 *
 * depending on the HTTP method ("GET" or "POST"),  delegates the handling
 * to the appropriate method handleGetRequest or handlePostRequest
 * If the method is not supported, it responds with a 405 Method Not Allowed error.
 */
void	HttpConnectionHandler::handleRequest() 
{
  if (!response.empty())
    return ;
	/* originalPath = path; */
	// if (!checkLocation()) {
	// 	return;
	// }
	if (checkCgi() != NONE) {
		// CgiHandler cgiHandler(*this);
		// //cgiHandler.printCgiInfo(); // Comment out when not needed
		// cgiHandler.executeCgi();
		// char buffer[1024];
		// memset(buffer, 0, 1024);
		// int* pipeFromCgi = cgiHandler.getPipeFromCgi();
		// int size = read(pipeFromCgi[0], buffer, 1024);
		// if (size < 0)
		// 	perror("read from cgi:");
		// assert(size >= 0);
		// write(1, buffer, size);
		// std::string response = createHttpResponse(200, buffer, "text/html");
		// logDebug("%s", response.c_str());
		// send(clientSocket, response.c_str(), response.size(), 0);
		// return;
		//serveCgi();

	}
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
		logError("Method " + method + " not implemented");
		errorCode = 501;
	}
	/*check here whether error happened, post or delete was executed and we can send, or we need to file serv
	  basic idea is either to send everything at once is possible, like redirections and short post/delete responses
	  if error or serving get request then send headers and go to send body in chunks
	 */
	if (errorCode) {
		response = createErrorResponse(errorCode);
	}
}
