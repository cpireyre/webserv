#pragma once

# include "Configuration.hpp"

int getRawFile(std::string& fileName, std::vector<std::string>& rawFile);
// void populateMethods(Configuration& server, std::vector<std::string> inheritedMethods);
void populateConfigMap(const std::vector<std::string>& rawFile, std::multimap<std::string, Configuration>& serverMap);
int parser(std::string fileName);