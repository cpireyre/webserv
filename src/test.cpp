#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept> // For exceptions
#include <filesystem> // Requires C++17
#include <chrono>     // For timestamp filenames
#include <iomanip>    // For formatting timestamps
#include <string_view> // Requires C++17
#include <algorithm>  // For std::remove_if

// --- Helper Structs ---

// Holds information about a parsed part
struct ParsedPartInfo {
    bool isFile = false;
    std::string contentDispositionFilename; // Filename from header, might be empty
    std::string_view data; // View of the actual file content within the original body string
    // Could add other fields like 'name' attribute, 'Content-Type' etc. if needed
};

// Holds the result of attempting to save one file
struct FileUploadResult {
    bool success = false;
    std::string originalFilename; // Filename from Content-Disposition or generated
    std::string finalFilename;    // Actual filename used for saving (after potential sanitization/uniquification)
    std::string savedPath;        // Full path where the file was saved
    size_t fileSize = 0;
    std::string message; // Success message or error description
};


// --- Class Definition ---

class HttpConnectionHandler {
private:
    // Assume these members exist and are populated before calling handleFileUpload
    std::string body;              // The entire request body
    std::string requestTargetUri; // e.g., "/upload/specific_name.txt" or "/upload"
    std::string uploadDirectory;   // Base directory for uploads, e.g., "uploads/" (ensure trailing slash)
    std::string response;          // To store the final HTTP response

    // --- Helper Functions ---

    /**
     * @brief Sanitizes a filename to prevent directory traversal and invalid characters.
     *
     * Removes potentially harmful sequences like "../", "/", "\" and control characters.
     * Replaces spaces with underscores.
     *
     * @param filename The raw filename to sanitize.
     * @return A sanitized version of the filename.
     */
    std::string sanitizeFilename(const std::string& filename) {
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

        // 3. Replace spaces with underscores (optional, but common)
        std::replace(sanitized.begin(), sanitized.end(), ' ', '_');

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

    /**
     * @brief Generates a unique filename based on the current timestamp.
     *
     * Format: YYYYMMDD_HHMMSS_microseconds_upload
     *
     * @return A unique string suitable for use as a filename.
     */
    std::string generateTimestampFilename() {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S");
        ss << "_" << std::setfill('0') << std::setw(6) << ms.count();
        ss << "_upload"; // Add a suffix
        return ss.str();
    }


    /**
     * @brief Determines the filename to use based on priority.
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
    std::string determineFilename(const std::string& reqTargetUri, const std::string& contentDispositionFilename) {
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


    /**
     * @brief Parses a single part of a multipart/form-data body.
     *
     * Extracts headers (specifically Content-Disposition filename) and identifies
     * the data portion as a string_view without copying the data.
     *
     * @param partView A string_view representing the content of one part (between boundaries).
     * @return ParsedPartInfo struct containing results.
     */
    ParsedPartInfo parsePart(std::string_view partView) {
        ParsedPartInfo info;

        // Find header end (\r\n\r\n)
        size_t headerEndPos = partView.find("\r\n\r\n");
        if (headerEndPos == std::string_view::npos) {
            std::cerr << "Warning: Malformed part - no header/data separator found." << std::endl;
            return info; // Not a valid part
        }

        std::string_view headersView = partView.substr(0, headerEndPos);
        size_t dataStartPos = headerEndPos + 4; // Skip \r\n\r\n

        // Find Content-Disposition filename
        size_t cdPos = headersView.find("Content-Disposition:");
        if (cdPos != std::string_view::npos) {
            size_t filenamePos = headersView.find("filename=\"", cdPos);
            if (filenamePos != std::string_view::npos) {
                filenamePos += 10; // Length of "filename=\""
                size_t filenameEndPos = headersView.find("\"", filenamePos);
                if (filenameEndPos != std::string_view::npos) {
                    info.contentDispositionFilename = std::string(headersView.substr(filenamePos, filenameEndPos - filenamePos));
                    info.isFile = true; // Found filename, assume it's a file part
                     // Basic sanitization right away for the extracted name
                    info.contentDispositionFilename = sanitizeFilename(info.contentDispositionFilename);
                }
            }
            // Could also parse the 'name' attribute here if needed for non-file form fields
        }

        // Data is from dataStartPos to the end of the part, minus the trailing \r\n
        if (dataStartPos < partView.length()) {
             // The part view includes the trailing \r\n before the next boundary
             if (partView.length() >= dataStartPos + 2 && partView.substr(partView.length() - 2) == "\r\n") {
                 info.data = partView.substr(dataStartPos, partView.length() - dataStartPos - 2);
             } else {
                 // This might happen for the very last part before the final boundary "--"
                 // or if the part is malformed. Let's assume it's the end for now.
                 info.data = partView.substr(dataStartPos);
                 if (!info.data.empty()) { // Only warn if there was actual data expected
                    std::cerr << "Warning: Part data did not end with expected \\r\\n." << std::endl;
                 }
             }
        } else {
            // No data after headers
            info.data = std::string_view();
        }


        // If it wasn't marked as a file via filename, but has data, treat it as data.
        // If you need to distinguish non-file form fields, add parsing for the 'name' attribute.
        if (!info.isFile && !info.data.empty()) {
             // For now, we only care about files. If you need other form fields,
             // you'd set isFile = false and potentially store the 'name' and 'data'.
             // std::cout << "Found non-file form data part." << std::endl;
        }


        return info;
    }


    /**
     * @brief Creates a standard HTTP response string.
     * (Simplified version)
     */
    std::string createHttpResponse(int statusCode, const std::string& respBody, const std::string& contentType) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << statusCode << " " << (statusCode == 200 ? "OK" : "Bad Request") << "\r\n"; // Example status text
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << respBody.length() << "\r\n";
        oss << "Connection: close\r\n"; // Assuming close after response
        oss << "\r\n"; // End of headers
        oss << respBody;
        return oss.str();
    }


public:
    // Constructor (example)
    HttpConnectionHandler(std::string reqBody, std::string reqUri, std::string uploadDir) :
        body(std::move(reqBody)),
        requestTargetUri(std::move(reqUri)),
        uploadDirectory(std::move(uploadDir))
    {
         // Ensure upload directory has a trailing slash if it doesn't
        if (!uploadDirectory.empty() && uploadDirectory.back() != '/' && uploadDirectory.back() != '\\') {
            uploadDirectory += '/';
        }
         // Basic check if directory exists (assuming it was checked beforehand as requested)
        // if (!std::filesystem::exists(uploadDirectory)) {
        //     throw std::runtime_error("Upload directory does not exist: " + uploadDirectory);
        // }
    }


