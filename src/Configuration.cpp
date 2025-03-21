#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <map>
#include "Configuration.hpp"

// std::regex	listenRegex(R"(^listen (\d+)\s*;$)");

Configuration::Configuration() {}

Configuration::Configuration(const Configuration &other) {
	(void)other;
}

Configuration &Configuration::operator=(const Configuration &other) {
	(void)other;
	return *this;

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

Configuration::Configuration(std::vector<std::string> servBlck) : _rawServerBlock(servBlck) {
	createBarebonesBlock();

	std::regex listenRegex(R"(^listen (\d+)\s*;$)");
	std::regex hostRegex(R"(^host ([^\s]+)\s*;$)");
	std::regex serverNamesRegex(R"(^server_name ([^\s;]+(?: [^\s;]+)*)\s*;$)");
	std::regex maxClientBodyRegex(R"(^max_client_body_size (\d+)\s*;$)");
	std::regex errorPageRegex(R"(^error_page (400|403|404|405|408|409|411|413|414|431|500|501|503|505) (/home/\S+\.html)\s*;$)");
	std::regex indexRegex(R"(^index ([^\s]+)\s*;$)");
	std::regex locationRegex(R"(^location ([^\s]+)\s*$)");
	std::regex rootRegex(R"(^root /?([^/][^;]*[^/])?/?\s*;$)");
	std::regex returnRegex(R"(^return 307 (\S+)\s*;$)");
	std::regex methodsRegex(R"(^methods ([^\s;]+(?: [^\s;]+)*)\s*;$)");
	std::regex uploadDirRegex(R"(^upload_dir (home/\S+/)\s*;$)");
	std::regex dirListingRegex(R"(^dir_listing (on|off)\s*;$)");
	std::regex cgiPathRegexPHP(R"(^cgi_path_php (\/[^/][^;]*[^/])?/?\s*;$)");
	std::regex cgiPathRegexPython(R"(^cgi_path_python (\/[^/][^;]*[^/])?/?\s*;$)");

	for (const auto& line : servBlck) {
		std::smatch match;

		if (std::regex_search(line, match, maxClientBodyRegex)) {
			try {
				_maxClientBodySize = std::stoi(match[1]);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid max_client_body_size value: " << match[1] << " Using default value." << std::endl;
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
		// else if (std::regex_search(line, match, methodsRegex))
		// 	m_globalMethods = match[1];
		// else if (std::regex_search(line, match, cgiPathRegexPHP))
		// 	m_globalCgiPathPHP = match[1];
		// else if (std::regex_search(line, match, cgiPathRegexPython))
		// 	m_globalCgiPathPython = match[1];
	}

}

void populateConfigMap(const std::vector<std::string>& rawFile, std::multimap<std::string, Configuration> serverMap)
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
		if (line.find('}') != line.npos)
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
	for (const auto& server : serverCount) {
		std::cout << "Server #" << server.second << " on port " << server.first << std::endl;
	}
}

int parser(void) {
	std::string fileName = "complete.conf";
	std::vector<std::string> rawFile;

	if (getRawFile(fileName, rawFile) != 0) {
		std::cerr << "Error parsing file" << std::endl;
		return 1;
	}

	for (const auto& line : rawFile)
		std::cout << line << std::endl;

	std::multimap<std::string, Configuration> serverMap;

	populateConfigMap(rawFile, serverMap);

	return 0;
}