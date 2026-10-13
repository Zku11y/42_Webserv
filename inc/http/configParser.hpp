#pragma once

#include <string>
#include <map>
#include <vector>

class Config;
class ServerConfig;
class LocationConfig;

class ConfigParser
{
	private:
	typedef void (ConfigParser::*LocationHandler)(LocationConfig&);
	typedef void (ConfigParser::*ServerHandler)(ServerConfig&);

		std::string					_filename;
		std::string					_content;
		std::vector<std::string>	_tokens;
		size_t						_pos;

		std::map<std::string, LocationHandler> _locationHandlers;
		std::map<std::string, ServerHandler> _serverHandlers;
		
		void		readFile();
		void		tokenize();

		void		parseConfig(Config& config);
		void		parseServer(Config& config);
		void		parseLocation(ServerConfig& server);

		void		parseServerDirective(ServerConfig& server);
		void		parseLocationDirective(LocationConfig& location);

		size_t		parseSize(const std::string& value);
		int			strtoi(const std::string& value);
		void		parseListen(ServerConfig& server);
		void		parseHost(ServerConfig& server);
		void		parseServerName(ServerConfig& server);
		void		parseClientMaxBodySize(ServerConfig& server);
		void		parseErrorPage(ServerConfig& server);
		
		void		parseMethods(LocationConfig& location);
		void		parseRoot(LocationConfig& location);
		void		parseIndex(LocationConfig& location);
		void		parseAutoindex(LocationConfig& location);
		void		parseUploadEnable(LocationConfig& location);
		void		parseUploadStore(LocationConfig& location);
		void		parseReturn(LocationConfig& location);
		void		parseCgiExt(LocationConfig& location);


		bool		hasMoreTokens() const;
		std::string	currentToken() const;
		void		advance();
		void		expect(const std::string& token);

	public:
		ConfigParser(const std::string& filename);
		~ConfigParser();

		void	parse(Config& config);
};
