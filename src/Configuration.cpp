#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include "Configuration.hpp"
#include <sstream>

Configuration::Configuration() {}

Configuration::Configuration(const Configuration &other)
	: _globalMethods(other._globalMethods),
	  _globalCgiPathPHP(other._globalCgiPathPHP),
	  _globalCgiPathPython(other._globalCgiPathPython),
	  _errorPages(other._errorPages),
	  _rawBlock(other._rawBlock),
	  _host(other._host),
	  _port(other._port),
	  _serverNames(other._serverNames),
	  _index(other._index),
	  _maxClientBodySize(other._maxClientBodySize),
	  _locationBlocks(other._locationBlocks),
	  _rawServerBlock(other._rawServerBlock)
{}

Configuration &Configuration::operator=(const Configuration &other) {
	if (this != &other) {
		_globalMethods = other._globalMethods;
		_globalCgiPathPHP = other._globalCgiPathPHP;
		_globalCgiPathPython = other._globalCgiPathPython;
		_errorPages = other._errorPages;
		_rawBlock = other._rawBlock;
		_host = other._host;
		_port = other._port;
		_serverNames = other._serverNames;
		_index = other._index;
		_maxClientBodySize = other._maxClientBodySize;
		_locationBlocks = other._locationBlocks;
		_rawServerBlock = other._rawServerBlock;
	}
	return *this;
}

// Prints location blocks recursively. Level denotes nestedness, and used for indentation.
void Configuration::printLocationBlock(LocationBlock loc, int level) const {
	std::string indent(level + 2, ' ');

	std::cout << indent << "Path: " << GREEN << loc.path << DEFAULT_COLOR << std::endl;
	std::cout << indent << "Root: " << loc.root << std::endl;
	std::cout << indent << "Methods: ";
	for (const auto& method : loc.methods) {
		std::cout << method << " ";
	}
	std::cout << std::endl;
	std::cout << indent << "CGI Path PHP: " << loc.cgiPathPHP << std::endl;
	std::cout << indent << "CGI Path Python: " << loc.cgiPathPython << std::endl;
	std::cout << indent << "CGI Path (default): " << loc.cgiPathPython << std::endl;
	std::cout << indent << "Upload Directory: " << loc.uploadDir << std::endl;
	std::cout << indent << "Return Code: " << loc.returnCode << std::endl;
	std::cout << indent << "Return URL: " << loc.returnURL << std::endl;
	std::cout << indent << "Directory Listing: " << (loc.dirListing ? "on" : "off") << std::endl;
	if (!loc.nestedLocations.empty()) {
		for (const auto& nestedLoc : loc.nestedLocations) {
			std::cout << indent << BLUE << "  Nested Location Block:" << DEFAULT_COLOR << std::endl;
			printLocationBlock(nestedLoc, level + 2);
		}
	}
}

void Configuration::printServerBlock() const {
	std::cout << "Server Block:" << std::endl;
	std::cout << "Port: " << _port << std::endl;
	std::cout << "Host: " << _host << std::endl;
	std::cout << "Server Names: " << _serverNames << std::endl;
	std::cout << "Max Client Body Size: " << _maxClientBodySize << std::endl;
	std::cout << "Index: " << _index << std::endl;
	for (const auto& errorPage : _errorPages) {
		std::cout << "Error Page [" << errorPage.first << "]: " << errorPage.second << std::endl;
	}
	std::cout << "Location Blocks:" << std::endl;
	for (const auto& loc : _locationBlocks) {
		printLocationBlock(loc, 0);
	}
}

Configuration::~Configuration() {}

int getRawFile(std::string& fileName, std::vector<std::string>& rawFile) {
	int brace = 0;
	
	std::ifstream file(fileName);
	if (!file) {
		std::cerr << "Error opening file" << std::endl;
		return 1;
	}

	std::string line;
	while (std::getline(file, line)) {
		size_t comment = line.find('#');
		// remove comments
		if (comment != std::string::npos)
			line = line.substr(0, comment);
		// remove leading & trailing whitespaces
		line = std::regex_replace(line, std::regex("^\\s+|\\s+$"), "");
		// reduce multiple whitespaces to one
		line = std::regex_replace(line, std::regex("\\s+"), " ");

		if (line.empty())
			continue;
		if (line.find('{') != line.npos)
			brace++;
		if (line.find('}') != line.npos)
			brace--;
		
		if (!line.empty())
			rawFile.push_back(line);
	}
	if (brace != 0)
		throw std::runtime_error("Unmatched braces in file");
	if (rawFile.empty())
		throw std::runtime_error("File empty");
	file.close();
	return 0;
}