    /**
     * @brief Handles the file upload process for a multipart/form-data request.
     *
     * Parses the request body, extracts files, determines filenames, saves them,
     * and generates a JSON response summarizing the results.
     *
     * @return true if the request was processed (regardless of individual file success/failure),
     * false if the request was fundamentally malformed (e.g., no boundary).
     */
    bool handleFileUpload() {
        // Find Content-Type header (assuming it's pre-parsed into a map or similar)
        // For this example, let's find it in the raw body if needed, though it should be in headers
        std::string contentTypeHeader = "Content-Type: multipart/form-data; boundary="; // Example search
        size_t ctPos = body.find(contentTypeHeader); // Simplified search
        if (ctPos == std::string::npos) { // A real implementation would get this from parsed headers
             ctPos = body.find("content-type: multipart/form-data; boundary="); // Case-insensitive
        }


        if (ctPos == std::string::npos || body.find("boundary=", ctPos) == std::string::npos) {
             std::cerr << "Error: Invalid or missing Content-Type header for multipart/form-data." << std::endl;
             response = createHttpResponse(400, "{ \"status\": \"error\", \"message\": \"Invalid Content-Type for upload\" }", "application/json");
             return false;
        }


        size_t boundaryPos = body.find("boundary=", ctPos);
        if (boundaryPos == std::string::npos) {
             std::cerr << "Error: No boundary string found in Content-Type." << std::endl;
              response = createHttpResponse(400, "{ \"status\": \"error\", \"message\": \"Missing boundary in Content-Type\" }", "application/json");
             return false;
        }


        // Extract the boundary value. It might be quoted. It ends at the next \r\n.
        size_t boundaryValueStart = boundaryPos + 9; // length of "boundary="
        size_t boundaryValueEnd = body.find("\r\n", boundaryValueStart);
         if (boundaryValueEnd == std::string::npos) {
             boundaryValueEnd = body.length(); // Boundary might be the last part of the header section
         }
        std::string boundaryValue = body.substr(boundaryValueStart, boundaryValueEnd - boundaryValueStart);
        // Remove potential quotes around the boundary value
        boundaryValue.erase(std::remove(boundaryValue.begin(), boundaryValue.end(), '\"'), boundaryValue.end());


        std::string boundary = "--" + boundaryValue;
        std::string finalBoundary = boundary + "--";

        std::vector<FileUploadResult> uploadResults;

        // Find the first boundary marker after the headers
        size_t startPos = body.find(boundary); // Find first boundary
        if (startPos == std::string::npos) {
            std::cerr << "Error: Could not find the first boundary marker in the body." << std::endl;
            response = createHttpResponse(400, "{ \"status\": \"error\", \"message\": \"Malformed body - initial boundary not found\" }", "application/json");
            return false;
        }
        // Skip the first boundary line itself and the following \r\n
        startPos = body.find("\r\n", startPos);
        if (startPos == std::string::npos) {
             std::cerr << "Error: Malformed body - no newline after first boundary." << std::endl;
             response = createHttpResponse(400, "{ \"status\": \"error\", \"message\": \"Malformed body - no newline after boundary\" }", "application/json");
             return false;
        }
        startPos += 2; // Move past the \r\n


        // Loop through parts
        while (startPos < body.length()) {
            // Find the next boundary (either intermediate or final)
            size_t nextBoundaryPos = body.find(boundary, startPos);

            if (nextBoundaryPos == std::string::npos) {
                std::cerr << "Error: Malformed body - could not find subsequent boundary." << std::endl;
                // Potentially try to recover or just fail
                break; // Exit loop on error
            }

            // Create a view of the current part's content (including its trailing \r\n)
            std::string_view partView(body.data() + startPos, nextBoundaryPos - startPos);

            // Check if this is the final boundary marker
            bool isFinal = (body.substr(nextBoundaryPos, finalBoundary.length()) == finalBoundary);


            // Parse the part
            ParsedPartInfo partInfo = parsePart(partView);

            if (partInfo.isFile) {
                FileUploadResult result;
                result.originalFilename = partInfo.contentDispositionFilename; // Store the name from header
                result.fileSize = partInfo.data.size();

                // Determine the actual filename to use
                std::string baseFilename = determineFilename(requestTargetUri, partInfo.contentDispositionFilename);
                result.finalFilename = baseFilename; // Store the potentially sanitized/generated name
                result.savedPath = uploadDirectory + baseFilename;


                // Check for existence (using the final determined path)
                if (std::filesystem::exists(result.savedPath)) {
                    result.success = false;
                    result.message = "File already exists: " + baseFilename;
                    std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
                } else {
                    // Save the file (using the data view)
                    std::ofstream outFile(result.savedPath, std::ios::binary | std::ios::trunc); // Use trunc to be safe
                    if (!outFile) {
                        result.success = false;
                        result.message = "Could not create or open file for writing: " + baseFilename;
                         std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
                    } else {
                        try {
                            outFile.write(partInfo.data.data(), partInfo.data.size());
                            if (!outFile) { // Check stream state *after* write
                                 result.success = false;
                                 result.message = "Error writing file data: " + baseFilename;
                                 std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
                                 outFile.close(); // Attempt to close
                                 std::filesystem::remove(result.savedPath); // Clean up partial file
                            } else {
                                outFile.close(); // Close the file
                                if (!outFile) { // Check state *after* close
                                     result.success = false;
                                     result.message = "Error closing file after write: " + baseFilename;
                                     std::cerr << "Error: " << result.message << " at path " << result.savedPath << std::endl;
                                      // File might be partially written but closed state is bad
                                } else {
                                     result.success = true;
                                     result.message = "File uploaded successfully.";
                                     std::cout << result.message << " Path: " << result.savedPath << ", Size: " << result.fileSize << std::endl;
                                }
                            }
                        } catch (const std::exception& e) {
                             result.success = false;
                             result.message = "Exception during file write: " + std::string(e.what());
                             std::cerr << "Error: " << result.message << " for file " << result.savedPath << std::endl;
                             if (outFile.is_open()) outFile.close();
                             std::filesystem::remove(result.savedPath); // Clean up
                        }
                    }
                }
                 uploadResults.push_back(result);
            } else {
                 // Handle non-file parts if necessary
                 // std::cout << "Skipping non-file part." << std::endl;
            }


            // Move startPos to the beginning of the next part (after the current boundary and its \r\n)
            startPos = nextBoundaryPos + boundary.length();
            if (isFinal) {
                // We found the final boundary "--", processing is done.
                break;
            } else {
                 // Skip the \r\n after the intermediate boundary
                 if (startPos + 2 <= body.length() && body.substr(startPos, 2) == "\r\n") {
                     startPos += 2;
                 } else {
                     std::cerr << "Warning: Boundary not followed by \\r\\n." << std::endl;
                     // Decide how strict to be. If the next find works, maybe it's okay.
                     // If the next find fails, the outer loop condition or find will handle it.
                 }
            }
        } // End while loop


        // Build JSON response from results
        std::ostringstream jsonResponse;
        jsonResponse << "{ \"status\": \"processed\", \"uploads\": [";
        bool firstFile = true;
        for (const auto& res : uploadResults) {
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

     // Getter for the response
    const std::string& getResponse() const {
        return response;
    }
};

// --- Example Usage ---
/*
int main() {
    // --- Simulate an incoming request ---

    // 1. Request Target URI
    std::string requestUri = "/upload/target_name.zip"; // Example with filename in URI
    // std::string requestUri = "/upload"; // Example without filename in URI

    // 2. Upload Directory
    std::string uploadDir = "test_uploads/";
    try {
        // Ensure directory exists (as per user requirement that this is done beforehand)
        if (!std::filesystem::exists(uploadDir)) {
             std::filesystem::create_directories(uploadDir);
             std::cout << "Created upload directory: " << uploadDir << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating/accessing upload directory: " << e.what() << std::endl;
        return 1;
    }


    // 3. Request Body (Simulated multipart data)
    std::string boundaryValue = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    std::string boundary = "--" + boundaryValue;
    std::string finalBoundary = boundary + "--";

    std::ostringstream requestBodyStream;
    // Part 1: A text file
    requestBodyStream << boundary << "\r\n";
    requestBodyStream << "Content-Disposition: form-data; name=\"file1\"; filename=\"example.txt\"\r\n";
    requestBodyStream << "Content-Type: text/plain\r\n";
    requestBodyStream << "\r\n"; // Header/Body separator
    requestBodyStream << "This is the content of the first file.\r\nIt has multiple lines.\r\n"; // File data
    requestBodyStream << "\r\n"; // Separator before next boundary

    // Part 2: A simulated binary file (e.g., containing \r\n internally)
    requestBodyStream << boundary << "\r\n";
    requestBodyStream << "Content-Disposition: form-data; name=\"file2\"; filename=\"binary_data.bin\"\r\n";
    requestBodyStream << "Content-Type: application/octet-stream\r\n";
    requestBodyStream << "\r\n"; // Header/Body separator
    requestBodyStream << "Binary data start\r\n"; // Contains \r\n
    requestBodyStream << "More data\0with null\r\n"; // Contains null byte and \r\n
    requestBodyStream << "End of data.";
    requestBodyStream << "\r\n"; // Separator before next boundary

     // Part 3: Another file (will collide if target_name.zip exists and URI is used)
    requestBodyStream << boundary << "\r\n";
    requestBodyStream << "Content-Disposition: form-data; name=\"file3\"; filename=\"another.zip\"\r\n";
    requestBodyStream << "Content-Type: application/zip\r\n";
    requestBodyStream << "\r\n"; // Header/Body separator
    requestBodyStream << "ZIP..."; // File data
    requestBodyStream << "\r\n"; // Separator before next boundary


    // Part 4: A field without a filename (should be ignored by file processing)
    requestBodyStream << boundary << "\r\n";
    requestBodyStream << "Content-Disposition: form-data; name=\"text_field\"\r\n";
    requestBodyStream << "\r\n";
    requestBodyStream << "Some text value\r\n";
    requestBodyStream << "\r\n";

    // Final boundary
    requestBodyStream << finalBoundary << "\r\n"; // End marker

    std::string fullRequestBody = requestBodyStream.str();

    // Add simulated Content-Type header to the body for the example's parsing logic
    std::string simulatedRequest = "POST " + requestUri + " HTTP/1.1\r\n"
                                   "Host: example.com\r\n"
                                   "Content-Type: multipart/form-data; boundary=\"" + boundaryValue + "\"\r\n"
                                   "Content-Length: " + std::to_string(fullRequestBody.length()) + "\r\n\r\n" +
                                   fullRequestBody;


    // --- Process the request ---
    try {
        HttpConnectionHandler handler(simulatedRequest, requestUri, uploadDir); // Pass the full request for header parsing demo

        if (handler.handleFileUpload()) {
            std::cout << "\n--- Server Response ---" << std::endl;
            std::cout << handler.getResponse() << std::endl;
        } else {
            std::cerr << "\n--- File Upload Handling Failed ---" << std::endl;
             std::cout << handler.getResponse() << std::endl; // Print error response
        }
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        return 1;
    }


    return 0;
}
*/
