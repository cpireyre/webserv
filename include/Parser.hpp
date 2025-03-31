#pragma once

# include "Configuration.hpp"

extern std::vector<Configuration> serverMap;

int getRawFile(std::string& fileName, std::vector<std::string>& rawFile);
void populateConfigMap(const std::vector<std::string>& rawFile, std::multimap<std::string, Configuration>& serverMap);
std::vector<Configuration> parser(std::string fileName);