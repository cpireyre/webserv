#include "../include/Parser.hpp"
#include <signal.h>

extern sig_atomic_t g_ShouldStop;

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
    if (g_ShouldStop == true) return 0;
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
    if (g_ShouldStop == true) return;
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
	int pad = 0;
	if (level == 0) {
		std::cout << "├─"; pad += 2;
	}
	else {
		std::cout << "│"; pad += 1;
	}
	for (int i = 0; i < level; i++) {
		std::cout << " "; pad += 1;
	}
	if (level > 0) {
		std::cout << "└"; pad += 1;
	}
	if (!loc.nestedLocations.empty()) {
		std::cout << "┬"; pad += 1;
	}
	std::cout << "─"; pad += 1;
	std::cout << "⟨"; pad += 1;
	for (uint64_t i = 0; i < loc.methods.size(); i++) {
		std::cout << loc.methods[i]
			<< (i + 1 < loc.methods.size() ? " " : "");
		pad += loc.methods[i].size();
		pad += (i + 1 < loc.methods.size() ? 1 : 0);
	}
	std::cout << "⟩ "
		<< loc.path
		<< (loc.returnCode == 307 ? " ↷ " : " → ")
		<< (loc.returnCode == 307 ? loc.returnURL : loc.root);
	pad += 1 + (int)loc.path.length() + 3;
	pad += loc.returnCode == 307 ? (int)loc.returnURL.length() : (int)loc.root.length();
	printf("%*s", 80 - pad, "");
	std::cout << " (";
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
	constexpr char underline[] = "\e[0;4m";
	constexpr char reset[] = "\e[0m";
	std::cout
		<< "⌂ "
		<< underline
		<< _serverNames << " @ " << _host << ":" << _port << reset << " "
		<< "(" << "maxbody: " << _maxClientBodySize / 1e6 << "M"
		<< ", maxheader: " << _maxClientHeaderSize / 1e3 << "K" << ")"
		<< green << " ✓"  << reset
		<< "\n";
	for (const auto& loc : _locationBlocks)
		printLocationBlockCompact(loc, 0);
	std::cout << "└ ";
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
