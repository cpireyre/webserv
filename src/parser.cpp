#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>

// std::regex	listenRegex(R"(^listen (\d+)\s*;$)");

int parseFile(std::string& fileName, std::vector<std::string>& rawFile) {
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

int parser(void) {
	std::string fileName = "complete.conf";
	std::vector<std::string> rawFile;

	if (parseFile(fileName, rawFile) != 0) {
		std::cerr << "Error parsing file" << std::endl;
		return 1;
	}

	for (const auto& line : rawFile)
		std::cout << line << std::endl;

	return 0;
}