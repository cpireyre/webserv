#include "../include/Parser.hpp"

int getRawFile(std::string& fileName, std::vector<std::string>& rawFile) {
	int brace = 0;

	std::regex confRegex(R"((.+)\.conf$)");
	if (!std::regex_search(fileName, confRegex))
		throw std::runtime_error("File extension must be .conf");
	
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

void populateConfigMap(const std::vector<std::string>& rawFile, std::vector<Configuration>& srvrMap)
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
			srvrMap.push_back(Configuration(serverBlock));
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
		populateConfigMap(rawFile, serverMap);
	}
	catch (std::exception &e) {
		throw;
	}
	return serverMap;
}


void Configuration::printLocationBlockCompact(LocationBlock loc, int level) const
{
	if (level == 0)
		std::cout << "├─";
	else
		std::cout << "│";
	for (int i = 0; i < level; i++)
		std::cout << " ";
	if (level > 0)
		std::cout << "└";
	if (!loc.nestedLocations.empty())
		std::cout << "┬";
	std::cout << "─";
	std::cout << "⟨";
	for (uint64_t i = 0; i < loc.methods.size(); i++)
	{
		std::cout << loc.methods[i]
			<< (i + 1 < loc.methods.size() ? " " : "");
	}
	std::cout << "⟩ "
		<< loc.path << " → ./" << loc.root
		<< " (";
		std::cout << (loc.dirListing ? "●" : "○");
		std::cout<< " dir ";
		std::cout << (!loc.cgiPathPHP.empty() ? "●" : "○");
	std::cout << " php ";
		std::cout << (!loc.cgiPathPython.empty() ? "●" : "○");
	std::cout << " py";
	std::cout << ")";
	std::cout << "\n";
	for (const auto& o : loc.nestedLocations)
		printLocationBlockCompact(o, level + 1);
}

void	Configuration::printCompact() const
{
	constexpr char green[] = "\e[0;32m";
	constexpr char reset[] = "\e[0m";
	std::cout
		<< "⌂ "
		<< _serverNames << " @ " << _host << ":" << _port << " "
		<< "(" << "maxbody: " << _maxClientBodySize / 10e6 << "M"
		<< ", maxheader: " << _maxClientHeaderSize / 1e3 << "K" << ")"
		<< green << " ✓"  << reset
		<< "\n";
	for (const auto& loc : _locationBlocks)
		printLocationBlockCompact(loc, 0);
	std::cout << "└ .";
	std::string err_path = _errorPages.begin()->second;
	std::string err_dir = err_path.substr(0, err_path.size() - 8);
	std::cout << err_dir << "{";
	auto it = _errorPages.begin();
    for (; it != _errorPages.end(); it = std::next(it)) {
        std::cout << it->first;

        auto next = std::next(it);
        if (next != _errorPages.end()) {
            std::cout << ",";
        }
    }
	std::cout << "}.html";
	std::cout << "\n\n";
}
