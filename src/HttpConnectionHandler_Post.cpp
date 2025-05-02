#include "HttpConnectionHandler.hpp"

struct ParsedPartInfo
{
    bool		isFile = false;
    std::string		contentDispositionFilename; // filename from header, might be empty
    std::string_view	data;
};

// Holds the result of attempting to upload a file
struct FileUploadResult
{
    bool success = false;
    std::string originalFilename;
    std::string finalFilename;
    std::string savedPath;
    size_t fileSize = 0;
    std::string message;
};

/* @brief Sanitizes a filename to prevent directory traversal and invalid characters
 *
 * removes potentially harmful sequences like "../", "/", "\" and control characters.
 *
 * @param filename The raw filename to sanitize.
 * @return A sanitized version of the filename.
 */
std::string sanitizeFilename(const std::string& filename)
{
	if (filename.empty()) {
		return "";
	}

	std::string sanitized = filename;
	// 1. Remove ".." sequences to prevent directory traversal
	size_t pos;
	while ((pos = sanitized.find("..")) != std::string::npos) {
            sanitized.replace(pos, 2, ""); // Simple replacement; more robust parsing might be needed
        }

        // 2. Remove path separators
        sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '/'), sanitized.end());
        sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\\'), sanitized.end());

        // 4. Remove control characters and any other potentially problematic chars
        sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), [](unsigned char c) {
            return std::iscntrl(c); // Removes control characters
            // Add other characters to remove if needed: || c == ':' || c == '*' ...
        }), sanitized.end());


	// 5. Handle edge case where filename becomes empty after sanitization
        if (sanitized.empty() || sanitized == "." || sanitized == "..") {
             return "_sanitized_empty_"; // Return a placeholder
        }
	return sanitized;
}

/* @brief Generates a unique filename based on the current timestamp.
 *
 * Format: YYYYMMDD_HHMMSS_microseconds_upload
 *
 * @return A unique string suitable for use as a filename.
 */
std::string generateTimestampFilename()
{
	auto now = std::chrono::system_clock::now();
	auto now_c = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;

	std::stringstream ss;
	ss << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S");
	ss << "_" << std::setfill('0') << std::setw(6) << ms.count();
	ss << "_upload"; // Add a suffix
	return ss.str();
}

/* @brief Determines the filename to use based on priority.
 *
 * Priority:
 * 1. Filename extracted from the requestTargetUri (if present).
 * 2. Filename from the Content-Disposition header.
 * 3. Generated timestamp-based filename.
 *
 * Performs sanitization on the chosen filename.
 *
 * @param reqTargetUri The target URI of the HTTP request.
 * @param contentDispositionFilename Filename from the part's header.
 * @return The chosen and sanitized base filename (without directory path).
 */
std::string determineFilename(const std::string& reqTargetUri, const std::string& contentDispositionFilename)
{
	std::string baseFilename;

	// Priority 1: Filename from Request Target URI
        size_t lastSlash = reqTargetUri.find_last_of('/');
	if (lastSlash != std::string::npos && lastSlash + 1 < reqTargetUri.length()) {
		std::string nameFromUri = reqTargetUri.substr(lastSlash + 1);
		// Basic check if it looks like a filename (contains '.')
		// More robust checks might be needed depending on URI patterns
		if (!nameFromUri.empty() && nameFromUri.find('.') != std::string::npos) {
			baseFilename = nameFromUri;
			std::cout << "Using filename from URI: " << baseFilename << std::endl;
		}
	}

	// Priority 2: Filename from Content-Disposition
	if (baseFilename.empty() && !contentDispositionFilename.empty()) {
		baseFilename = contentDispositionFilename;
		std::cout << "Using filename from Content-Disposition: " << baseFilename << std::endl;
	}

	// Priority 3: Generate Timestamp Filename
	if (baseFilename.empty()) {
		baseFilename = generateTimestampFilename();
		std::cout << "Generating timestamp filename: " << baseFilename << std::endl;
	}

	// Sanitize the chosen name
	std::string sanitized = sanitizeFilename(baseFilename);
	if (sanitized != baseFilename) {
		std::cout << "Sanitized filename to: " << sanitized << std::endl;
	}
	// Handle case where sanitization resulted in an unusable name
	if (sanitized.empty() || sanitized == "_sanitized_empty_") {
		std::cerr << "Warning: Sanitization resulted in empty filename for original '" << baseFilename << "'. Generating new timestamp." << std::endl;
		sanitized = generateTimestampFilename() + ".bin"; // Add extension if generating
	}
	return sanitized;
}