void Configuration::createBarebonesBlock() {
	_index = "index.html";
	_host = "0.0.0.0"; // In case no host is specified, defaulting to all interfaces. Nginx default I think.
	_maxClientBodySize = 1048576; // 1MB, nginx default
	_globalCgiPathPHP = G_CGI_PATH_PHP;
	_globalCgiPathPython = G_CGI_PATH_PYTHON;
	_errorPages.emplace(400, "/default-error-pages/400.html");
	_errorPages.emplace(403, "/default-error-pages/403.html");
	_errorPages.emplace(404, "/default-error-pages/404.html");
	_errorPages.emplace(405, "/default-error-pages/405.html");
	_errorPages.emplace(408, "/default-error-pages/408.html");
	_errorPages.emplace(409, "/default-error-pages/409.html");
	_errorPages.emplace(411, "/default-error-pages/411.html");
	_errorPages.emplace(413, "/default-error-pages/413.html");
	_errorPages.emplace(414, "/default-error-pages/414.html");
	_errorPages.emplace(431, "/default-error-pages/431.html");
	_errorPages.emplace(500, "/default-error-pages/500.html");
	_errorPages.emplace(501, "/default-error-pages/501.html");
	_errorPages.emplace(503, "/default-error-pages/503.html");
	_errorPages.emplace(505, "/default-error-pages/505.html");
}

std::vector<std::string> Configuration::generateLocationBlock(std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end) {

	std::vector<std::string> locationBlock;
	std::string line;
	int brace = 0;
	while (it != end) {
		line = *it;
		if (line.find('{') != line.npos)
			brace++;
		else if (line.find('}') != line.npos) {
			brace--;
			if (!brace) {
				locationBlock.push_back(line);
				//++it;
				break;
			}
		}
		locationBlock.push_back(line);
		++it;
	}
	return locationBlock;
}

LocationBlock Configuration::handleLocationBlock(std::vector<std::string>& locationBlock) {
    LocationBlock loc;
	std::regex locationRegex(R"(^location ([^\s]+)\s*$)");
	std::regex rootRegex(R"(^root /?([^/][^;]*[^/])?/?\s*;$)");
	std::regex returnRegex(R"(^return 307 (\S+)\s*;$)");
	std::regex methodsRegex(R"(^methods ([^\s;]+(?: [^\s;]+)*)\s*;$)");
	std::regex uploadDirRegex(R"(^upload_dir (home/\S+/)\s*;$)");
	std::regex dirListingRegex(R"(^dir_listing (on|off)\s*;$)");
	std::regex cgiPathRegexPHP(R"(^cgi_path_php (\/[^/][^;]*[^/])?/?\s*;$)");
	std::regex cgiPathRegexPython(R"(^cgi_path_python (\/[^/][^;]*[^/])?/?\s*;$)");

	std::smatch match;
	int brace = 0;
	loc.returnCode = -1; // Default value for return code
	loc.returnURL = ""; // Default value for return URL
	loc.dirListing = false; // Default value for directory listing
	std::vector<std::string>::iterator it_begin = locationBlock.begin();
	std::vector<std::string>::iterator it_end = locationBlock.end();

	std::regex_search(*it_begin, match, locationRegex);
	loc.path = match[1];
	++it_begin;

	while (it_begin != it_end) {
		std::string line = *it_begin;

		if (line.find('{') != line.npos)
			brace++;
		else if (line.find('}') != line.npos) {
			brace--;
			if (!brace)
				break;
		}
		else if (std::regex_search(line, match, locationRegex)) {
			// std::cout << "Nested location block found: " << match[1] << std::endl;
			std::vector<std::string> locationBlockRaw = generateLocationBlock(it_begin, it_end);
			LocationBlock nestedLoc = handleLocationBlock(locationBlockRaw);
			loc.nestedLocations.push_back(nestedLoc);
		}
		else if (std::regex_search(line, match, rootRegex))
			loc.root = match[1];
		else if (std::regex_search(line, match, methodsRegex)) {
			std::string methodsStr = match[1];
			std::istringstream iss(methodsStr);
			std::string method;
			while (iss >> method) {
				loc.methods.push_back(method);
			}
		}
		else if (std::regex_search(line, match, cgiPathRegexPHP))
			loc.cgiPathPHP = match[1];
		else if (std::regex_search(line, match, cgiPathRegexPython))
			loc.cgiPathPython = match[1];
		else if (std::regex_search(line, match, uploadDirRegex))
			loc.uploadDir = match[1];
		else if (std::regex_search(line, match, returnRegex)) {
			loc.returnCode = 307;
			loc.returnURL = match[1];
		}
		else if (std::regex_search(line, match, dirListingRegex))
			loc.dirListing = (match[1] == "on");
		it_begin++;
	}
    return loc;
}


