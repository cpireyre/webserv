#pragma once

#include <iostream>
#include <map>

int parser(void);

class ConfigurationHandler {
	private:
		std::string								m_globalMethods;		
		std::string								m_globalCgiPathPHP;	
		std::string								m_globalCgiPathPython;	
		std::map<int, std::string>				m_defaultErrorPages;	
		std::vector<std::string>				m_rawBlock;				
		std::string 							m_host;	
		std::string 							m_port;			
		std::string								m_names;
		std::string								m_index;	
		// unsigned int 							m_maxClientBodySize;
		//std::map<std::string, locationBlock>	m_routes;
		std::map<int, std::string>				m_errorPages;
		// int										m_errorStatus = 0;
};