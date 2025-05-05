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

/* not used but if we need to generate name when one is missing
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
			result.savedPath = path + result.originalFilename;

			// check if exists already
			if (std::filesystem::exists(result.savedPath)) {
				result.success = false;
				result.message = "File already exists: " + result.originalFilename;
				std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
			}
			else {
				// Save the file
				std::ofstream outFile(result.savedPath, std::ios::binary | std::ios::trunc);
				if (!outFile) {
					result.success = false;
					result.message = "Could not create or open file for writing: " + result.savedPath;
					std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
				}
				else {
					try {
						outFile.write(partInfo.data.data(), partInfo.data.size());
						if (!outFile) { // Check stream state after write
							result.success = false;
							result.message = "Error writing file data: " + result.savedPath;
							outFile.close(); // Attempt to close
							std::filesystem::remove(result.savedPath); // Clean up partial file
						}
						else {
							outFile.close(); // Close the file
							if (!outFile) { // Check state *after* close
								result.success = false;
								result.message = "Error closing file after write: " + result.originalFilename;
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
                 std::cout << "Skipping non-file upload part" << std::endl;
		}

		// Move startPos to the beginning of the next part
		startPos = nextBoundaryPos + boundaryStart.length();
		if (isFinal) {
			break;
		}
		else
		{
			if (startPos + 2 <= body.length() && body.substr(startPos, 2) == "\r\n") {
				startPos += 2;
			}
			else {
				std::cerr << "Warning: Boundary not followed by \\r\\n." << std::endl;
				return false;
			}
		}
	}

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
		jsonResponse << ", \"originalFilename\": \"" << res.originalFilename << "\"";
		jsonResponse << ", \"finalFilename\": \"" << res.finalFilename << "\"";
		jsonResponse << ", \"savedPath\": \"" << res.savedPath << "\"";
		jsonResponse << ", \"size\": " << res.fileSize;
		jsonResponse << ", \"message\": \"" << res.message << "\"";
		jsonResponse << "}";
		firstFile = false;
	}
	jsonResponse << "] }";

        response = createHttpResponse(200, jsonResponse.str(), "application/json");
        return true;
}
