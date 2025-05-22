#include "../include/Configuration.hpp"


Configuration::Configuration() {}

Configuration::Configuration(const Configuration &other)
	: _globalMethods(other._globalMethods),
	  _globalCgiPathPHP(other._globalCgiPathPHP),
	  _globalCgiPathPython(other._globalCgiPathPython),
	  _errorPages(other._errorPages),
	  _host(other._host),
	  _port(other._port),
	  _serverNames(other._serverNames),
	  _index(other._index),
	  _maxClientBodySize(other._maxClientBodySize),
	  _maxClientHeaderSize(other._maxClientHeaderSize),
	  _locationBlocks(other._locationBlocks),
	  _allPaths(other._allPaths),
	  _rawBlock(other._rawBlock),
	  _rawServerBlock(other._rawServerBlock)
{}

Configuration &Configuration::operator=(const Configuration &other) {
	if (this != &other) {
		_globalMethods = other._globalMethods;
		_globalCgiPathPHP = other._globalCgiPathPHP;
		_globalCgiPathPython = other._globalCgiPathPython;
		_errorPages = other._errorPages;
		_host = other._host;
		_port = other._port;
		_serverNames = other._serverNames;
		_index = other._index;
		_maxClientBodySize = other._maxClientBodySize;
		_maxClientHeaderSize = other._maxClientHeaderSize;
		_locationBlocks = other._locationBlocks;
		_allPaths = other._allPaths;
		_rawBlock = other._rawBlock;
		_rawServerBlock = other._rawServerBlock;
	}
	return *this;
}

// Prints location blocks recursively. Level denotes nestedness, and used for indentation.
void Configuration::printLocationBlock(LocationBlock loc, int level) const {
	std::string indent(level, ' ');

	std::cout << indent << "Path: " << GREEN << loc.path << DEFAULT_COLOR << std::endl;
	std::cout << indent << "Root: " << loc.root << std::endl;
	std::cout << indent << "Methods: ";
	for (const auto& method : loc.methods) {
		std::cout << method << " ";
	}
	std::cout << std::endl;
	std::cout << indent << "CGI Path PHP: " << loc.cgiPathPHP << std::endl;
	std::cout << indent << "CGI Path Python: " << loc.cgiPathPython << std::endl;
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
	std::cout << "Max Client Header Size: " << _maxClientHeaderSize << std::endl;
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

void Configuration::createBarebonesBlock() {
	_index = "index.html";
	_host = "0.0.0.0"; // In case no host is specified, defaulting to all interfaces. Nginx default I think.
	_port = DEFAULT_LISTEN;
	_maxClientBodySize = 1048576; // 1MB, nginx default
	_maxClientHeaderSize = 4000;
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
	_errorPages.emplace(415, "/default-error-pages/415.html");
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
	std::regex maxClientHeaderRegex(R"(^max_client_header_size (\d+)\s*;$)");
	std::regex errorPageRegex(R"(^error_page (400|403|404|405|408|409|411|413|414|415|431|500|501|503|505) (/home/\S+\.html)\s*;$)");
	std::regex indexRegex(R"(^index ([^\s]+)\s*;$)");
	std::regex locationRegex(R"(^location ([^\s]+)\s*$)");

	for (std::vector<std::string>::iterator it = servBlck.begin(); it != servBlck.end(); ++it) {
		std::string line = *it;
		std::smatch match;

		if (std::regex_search(line, match, maxClientBodyRegex)) {
			try {
				unsigned int temp = std::stoul(match[1]);
				if (temp < _maxClientBodySize)	
					_maxClientBodySize = temp;
			}
			catch (const std::exception& e) {
				std::cerr << "Invalid max_client_body_size value: " << match[1] << ". Using default value." << std::endl;
			}
		}
		else if (std::regex_search(line, match, maxClientHeaderRegex)) {
			try {
				_maxClientHeaderSize = std::stoul(match[1]);
			}
			catch (const std::exception& e) {
				std::cerr << "Invalid max_client_header_size value: " << match[1] << ". Using default value." << std::endl;
			}
		}
		else if (std::regex_search(line, match, listenRegex))
			_port = match[1];
		else if (std::regex_search(line, match, hostRegex))
			_host = match[1];
		else if (std::regex_search(line, match, serverNamesRegex))
			_serverNames = match[1];
		else if (std::regex_search(line, match, errorPageRegex))
			_errorPages.insert_or_assign(std::stoi(match[1]), match[2]);
		else if (std::regex_search(line, match, indexRegex))
			_index = match[1];
		else if (std::regex_search(line, match, locationRegex)) {

			std::vector<std::string> locationBlock = generateLocationBlock(it, servBlck.end());

			LocationBlock loc = handleLocationBlock(locationBlock);
			_locationBlocks.push_back(loc);
		}
	}

	for (auto& locationBlock : _locationBlocks)
		populateMethodsPathsCgi(locationBlock, DEFAULT_METHODS, DEFAULT_CGI_PYTHON, DEFAULT_CGI_PHP);
}

void Configuration::populateMethodsPathsCgi(LocationBlock& locationBlock, std::vector<std::string> inheritedMethods, std::string inheritedCgiPathPython, std::string inheritedCgiPathPHP) {

	if (locationBlock.methods.empty())
		locationBlock.methods.insert(locationBlock.methods.end(), inheritedMethods.begin(), inheritedMethods.end());
	else
		inheritedMethods = locationBlock.methods;

	if (locationBlock.cgiPathPython.empty())
		locationBlock.cgiPathPython = inheritedCgiPathPython;
	else
		inheritedCgiPathPython = locationBlock.cgiPathPython;

	if (locationBlock.cgiPathPHP.empty())
		locationBlock.cgiPathPHP = inheritedCgiPathPHP;
	else
		inheritedCgiPathPHP = locationBlock.cgiPathPHP;

	_allPaths.insert(std::make_pair(locationBlock.path, locationBlock));
	
	std::vector<LocationBlock> &nestedLocations = locationBlock.nestedLocations;

	for (auto& nestedLocation : nestedLocations)
		populateMethodsPathsCgi(nestedLocation, inheritedMethods, inheritedCgiPathPython, inheritedCgiPathPHP);
}

std::vector<LocationBlock>& Configuration::getLocationBlocks() {
	return _locationBlocks;
}

std::vector<std::string> Configuration::getGlobalMethods() const {
	return _globalMethods;
}

std::string	Configuration::getGlobalCgiPathPHP() const {
	return _globalCgiPathPHP;
}

std::string	Configuration::getGlobalCgiPathPython() const {
	return _globalCgiPathPython;
}

std::map<int, std::string>	Configuration::getErrorPages() const {
	return _errorPages;
}

std::string	Configuration::getHost() const {
	return _host;
}

std::string	Configuration::getPort() const {
	return _port;
}

std::string	Configuration::getServerNames() const {
	return _serverNames;
}

std::string	Configuration::getIndex() const {
	return _index;
}

unsigned int	Configuration::getMaxClientBodySize() const {
	return _maxClientBodySize;
}

unsigned int	Configuration::getMaxClientHeaderSize() const {
	return _maxClientHeaderSize;
}

std::string Configuration::getRootViaLocation(std::string path) const {
	std::map<std::string, LocationBlock>::const_iterator it = _allPaths.find(path);
	if (it != _allPaths.end())
		return it->second.root;
	return "";
}
