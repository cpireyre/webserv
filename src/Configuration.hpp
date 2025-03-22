#pragma once

#include <iostream>
#include <map>

#include "Configuration.hpp"

class Configuration {
	private:
		std::string								_globalMethods;		
		std::string								_globalCgiPathPHP;	
		std::string								_globalCgiPathPython;	
		std::map<int, std::string>				_errorPages;
		std::vector<std::string>				_rawBlock;				
		std::string 							_host;	
		std::string 							_port;			
		std::string								_serverNames;
		std::string								_index;	
		unsigned int 							_maxClientBodySize;
		//std::map<std::string, locationBlock>	m_routes;

		std::vector<std::string>				_rawServerBlock;

		void handleLocationBlock(std::vector<std::string>::iterator& it, const std::vector<std::string>& servBlck);
		void createBarebonesBlock();
	public:
		Configuration();
		~Configuration();
		Configuration(const Configuration& other);
		Configuration& operator=(const Configuration& other);

		Configuration(std::vector<std::string> servBlck);

		void printServerBlock() const;
};

int parser(void);