Configuration::Configuration(std::vector<std::string> servBlck) : _rawServerBlock(servBlck) {
	createBarebonesBlock();

	std::regex listenRegex(R"(^listen (\d+)\s*;$)");
	std::regex hostRegex(R"(^host ([^\s]+)\s*;$)");
	std::regex serverNamesRegex(R"(^server_name ([^\s;]+(?: [^\s;]+)*)\s*;$)");
	std::regex maxClientBodyRegex(R"(^max_client_body_size (\d+)\s*;$)");
	std::regex errorPageRegex(R"(^error_page (400|403|404|405|408|409|411|413|414|431|500|501|503|505) (/home/\S+\.html)\s*;$)");
	std::regex indexRegex(R"(^index ([^\s]+)\s*;$)");
	std::regex locationRegex(R"(^location ([^\s]+)\s*$)");

	for (std::vector<std::string>::iterator it = servBlck.begin(); it != servBlck.end(); ++it) {
		std::string line = *it;
		std::smatch match;

		if (std::regex_search(line, match, maxClientBodyRegex)) {
			try {
				_maxClientBodySize = std::stoul(match[1]);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid max_client_body_size value: " << match[1] << ". Using default value." << std::endl;
			}
		}
		else if (std::regex_search(line, match, listenRegex))
			_port = match[1];
		else if (std::regex_search(line, match, hostRegex))
			_host = match[1];
		else if (std::regex_search(line, match, serverNamesRegex))
			_serverNames = match[1];
		else if (std::regex_search(line, match, errorPageRegex))
			_errorPages.emplace(std::stoi(match[1]), match[2]);
		else if (std::regex_search(line, match, indexRegex))
			_index = match[1];
		else if (std::regex_search(line, match, locationRegex)) {

			std::vector<std::string> locationBlock = generateLocationBlock(it, servBlck.end());

			LocationBlock loc = handleLocationBlock(locationBlock);
			_locationBlocks.push_back(loc);
		}
	}
}

void populateConfigMap(const std::vector<std::string>& rawFile, std::multimap<std::string, Configuration>& serverMap)
{

	std::vector<std::string> serverBlock;
	std::string port;
	int brace = 0;
	std::regex	listenRegex(R"(^listen (\d+)\s*;$)");
	int serverCountForTestPurposes = 0;
	std::multimap<std::string, int> serverCount;

	std::regex regex(R"((\w+)\s*:\s*(.+);)");

	for (const auto& line : rawFile) {
		std::smatch match;
		if (regex_search(line, match, std::regex("^server$"))) {
			serverBlock.push_back(line);
			continue;
		}
		if (regex_search(line, match, listenRegex))
			port = match[1];
		if (line.find('{') != line.npos)
			brace++;
		else if (line.find('}') != line.npos)
			brace--;
		if (!brace) {
			serverBlock.push_back(line);
			serverMap.emplace(port, Configuration(serverBlock));
			serverCount.emplace(port, ++serverCountForTestPurposes);
			serverBlock.clear();
			port.clear();
		}
		if (!serverBlock.empty())
			serverBlock.push_back(line);
	}


	for (const auto& server : serverMap) {
		server.second.printServerBlock();
	}
	for (const auto& server : serverCount) {
		std::cout << "Server #" << server.second << " on port " << server.first << std::endl;
	}
}

int parser(std::string fileName) {
	std::vector<std::string> rawFile;

	if (getRawFile(fileName, rawFile) != 0) {
		std::cerr << "Error parsing file" << std::endl;
		return 1;
	}

	// for (const auto& line : rawFile)
	// 	std::cout << line << std::endl;

	std::multimap<std::string, Configuration> serverMap;

	populateConfigMap(rawFile, serverMap);

	return 0;
}