/* @brief parses a single part of a multipart/form-data body
 *
 * extracts headers (specifically filename) and finds
 * the data portion as a string_view without copying the data
 *
 * @param partView A string_view representing the content of one part (between 2 boundaries)
 * @return ParsedPartInfo struct containing results.
 */
ParsedPartInfo	parsePart(std::string_view partView)
{
        ParsedPartInfo	 info;

	// headers and body is split by \r\n\r\n
	size_t		 headerEndPos = partView.find("\r\n\r\n");
	if (headerEndPos == std::string_view::npos) {
		std::cerr << "malformed part, no header/data separator\n";
		return info;
	}
	std::string_view headersView = partView.substr(0, headerEndPos);
	size_t		 dataStartPos = headerEndPos + 4; // skip \r\n\r\n

	// find possible filename
	size_t		 cdPos = headersView.find("Content-Disposition:");
	if (cdPos != std::string_view::npos)
	{
		size_t	 filenamePos = headersView.find("filename=\"", cdPos);
		if (filenamePos != std::string_view::npos)
		{
			filenamePos += 10;
			size_t filenameEndPos = headersView.find("\"", filenamePos);
			if (filenameEndPos != std::string_view::npos)
			{
				info.contentDispositionFilename = std::string(headersView.substr(filenamePos,
							filenameEndPos - filenamePos));
				info.isFile = true;
				info.contentDispositionFilename = sanitizeFilename(info.contentDispositionFilename);
			}
		}
	}

	if (dataStartPos < partView.length())
	{
		// the part view includes the trailing \r\n before the next boundary
		if (partView.length() >= dataStartPos + 2 && partView.substr(partView.length() - 2) == "\r\n") {
			info.data = partView.substr(dataStartPos, partView.length() - dataStartPos - 2);
		}
		else {
			info.data = partView.substr(dataStartPos);
			if (!info.data.empty()) {
				std::cerr << "Warning: Part data did not end with expected \\r\\n." << std::endl;
			}
		}
	}
	else {
		// No data after headers
		info.data = std::string_view();
	}

        if (!info.isFile && !info.data.empty()) {
		std::cerr << "non file upload?\n";
		// what to do to non file uploads for now?
        }
        return info;
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

    	std::string fileData = part.substr(dataStart, dataEnd - dataStart);
	if (access((path + filename).c_str(), F_OK) == 0) {
		responseBody = "{ \"status\": \"error\", \"message\": \"File already exists: " + filename + "\" }";
		return false;
	}

    	// save the file
    	std::ofstream outFile(path + filename, std::ios::binary);
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
	std::string			contentType = headers["Content-Type"];
	size_t				boundaryPos = contentType.find("boundary=");
	if (boundaryPos == std::string::npos) {
		std::cout << "Error: No boundary found in multipart/form-data" << std::endl;
		return false;
	}
	std::string			boundaryStart = "--" + contentType.substr(boundaryPos + 9);
	std::string			boundaryEnd = boundaryStart + "--";
	std::vector<FileUploadResult>	uploadResults;

	size_t startPos = body.find(boundaryStart); // Find first boundary
	if (startPos == std::string::npos) {
		std::cerr << "Unable to locate first boundary in Post body\n";
		return false;
	}
	startPos = body.find("\r\n", startPos);
	if (startPos == std::string::npos) {
		return false;
	}
	startPos += 2;

	size_t bodySize = body.size();
	while (startPos < bodySize)
	{
		size_t nextBoundaryPos = body.find(boundaryStart, startPos);
		if (nextBoundaryPos == std::string::npos) {
			std::cerr << "Unable to locate next boundary in Post body\n";
			break;
		}
		std::string_view partView(body.data() + startPos, nextBoundaryPos - startPos);
		bool isFinal = (body.substr(nextBoundaryPos, boundaryEnd.length()) == boundaryEnd);

		ParsedPartInfo partInfo = parsePart(partView);

		if (partInfo.isFile) {
			FileUploadResult result;

			result.originalFilename = partInfo.contentDispositionFilename;
			result.fileSize = partInfo.data.size();
			
			std::string baseFilename = determineFilename(path, partInfo.contentDispositionFilename);
			result.finalFilename = baseFilename; // Store the potentially sanitized/generated name
			result.savedPath = path + baseFilename;

			// Check for existence (using the final determined path)
			if (std::filesystem::exists(result.savedPath)) {
				result.success = false;
				result.message = "File already exists: " + baseFilename;
				std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
			}
			else {
				// Save the file (using the data view)
				std::ofstream outFile(result.savedPath, std::ios::binary | std::ios::trunc); // Use trunc to be safe
				if (!outFile) {
					result.success = false;
					result.message = "Could not create or open file for writing: " + baseFilename;
					std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
				}
				else {
					try {
						outFile.write(partInfo.data.data(), partInfo.data.size());
						if (!outFile) { // Check stream state *after* write
							result.success = false;
							result.message = "Error writing file data: " + baseFilename;
							std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
							outFile.close(); // Attempt to close
							std::filesystem::remove(result.savedPath); // Clean up partial file
						}
						else {
							outFile.close(); // Close the file
							if (!outFile) { // Check state *after* close
								result.success = false;
								result.message = "Error closing file after write: " + baseFilename;
								std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
								// File might be partially written but closed state is bad
							}
							else {
								result.success = true;
								result.message = "File uploaded successfully.";
								std::cout << result.message << " Path: " << result.savedPath << ", Size: " << result.fileSize << std::endl;
							}
						}
					} catch (const std::exception& e) {
						result.success = false;
						result.message = "Exception during file write: " + std::string(e.what());
						std::cerr << "Error: " << result.message << " for file " << result.savedPath << std::endl;
						if (outFile.is_open())
							outFile.close();
						std::filesystem::remove(result.savedPath); // Clean up
					}
				}
			}
			uploadResults.push_back(result);
		}
		else {
                 // Handle non-file parts if necessary
                 std::cout << "Skipping non-file part." << std::endl;
		}

		// Move startPos to the beginning of the next part (after the current boundary and its \r\n)
		startPos = nextBoundaryPos + boundaryStart.length();
		if (isFinal) {
			// We found the final boundary "--", processing is done.
			break;
		}
		else
		{
			// Skip the \r\n after the intermediate boundary
			if (startPos + 2 <= body.length() && body.substr(startPos, 2) == "\r\n") {
				startPos += 2;
			}
			else {
				std::cerr << "Warning: Boundary not followed by \\r\\n." << std::endl;
				// Decide how strict to be. If the next find works, maybe it's okay
				// If the next find fails, the outer loop condition or find will handle it.
			}
		}
	}

	// Build JSON response from results
	std::ostringstream jsonResponse;
	jsonResponse << "{ \"status\": \"processed\", \"uploads\": [";
	bool firstFile = true;
	for (const auto& res : uploadResults)
	{
		if (!firstFile) {
			jsonResponse << ",";
		}
		jsonResponse << "{";
		jsonResponse << "\"success\": " << (res.success ? "true" : "false");
		jsonResponse << ", \"originalFilename\": \"" << sanitizeFilename(res.originalFilename) << "\""; // Sanitize for JSON output too
		jsonResponse << ", \"finalFilename\": \"" << sanitizeFilename(res.finalFilename) << "\"";
		jsonResponse << ", \"savedPath\": \"" << sanitizeFilename(res.savedPath) << "\""; // Be careful about exposing full paths
		jsonResponse << ", \"size\": " << res.fileSize;
		jsonResponse << ", \"message\": \"" << res.message << "\""; // Basic escaping needed here for quotes in message
		jsonResponse << "}";
		firstFile = false;
	}
	jsonResponse << "] }";

        // Set the final response (assuming 200 OK even if some files failed, adjust if needed)
        response = createHttpResponse(200, jsonResponse.str(), "application/json");
        return true;
}
