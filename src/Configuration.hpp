#pragma once

#include <iostream>
#include <map>

#include "Configuration.hpp"

class Configuration {
	private:
		std::string								m_globalMethods;		
		std::string								m_globalCgiPathPHP;	
		std::string								m_globalCgiPathPython;	
		std::map<int, std::string>				_errorPages;
		// std::map<int, std::string>				_defaultErrorPages;	
		std::vector<std::string>				m_rawBlock;				
		std::string 							_host;	
		std::string 							_port;			
		std::string								m_names;
		std::string								_index;	
		unsigned int 							_maxClientBodySize;
		//std::map<std::string, locationBlock>	m_routes;
		// int										m_errorStatus = 0;

		std::vector<std::string>				_rawServerBlock;
	public:
		Configuration();
		~Configuration();
		Configuration(const Configuration& other);
		Configuration& operator=(const Configuration& other);

		Configuration(std::vector<std::string> servBlck);
		void createBarebonesBlock();
};

int parser(void);