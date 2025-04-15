#pragma once

# include "Configuration.hpp"

extern std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);