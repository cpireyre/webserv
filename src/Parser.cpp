#include "../include/Parser.hpp"

int getRawFile(std::string& fileName, std::vector<std::string>& rawFile) {
	int brace = 0;
	
	std::ifstream file(fileName);
	if (!file)
		throw std::runtime_error("Error opening file");
	
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

void populateConfigMap(const std::vector<std::string>& rawFile, std::vector<Configuration>& serverMap)
{

	std::vector<std::string> serverBlock;
	std::string port;
	int brace = 0;
	std::regex	listenRegex(R"(^listen (\d+)\s*;$)");

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
			if (port.empty())
				port = DEFAULT_LISTEN;
			serverMap.push_back(Configuration(serverBlock));
			serverBlock.clear();
			port.clear();
		}
		if (!serverBlock.empty())
			serverBlock.push_back(line);
	}
}

std::vector<Configuration> parser(std::string fileName) {
	std::vector<std::string> rawFile;
	
	try {
		getRawFile(fileName, rawFile);
	}
	catch (std::exception &e) {
		throw e;
	}

	populateConfigMap(rawFile, serverMap);

	return serverMap;
}
