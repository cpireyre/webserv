#pragma once

#include <iostream>
#include <map>

#define G_CGI_PATH_PHP		"/usr/bin"
#define G_CGI_PATH_PYTHON	"/usr/bin"

struct LocationBlock {
    std::string path;                   // The location path (e.g. "/images/")
    std::string root;                   // The file system root for this location
    std::vector<std::string> methods;   // Allowed methods, e.g. {"GET", "POST", "DELETE"}
    std::string cgiPathPHP;             // Path for the PHP CGI interpreter (if provided)
    std::string cgiPathPython;          // Path for the Python CGI interpreter (if provided)
    std::string cgiPath;                // Generic CGI path (optional, for cases using "cgi_path")
    std::string uploadDir;              // Upload directory (optional)
    int returnCode;                     // HTTP status code for redirection (e.g. 307), default could be -1 or 0 if not set
    std::string returnURL;              // URL to redirect to if a return directive is present
    bool dirListing;                    // Directory listing flag (true for "on", false for "off")
    std::vector<LocationBlock> nestedLocations; // For any nested location blocks
};


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
		
		std::vector<LocationBlock>				_locationBlocks;

		std::vector<std::string>				_rawServerBlock;

		LocationBlock handleLocationBlock(std::vector<std::string>& locationBlock);
		std::vector<std::string> generateLocationBlock(std::vector<std::string>::iterator& it, std::vector<std::string>::iterator end);

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