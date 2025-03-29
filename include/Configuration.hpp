#pragma once

# include <iostream>
# include <map>
# include <fstream>
# include <string>
# include <vector>
# include <regex>
# include <sstream>

# define G_CGI_PATH_PHP		"/usr/bin"
# define G_CGI_PATH_PYTHON	"/usr/bin"
# define GREEN				"\033[32m"
# define BLUE				"\033[34m"
# define DEFAULT_COLOR		"\033[0m"

const std::vector<std::string> DEFAULT_METHODS = {"GET"};
const std::string DEFAULT_LISTEN = "80";

struct LocationBlock {
    std::string path;                   		// The location path (e.g. "/images/")
    std::string root;                   		// The file system root for this location
    std::vector<std::string> methods;   		// Allowed methods, e.g. {"GET", "POST", "DELETE"}
    std::string cgiPathPHP;             		// Path for the PHP CGI interpreter (if provided)
    std::string cgiPathPython;          		// Path for the Python CGI interpreter (if provided)
    std::string uploadDir;              		// Upload directory (optional)
    int returnCode;                     		// HTTP status code for redirection (e.g. 307), default could be -1 or 0 if not set
    std::string returnURL;              		// URL to redirect to if a return directive is present
    bool dirListing;                    		// Directory listing flag (true for "on", false for "off")
    std::vector<LocationBlock> nestedLocations;	// For any nested location blocks
};

class Configuration {
	private:
		std::vector<std::string>				_globalMethods;		
		std::string								_globalCgiPathPHP;	
		std::string								_globalCgiPathPython;	
		std::map<int, std::string>				_errorPages;				
		std::string 							_host;	
		std::string 							_port;
		std::string								_serverNames;
		std::string								_index;	
		unsigned int 							_maxClientBodySize;
		std::vector<LocationBlock>				_locationBlocks;

		std::vector<std::string>				_rawBlock;
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
		void printLocationBlock(LocationBlock loc, int level) const;

		std::vector<std::string> getGlobalMethods() const;
		std::string getGlobalCgiPathPHP() const;
		std::string getGlobalCgiPathPython() const;
		std::map<int, std::string> getErrorPages() const;
		std::string getHost() const;
		std::string getPort() const;
		std::string getServerNames() const;
		std::string getIndex() const;
		unsigned int getMaxClientBodySize() const;
		std::vector<LocationBlock>& getLocationBlocks();
	};
